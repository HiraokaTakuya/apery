/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2018 Hiraoka Takuya

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

#ifndef APERY_MOVEPICKER_HPP
#define APERY_MOVEPICKER_HPP

#include "move.hpp"
#include "position.hpp"
#include "search.hpp"

enum Stages {
    MainSearch, TacticalInit, GoodTacticals, Killers, Countermove, QuietInit, Quiet, BadCaptures,
    EvasionSearch, EvasionsInit, AllEvasions,
    Probcut, ProbcutInit, ProbcutCaptures,
#if defined USE_QCHECKS
    QSearchWithChecks, QCaptures1Init, QCaptures1, QChecks,
#endif
    QSearchNoChecks, QCaptures2Init, QCaptures2,
    QSearchRecaptures, QRecaptures
};
OverloadEnumOperators(Stages);

class MovePicker {
public:
    MovePicker(const MovePicker&) = delete;
    MovePicker& operator = (const MovePicker&) = delete;

    MovePicker(const Position& pos, const Move ttm, const Score th);
    MovePicker(const Position& pos, const Move ttm, const Depth depth, const Square sq);
    MovePicker(const Position& pos, const Move ttm, const Depth depth, SearchStack* searchStack);

    Move nextMove();

private:
    void scoreCaptures();
    template <bool IsDrop> void scoreNonCapturesMinusPro();
    void scoreEvasions();
    ExtMove* begin() { return cur_; }
    ExtMove* end() { return endMoves_; }
    ExtMove* first() { return &moves_[1]; } // 番兵を除いている。

    const Position& pos_;
    const SearchStack* ss_;
    Move counterMove_;
    Depth depth_;
    Move ttMove_;
    Square recaptureSquare_;
    Score threshold_;
    Stages stage_;
    ExtMove* cur_;
    ExtMove* endMoves_;
    ExtMove* endBadCaptures_;
    ExtMove moves_[MaxLegalMoves];
};

#endif // #ifndef APERY_MOVEPICKER_HPP
