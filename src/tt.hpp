/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2016 Hiraoka Takuya

  Apery is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Apery is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef APERY_TT_HPP
#define APERY_TT_HPP

#include "common.hpp"
#include "move.hpp"

enum Depth {
    OnePly           = 1,
    Depth0           = 0,
    Depth1           = 1,
    DepthQChecks     = 0 * OnePly,
    DepthQNoChecks   = -1 * OnePly,
    DepthQRecaptures = -5 * OnePly,

    DepthNone        = -6 * OnePly,
    DepthMax         = MaxPly * OnePly,
};
OverloadEnumOperators(Depth);
static_assert(!(OnePly & (OnePly - 1)), "OnePly is not a power of 2");

class TTEntry {
public:
    u16   key() const        { return key16_; }
    Move  move() const       { return static_cast<Move>(move16_); }
    Score score() const      { return static_cast<Score>(score16_); }
    Score evalScore() const  { return static_cast<Score>(eval16_); }
    Depth depth() const      { return static_cast<Depth>(depth8_); }
    Bound bound() const      { return static_cast<Bound>(genBound8_ & 0x3); }
    u8    generation() const { return genBound8_ & 0xfc; }

    void save(const Key posKey, const Score score, const Bound bound, const Depth depth,
              const Move move, const Score evalScore, const u8 generation)
    {
        assert(depth / OnePly * OnePly == depth);
        if (move || (posKey>>48) != key16_)
            move16_ = static_cast<u16>(move.value());

        if ((posKey>>48) != key16_
            || depth / OnePly > depth8_ - 4
            || bound == BoundExact)
        {
            key16_     = static_cast<u16>(posKey>>48);
            score16_   = static_cast<s16>(score);
            eval16_    = static_cast<s16>(evalScore);
            genBound8_ = static_cast<u8 >(generation | bound);
            depth8_    = static_cast<s8 >(depth / OnePly);
        }
    }

private:
    friend class TranspositionTable;

    u16 key16_;
    u16 move16_;
    s16 score16_;
    s16 eval16_;
    u8 genBound8_;
    s8 depth8_;
};

const int ClusterSize = 3;

struct TTCluster {
    TTEntry entry[ClusterSize];
    s8 padding[2];
};

class TranspositionTable {
public:
    TranspositionTable() : clusterCount_(0), table_(nullptr), mem_(nullptr), generation_(0) {}
    ~TranspositionTable() { free(mem_); }
    void newSearch() { generation_ += 4; } // TTEntry::genBound8_ の Bound の部分を書き換えないように。
    u8 generation() const { return generation_; }
    TTEntry* probe(const Key posKey, bool& found) const;
    void resize(const size_t mbSize); // Mega Byte 指定
    void clear();
    TTEntry* firstEntry(const Key posKey) const {
        // (clusterCount_ - 1) は置換表で使用するバイト数のマスク
        // posKey の下位 (clusterCount_ - 1) ビットを hash key として使用。
        // ここで posKey の下位ビットの一致を確認。
        // posKey の上位16ビットとの一致は probe 内で確認する。
        return &table_[(size_t)posKey & (clusterCount_ - 1)].entry[0];
    }

private:
    TranspositionTable(const TranspositionTable&);
    TranspositionTable& operator = (const TranspositionTable&);

    size_t clusterCount_;
    TTCluster* table_;
    void* mem_;
    // iterative deepening していくとき、過去の探索で調べたものかを判定する。
    u8 generation_;
};

#endif // #ifndef APERY_TT_HPP
