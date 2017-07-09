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

#ifndef APERY_LEARNER_HPP
#define APERY_LEARNER_HPP

#include "position.hpp"
#include "thread.hpp"
#include "evaluate.hpp"

#if defined LEARN

#if 0
#define PRINT_PV(x) x
#else
#define PRINT_PV(x)
#endif

inline void printEvalTable(const Square ksq, const int p0, const int p1_base, const bool isTurn) {
    for (Rank r = Rank1; r < RankNum; ++r) {
        for (File f = File9; File1 <= f; --f) {
            const Square sq = makeSquare(f, r);
            printf("%5d", Evaluator::KPP[ksq][p0][p1_base + sq][isTurn]);
        }
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
}

#endif

#endif // #ifndef APERY_LEARNER_HPP
