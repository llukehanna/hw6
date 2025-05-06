#ifndef HT_H
#define HT_H

#include <vector>
#include <iostream>
#include <cmath>
#include <stdexcept>

typedef size_t HASH_INDEX_T;

// ----------------------------------------------------------------------------
//                               Probers
// ----------------------------------------------------------------------------

template <typename KeyType>
struct Prober {
    HASH_INDEX_T start_;      // h1(k)
    HASH_INDEX_T m_;          // table size
    size_t       numProbes_;  // how many calls to next()
    static const HASH_INDEX_T npos = (HASH_INDEX_T)-1;

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        (void)key;
        start_    = start;
        m_        = m;
        numProbes_= 0;
    }

    HASH_INDEX_T next() {
        throw std::logic_error("Prober::next() must be overridden");
    }
};

template <typename KeyType>
struct LinearProber : public Prober<KeyType> {
    HASH_INDEX_T next() {
        // fail after we've tried m slots
        if (this->numProbes_ >= this->m_) {
            return this->npos;
        }
        HASH_INDEX_T loc = (this->start_ + this->numProbes_) % this->m_;
        this->numProbes_++;
        return loc;
    }
};

template <typename KeyType, typename Hash2>
struct DoubleHashProber : public Prober<KeyType> {
    Hash2          h2_;
    HASH_INDEX_T   dhstep_;

    static const HASH_INDEX_T DOUBLE_HASH_MOD_VALUES[];
    static const int        DOUBLE_HASH_MOD_SIZE;

    DoubleHashProber(const Hash2& h2 = Hash2())
      : h2_(h2)
    {}

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        Prober<KeyType>::init(start, m, key);
        // pick the largest modulus < m
        HASH_INDEX_T mod = DOUBLE_HASH_MOD_VALUES[0];
        for (int i = 0; i < DOUBLE_HASH_MOD_SIZE && DOUBLE_HASH_MOD_VALUES[i] < m; ++i) {
            mod = DOUBLE_HASH_MOD_VALUES[i];
        }
        dhstep_ = mod - (h2_(key) % mod);
    }

    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_) {
            return this->npos;
        }
        // double‐hash: h1 + i * dhstep
        HASH_INDEX_T loc = (this->start_ + this->numProbes_ * dhstep_) % this->m_;
        this->numProbes_++;
        return loc;
    }
};

// static array init
template<typename K, typename H2>
const HASH_INDEX_T DoubleHashProber<K,H2>::DOUBLE_HASH_MOD_VALUES[] = {
    7, 19, 43, 89, 193, 389, 787, 1583, 3191, 6397,
    12841, 25703, 51431, 102871, 205721, 411503,
    823051, 1646221, 3292463, 6584957, 13169963,
    26339921, 52679927, 105359939, 210719881,
    421439749, 842879563, 1685759113
};
template<typename K, typename H2>
const int DoubleHashProber<K,H2>::DOUBLE_HASH_MOD_SIZE =
    sizeof(DoubleHashProber<K,H2>::DOUBLE_HASH_MOD_VALUES)
    / sizeof(HASH_INDEX_T);

// ----------------------------------------------------------------------------
//                              HashTable
// ----------------------------------------------------------------------------

template<
    typename K,
    typename V,
    typename Prober    = LinearProber<K>,
    typename Hash      = std::hash<K>,
    typename KEqual    = std::equal_to<K>
>
class HashTable {
public:
    typedef K                        KeyType;
    typedef V                        ValueType;
    typedef std::pair<KeyType,ValueType> ItemType;
    typedef Hash                     Hasher;

    struct HashItem {
        ItemType item;
        bool     deleted;
        HashItem(const ItemType& p)
          : item(p)
          , deleted(false)
        {}
    };

    // ctor/dtor
    HashTable(double resizeAlpha = 0.4,
              const Prober&  prober      = Prober(),
              const Hasher&  hash        = Hasher(),
              const KEqual&  kequal      = KEqual())
      : resizeAlpha_(resizeAlpha)
      , prober_(prober)
      , hash_(hash)
      , kequal_(kequal)
      , totalProbes_(0)
      , mIndex_(0)
      , count_(0)
      , used_(0)
    {
        table_.assign(CAPACITIES[mIndex_], nullptr);
    }

    ~HashTable() {
        for (auto p : table_)
            delete p;
    }

    bool empty() const {
        return count_ == 0;
    }

    size_t size() const {
        return count_;
    }

    void insert(const ItemType& p) {
        // resize if load factor ≥ α
        if (double(used_ + 1) / table_.size() >= resizeAlpha_) {
            resize();
        }

        HASH_INDEX_T loc = probe(p.first);
        if (loc == npos) {
            throw std::logic_error("HashTable full, cannot insert");
        }

        if (table_[loc] == nullptr) {
            // brand‐new slot
            table_[loc] = new HashItem(p);
            ++count_;
            ++used_;
        }
        else {
            // overwrite existing key
            table_[loc]->item.second = p.second;
        }
    }

    void remove(const KeyType& key) {
        auto p = internalFind(key);
        if (p) {
            p->deleted = true;
            --count_;
            // note: 'used_' stays the same, so deleted slots count toward α
        }
    }

    ItemType const* find(const KeyType& key) const {
        HASH_INDEX_T loc = probe(key);
        if (loc == npos || table_[loc] == nullptr) return nullptr;
        return &table_[loc]->item;
    }
    ItemType* find(const KeyType& key) {
        HASH_INDEX_T loc = probe(key);
        if (loc == npos || table_[loc] == nullptr) return nullptr;
        return &table_[loc]->item;
    }

    const ValueType& at(const KeyType& key) const {
        auto p = internalFind(key);
        if (!p) throw std::out_of_range("Key not found");
        return p->item.second;
    }
    ValueType& at(const KeyType& key) {
        auto p = internalFind(key);
        if (!p) throw std::out_of_range("Key not found");
        return p->item.second;
    }

    const ValueType& operator[](const KeyType& key) const {
        return at(key);
    }
    ValueType& operator[](const KeyType& key) {
        return at(key);
    }

    void reportAll(std::ostream& out) const {
        for (HASH_INDEX_T i = 0; i < table_.size(); ++i) {
            if (table_[i]) {
                out << "Bucket " << i << ": "
                    << table_[i]->item.first << " → "
                    << table_[i]->item.second << "\n";
            }
        }
    }

    void clearTotalProbes() { totalProbes_ = 0; }
    size_t totalProbes() const { return totalProbes_; }

private:
    static const HASH_INDEX_T CAPACITIES[];

    // helper to find a key's slot (either where it lives, or first nullptr)
    HASH_INDEX_T probe(const KeyType& key) const {
        HASH_INDEX_T h0 = hash_(key) % CAPACITIES[mIndex_];
        prober_.init(h0, CAPACITIES[mIndex_], key);

        HASH_INDEX_T loc = prober_.next();
        ++totalProbes_;
        while (loc != Prober::npos) {
            if (table_[loc] == nullptr) {
                // empty slot → can insert here
                return loc;
            }
            else if (!table_[loc]->deleted
                     && kequal_(table_[loc]->item.first, key))
            {
                // found the key
                return loc;
            }
            loc = prober_.next();
            ++totalProbes_;
        }
        return npos;
    }

    HashItem* internalFind(const KeyType& key) const {
        HASH_INDEX_T loc = probe(key);
        if (loc == npos || table_[loc] == nullptr) return nullptr;
        return table_[loc];
    }

    void resize() {
        // bump to next capacity
        size_t capCount = sizeof(CAPACITIES)/sizeof(HASH_INDEX_T);
        if (mIndex_ + 1 >= capCount) {
            throw std::logic_error("No more prime capacities");
        }
        size_t oldSize = table_.size();
        auto   oldTab  = table_;

        ++mIndex_;
        table_.assign(CAPACITIES[mIndex_], nullptr);
        count_ = 0;
        used_  = 0;

        // rehash only non-deleted items
        for (auto pItem : oldTab) {
            if (pItem && !pItem->deleted) {
                insert(pItem->item);
            }
            delete pItem;
        }
    }

    // data
    std::vector<HashItem*> table_;
    Prober                  prober_;
    Hasher                  hash_;
    KEqual                  kequal_;

    double                  resizeAlpha_;
    size_t                  totalProbes_;

    // mIndex_ chooses CAPACITIES[mIndex_], plus size bookkeeping:
    size_t                  mIndex_;
    size_t                  count_;   // active (non-deleted) items
    size_t                  used_;    // total non-null (incl. deleted)

    static const HASH_INDEX_T npos = Prober::npos;
};

// static primes
template<typename K,typename V,typename P,typename H,typename E>
const HASH_INDEX_T HashTable<K,V,P,H,E>::CAPACITIES[] = {
    11, 23, 47, 97, 197, 397, 797, 1597,
    3203, 6421, 12853, 25717, 51437, 102877,
    205759, 411527, 823117, 1646237, 3292489,
    6584983, 13169977, 26339969, 52679969,
    105359969, 210719881, 421439783,
    842879579, 1685759167
};

#endif
