#ifndef RECCHECK
#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <random>
#include <iomanip>
#include <fstream>
#include <exception>
#endif

#include "boggle.h"

std::vector<std::vector<char> > genBoard(unsigned int n, int seed)
{
    // random number generator
    std::mt19937 r(seed);

    // scrabble letter frequencies
    int freq[26] = {9,2,2,4,12,2,3,2,9,1,1,4,2,6,8,2,1,6,4,6,4,2,2,1,2,1};
    std::vector<char> letters;
    for(char c='A'; c<='Z'; c++)
        for(int i=0; i<freq[c-'A']; i++)
            letters.push_back(c);

    std::vector<std::vector<char>> board(n, std::vector<char>(n));
    for(unsigned i=0; i<n; i++)
        for(unsigned j=0; j<n; j++)
            board[i][j] = letters[r() % letters.size()];

    return board;
}

void printBoard(const std::vector<std::vector<char>>& board)
{
    unsigned n = board.size();
    for(unsigned i=0; i<n; i++){
        for(unsigned j=0; j<n; j++)
            std::cout << std::setw(2) << board[i][j];
        std::cout << "\n";
    }
}

std::pair<std::set<std::string>, std::set<std::string>> parseDict(std::string fname)
{
    std::ifstream dictfs(fname);
    if(dictfs.fail())
        throw std::invalid_argument("unable to open dictionary file");

    std::set<std::string> dict, prefix;
    std::string word;
    while(dictfs >> word){
        dict.insert(word);
        // all proper prefixes of word
        for(size_t i = word.size(); i > 1; --i)
            prefix.insert(word.substr(0, i-1));
    }
    prefix.insert("");  // allow empty prefix
    return {dict, prefix};
}

std::set<std::string> boggle(
    const std::set<std::string>& dict,
    const std::set<std::string>& prefix,
    const std::vector<std::vector<char>>& board)
{
    std::set<std::string> result;
    unsigned n = board.size();
    for(unsigned i=0; i<n; i++){
        for(unsigned j=0; j<n; j++){
            boggleHelper(dict, prefix, board, "", result, i, j, 0, 1);
            boggleHelper(dict, prefix, board, "", result, i, j, 1, 0);
            boggleHelper(dict, prefix, board, "", result, i, j, 1, 1);
        }
    }
    return result;
}

// Returns true if this call inserted a word (so parent knows not to insert its shorter prefix)
bool boggleHelper(
    const std::set<std::string>& dict,
    const std::set<std::string>& prefix,
    const std::vector<std::vector<char>>& board,
    std::string word,
    std::set<std::string>& result,
    unsigned r, unsigned c,
    int dr, int dc)
{
    unsigned n = board.size();
    // out of bounds?
    if(r >= n || c >= n) 
        return false;

    // extend current string
    word.push_back(board[r][c]);

    bool foundLonger = false;
    // if we can still build a longer word, recurse
    if(prefix.find(word) != prefix.end()){
        foundLonger = boggleHelper(dict, prefix, board, word, result, r + dr, c + dc, dr, dc);
    }

    // if no longer word was found down this path, and 'word' is in dict, insert it
    if(!foundLonger && dict.find(word) != dict.end()){
        result.insert(word);
        return true;
    }

    return foundLonger;
}
