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

#include "common.hpp"
#include "piece.hpp"
#include "hand.hpp"

const int Hand::HandPieceShiftBits[HandPieceNum] = {
    HPawnShiftBits,
    HLanceShiftBits,
    HKnightShiftBits,
    HSilverShiftBits,
    HGoldShiftBits,
    HBishopShiftBits,
    HRookShiftBits
};
const u32 Hand::HandPieceMask[HandPieceNum] = {
    HPawnMask,
    HLanceMask,
    HKnightMask,
    HSilverMask,
    HGoldMask,
    HBishopMask,
    HRookMask
};
// 特定の種類の持ち駒を 1 つ増やしたり減らしたりするときに使用するテーブル
const u32 Hand::HandPieceOne[HandPieceNum] = {
    1 << HPawnShiftBits,
    1 << HLanceShiftBits,
    1 << HKnightShiftBits,
    1 << HSilverShiftBits,
    1 << HGoldShiftBits,
    1 << HBishopShiftBits,
    1 << HRookShiftBits
};
