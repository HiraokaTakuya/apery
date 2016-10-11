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

#ifndef APERY_SEARCH_HPP
#define APERY_SEARCH_HPP

#include "move.hpp"
#include "pieceScore.hpp"
#include "timeManager.hpp"
#include "tt.hpp"
#include "thread.hpp"

class Position;
struct SplitPoint;

struct SearchStack {
    Move* pv;
    Ply ply;
    Move currentMove;
    Move excludedMove;
    Move killers[2];
    Score staticEval;
    bool skipEarlyPruning;
    int moveCount;
    CounterMoveStats* counterMoves;
    EvalSum staticEvalRaw; // 評価関数の差分計算用、値が入っていないときは [0] を ScoreNotEvaluated にしておく。
                           // 常に Black 側から見た評価値を入れておく。
                           // 0: 先手玉に対する評価値, 1: 後手玉に対する評価値, 2: 双玉に対する評価値
};

struct SignalsType {
    std::atomic_bool stop;
    std::atomic_bool stopOnPonderHit;
};

enum InaniwaFlag {
    NotInaniwa,
    InaniwaIsBlack,
    InaniwaIsWhite,
    InaniwaFlagNum
};

enum BishopInDangerFlag {
    NotBishopInDanger,
    BlackBishopInDangerIn28,
    WhiteBishopInDangerIn28,
    BlackBishopInDangerIn78,
    WhiteBishopInDangerIn78,
    BlackBishopInDangerIn38,
    WhiteBishopInDangerIn38,
    BishopInDangerFlagNum
};

struct EasyMoveManager {
    void clear() {
        stableCount = 0;
        expectedPosKey = 0;
        pv[0] = pv[1] = pv[2] = Move::moveNone();
    }

    Move get(Key key) const {
        return expectedPosKey == key ? pv[2] : Move::moveNone();
    }

    void update(Position& pos, const std::vector<Move>& newPv) {
        assert(newPv.size() >= 3);
        stableCount = (newPv[2] == pv[2]) ? stableCount + 1 : 0;
        if (!std::equal(std::begin(newPv), std::begin(newPv) + 3, pv)) {
            std::copy(std::begin(newPv), std::begin(newPv) + 3, pv);
            StateInfo st[2];
            pos.doMove(newPv[0], st[0]);
            pos.doMove(newPv[1], st[1]);
            expectedPosKey = pos.getKey();
            pos.undoMove(newPv[1]);
            pos.undoMove(newPv[0]);
        }
    }

    int stableCount;
    Key expectedPosKey;
    Move pv[3];
};

class TranspositionTable;

struct Searcher {
    // static メンバ関数からだとthis呼べないので代わりに thisptr を使う。
    // static じゃないときは this を入れることにする。
    STATIC Searcher* thisptr;
    STATIC SignalsType signals;
    STATIC LimitsType limits;
    STATIC StateListPtr states;

#if defined LEARN
    STATIC Score alpha;
    STATIC Score beta;
#endif

    STATIC TimeManager timeManager;
    STATIC TranspositionTable tt;

#if defined INANIWA_SHIFT
    STATIC InaniwaFlag inaniwaFlag;
#endif
    STATIC ThreadPool threads;
    STATIC OptionsMap options;
    STATIC EasyMoveManager easyMove;

    STATIC void init();
    STATIC void clear();
    template <NodeType NT, bool INCHECK>
    STATIC Score qsearch(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth);
#if defined INANIWA_SHIFT
    STATIC void detectInaniwa(const Position& pos);
#endif
    template <NodeType NT>
    STATIC Score search(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth, const bool cutNode);
    STATIC void think();
    STATIC void checkTime();

    STATIC void doUSICommandLoop(int argc, char* argv[]);
    STATIC void setOption(std::istringstream& ssCmd);
};

void initSearchTable();

#endif // #ifndef APERY_SEARCH_HPP
