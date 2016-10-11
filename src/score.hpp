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

#ifndef APERY_SCORE_HPP
#define APERY_SCORE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"

using Ply = int;

const Ply MaxPly = 128;

enum Bound {
    BoundNone  = 0,
    BoundUpper = Binary< 1>::value, // fail low  で正しい score が分からない。alpha 以下が確定という意味。
    BoundLower = Binary<10>::value, // fail high で正しい score が分からない。beta 以上が確定という意味。
    BoundExact = Binary<11>::value  // alpha と beta の間に score がある。
};

inline bool exactOrLower(const Bound st) {
    return (st & BoundLower ? true : false);
}
inline bool exactOrUpper(const Bound st) {
    return (st & BoundUpper ? true : false);
}

// 評価値
enum Score {
    ScoreZero          = 0,
    ScoreDraw          = 0,
    ScoreMaxEvaluate   = 30000,
    ScoreMateLong      = 30002,
    ScoreMate1Ply      = 32599,
    ScoreMate0Ply      = 32600,
    ScoreMateInMaxPly  = ScoreMate0Ply - MaxPly,
    ScoreMatedInMaxPly = -ScoreMateInMaxPly,
    ScoreInfinite      = 32601,
    ScoreNotEvaluated  = INT_MAX,
    ScoreNone          = 32602
};
OverloadEnumOperators(Score);

inline Score mateIn(const Ply ply) {
    return ScoreMate0Ply - static_cast<Score>(ply);
}
inline Score matedIn(const Ply ply) {
    return -ScoreMate0Ply + static_cast<Score>(ply);
}

#endif // #ifndef APERY_SCORE_HPP
