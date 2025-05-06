#ifndef HT_H
#define HT_H

#include <vector>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <functional>

typedef std::size_t HASH_INDEX_T;

// ------------------------------ Probers ------------------------------------

template <typename KeyType>
struct Prober {
    static const HASH_INDEX_T npos = static_cast<HASH_INDEX_T>(-1);
    HASH_INDEX_T start_, m_;
    size_t      numProbes_;

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType&) {
        start_ = start;
        m_ = m;
        numProbes_ = 0;
    }

    HASH_INDEX_T next() {
        throw std::logic_error("Prober::next must be overridden");
    }
};

template <typename KeyType>
struct LinearProber : public Prober<KeyType> {
    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_) return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_) % this->m_;
        ++this->numProbes_;
        return loc;
    }
};

template <typename KeyType, typename Hash2>
struct DoubleHashProber : public Prober<KeyType> {
    Hash2        h2_;
    HASH_INDEX_T step_;
    static const HASH_INDEX_T modVals[];
    static const int modCount;

    DoubleHashProber(const Hash2& h2 = Hash2()) : h2_(h2) {}

    void init(HASH_INDEX_T start, HASH_INDEX_T m, const KeyType& key) {
        Prober<KeyType>::init(start, m, key);
        HASH_INDEX_T mod = modVals[0];
        for (int i = 0; i < modCount && modVals[i] < m; ++i) mod = modVals[i];
        step_ = mod - (h2_(key) % mod);
    }

    HASH_INDEX_T next() {
        if (this->numProbes_ >= this->m_) return this->npos;
        HASH_INDEX_T loc = (this->start_ + this->numProbes_ * step_) % this->m_;
        ++this->numProbes_;
        return loc;
    }
};

template<typename K, typename H2>
const HASH_INDEX_T DoubleHashProber<K,H2>::modVals[] = {
    7,19,43,89,193,389,787,1583,3191,6397,12841,25703,
    51431,102871,205721,411503,823051,1646221,3292463,6584957,
    13169963,26339921,52679927,105359939,210719881,421439783,
    842879563,1685759113
};

template<typename K, typename H2>
const int DoubleHashProber<K,H2>::modCount = sizeof(DoubleHashProber<K,H2>::modVals)/sizeof(HASH_INDEX_T);

// ---------------------------- HashTable ------------------------------------

template<
    typename K,
    typename V,
    typename ProberType = LinearProber<K>,
    typename Hash       = std::hash<K>,
    typename KeyEqual   = std::equal_to<K>
>
class HashTable {
public:
    using KeyType   = K;
    using ValueType = V;
    using ItemType  = std::pair<KeyType,ValueType>;
    using Hasher    = Hash;

    struct HashItem {
        ItemType item;
        bool     deleted;
        HashItem(const ItemType& p) : item(p), deleted(false) {}
    };

    HashTable(double alpha = 0.4,
              const ProberType& prober = ProberType(),
              const Hasher& hash     = Hasher(),
              const KeyEqual& eq     = KeyEqual())
      : prober_(prober)
      , hash_(hash)
      , eq_(eq)
      , alpha_(alpha)
      , totalProbes_(0)
      , index_(0)
      , count_(0)
      , used_(0)
    {
        table_.assign(sizes[index_], nullptr);
    }

    ~HashTable() {
        for (auto p : table_) delete p;
    }

    bool empty() const { return count_ == 0; }
    size_t size()  const { return count_; }

    void insert(const ItemType& p) {
        // Resize if current loading factor reaches threshold
        if (double(used_) / table_.size() >= alpha_)
            resize();
        HASH_INDEX_T loc = probe(p.first);
        if (loc == npos)
            throw std::logic_error("HashTable full");
        if (!table_[loc]) {
            table_[loc] = new HashItem(p);
            ++count_; ++used_;
        } else {
            table_[loc]->item.second = p.second;
        }
    } else {
            table_[loc]->item.second = p.second;
        }
    }

    void remove(const KeyType& key) {
        auto p = internalFind(key);
        if (p && !p->deleted) { p->deleted = true; --count_; }
    }

    const ValueType& at(const KeyType& key) const {
        auto p = internalFind(key);
        if (!p) throw std::out_of_range("Bad key");
        return p->item.second;
    }
    ValueType& at(const KeyType& key) {
        auto p = internalFind(key);
        if (!p) throw std::out_of_range("Bad key");
        return p->item.second;
    }

    // operator[] overloads
    ValueType& operator[](const KeyType& key) {
        return at(key);
    }
    const ValueType& operator[](const KeyType& key) const {
        return at(key);
    }

    ItemType* find(const KeyType& key) {
        auto p = internalFind(key);
        return p ? &p->item : nullptr;
    }
    const ItemType* find(const KeyType& key) const {
        auto p = internalFind(key);
        return p ? &p->item : nullptr;
    }

    void reportAll(std::ostream& out) const {
        for (size_t i = 0; i < table_.size(); ++i) {
            if (table_[i] && !table_[i]->deleted)
                out << "Bucket " << i << ": "
                    << table_[i]->item.first << " -> "
                    << table_[i]->item.second << "\n";
        }
    }

    void clearTotalProbes() { totalProbes_ = 0; }
    size_t totalProbes() const { return totalProbes_; }

private:
    static const HASH_INDEX_T sizes[];
    static const HASH_INDEX_T npos = ProberType::npos;

    HASH_INDEX_T probe(const KeyType& key) const {
        HASH_INDEX_T h0 = hash_(key) % sizes[index_];
        prober_.init(h0, sizes[index_], key);
        for (size_t i = 0; i < sizes[index_]; ++i) {
            HASH_INDEX_T loc = prober_.next();
            ++totalProbes_;
            if (loc == npos) return npos;
            auto pi = table_[loc];
            if (!pi || (!pi->deleted && eq_(pi->item.first, key)))
                return loc;
        }
        return npos;
    }

    HashItem* internalFind(const KeyType& key) const {
        HASH_INDEX_T loc = probe(key);
        if (loc == npos) return nullptr;
        auto p = table_[loc];
        return (p && !p->deleted) ? p : nullptr;
    }

    void resize() {
        if (index_ + 1 >= sizeof(sizes)/sizeof(HASH_INDEX_T))
            throw std::logic_error("No more capacities");
        auto old = table_;
        ++index_;
        table_.assign(sizes[index_], nullptr);
        count_ = used_ = 0;
        for (auto p : old) {
            if (p && !p->deleted) insert(p->item);
            delete p;
        }
    }

    mutable ProberType         prober_;
    Hasher                     hash_;
    KeyEqual                   eq_;
    double                     alpha_;
    mutable size_t             totalProbes_;
    size_t                     index_, count_, used_;
    std::vector<HashItem*>     table_;
};

template<typename K,typename V,typename P,typename H,typename E>
const HASH_INDEX_T HashTable<K,V,P,H,E>::sizes[] = {
    11,23,47,97,197,397,797,1597,
    3203,6421,12853,25717,51437,102877,
    205759,411527,823117,1646237,3292489,
    6584983,13169977,26339969,52679969,
    105359969,210719881,421439783,
    842879579,1685759167
};

#endif // HT_H
