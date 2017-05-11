/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2017 Hiraoka Takuya

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

#ifndef APERY_THREAD_HPP
#define APERY_THREAD_HPP

#include "common.hpp"
#include "evaluate.hpp"
#include "usi.hpp"
#include "tt.hpp"

const int MaxThreads = 256;

enum NodeType {
    NonPV, PV
};

// 時間や探索深さの制限を格納する為の構造体
struct LimitsType {
    LimitsType() {
        nodes = time[Black] = time[White] = inc[Black] = inc[White] = movesToGo = depth = moveTime = mate = infinite = ponder = 0;
    }
    bool useTimeManagement() const { return !(mate | moveTime | depth | nodes | infinite); }

    std::vector<Move> searchmoves;
    int time[ColorNum], inc[ColorNum], movesToGo, depth, moveTime, mate, infinite, ponder;
    s64 nodes;
    Timer startTime;
};

template <typename T, bool CM = false>
struct Stats {
    static const Score Max = Score(1 << 28);

    const T* operator [](const Piece pc) const { return table[pc]; }
    T* operator [](const Piece pc) { return table[pc]; }
    void clear() { std::memset(table, 0, sizeof(table)); }
    void update(const Piece pc, const Square to, const Move m) { table[pc][to] = m; }
    void update(const Piece pc, const Square to, const Score s) {
        if (abs(int(s)) >= 324)
            return;
        table[pc][to] -= table[pc][to] * abs(int(s)) / (CM ? 936 : 324);
        table[pc][to] += int(s) * 32;
    }

private:
    T table[PieceNone][SquareNum];
};

using MoveStats               = Stats<Move>;
using HistoryStats            = Stats<Score, false>;
using CounterMoveStats        = Stats<Score, true >;
using CounterMoveHistoryStats = Stats<CounterMoveStats>;

struct FromToStats {
    Score get(const Color c, const Move m) const { return table[c][m.from()][m.to()]; }
    void clear() { std::memset(table, 0, sizeof(table)); }
    void update(const Color c, const Move m, const Score s) {
        if (abs(int(s)) >= 324)
            return;
        const Square from = m.from();
        const Square to = m.to();

        table[c][from][to] -= table[c][from][to] * abs(int(s)) / 324;
        table[c][from][to] += int(s) * 32;
    }

private:
    Score table[ColorNum][(Square)PieceTypeNum + SquareNum][SquareNum]; // from は駒打ちも含めるので、その分のサイズをとる。
};

struct RootMove {
    explicit RootMove(const Move m) : pv(1, m) {}
    bool operator < (const RootMove& m) const { return m.score < score; } // Descending sort
    bool operator == (const Move& m) const { return pv[0] == m; }
    bool extractPonderFromTT(Position& pos);
    void extractPVFromTT(Position& pos);

    Score score = -ScoreInfinite;
    Score previousScore = -ScoreInfinite;
    std::vector<Move> pv;
};

struct Thread {
    explicit Thread(Searcher* s);
    virtual ~Thread();
    virtual void search();
    void idleLoop();
    void startSearching(const bool resume = false);
    void waitForSearchFinished();
    void wait(std::atomic_bool& condition);

    Searcher* searcher;
    size_t idx;
    size_t pvIdx;
    int maxPly;
    int callsCnt;

    Position rootPos;
    std::vector<RootMove> rootMoves;
    Depth rootDepth;
    Depth completedDepth;
    std::atomic_bool resetCalls;
    HistoryStats history;
    MoveStats counterMoves;
    FromToStats fromTo;
    CounterMoveHistoryStats counterMoveHistory;

private:
    std::thread nativeThread;
    Mutex mutex;
    ConditionVariable sleepCondition;
    bool exit;
    bool searching;
};

struct MainThread : public Thread {
    explicit MainThread(Searcher* s) : Thread(s) {}
    virtual void search();

    bool easyMovePlayed;
    bool failedLow;
    double bestMoveChanges;
    Score previousScore;
};

struct ThreadPool : public std::vector<Thread*> {
    void init(Searcher* s);
    void exit();

    MainThread* main() { return static_cast<MainThread*>((*this)[0]); }
    void startThinking(const Position& pos, const LimitsType& limits, StateListPtr& states);
    void readUSIOptions(Searcher* s);
    s64 nodesSearched() const;

private:
    StateListPtr setupStates;
};

#endif // #ifndef APERY_THREAD_HPP
