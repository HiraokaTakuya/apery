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

#ifndef APERY_PIECESCORE_HPP
#define APERY_PIECESCORE_HPP

#include "score.hpp"
#include "piece.hpp"

const Score PawnScore             = static_cast<Score>( 100 * 9 / 10);
const Score LanceScore            = static_cast<Score>( 350 * 9 / 10);
const Score KnightScore           = static_cast<Score>( 450 * 9 / 10);
const Score SilverScore           = static_cast<Score>( 550 * 9 / 10);
const Score GoldScore             = static_cast<Score>( 600 * 9 / 10);
const Score BishopScore           = static_cast<Score>( 950 * 9 / 10);
const Score RookScore             = static_cast<Score>(1100 * 9 / 10);
const Score ProPawnScore          = static_cast<Score>( 600 * 9 / 10);
const Score ProLanceScore         = static_cast<Score>( 600 * 9 / 10);
const Score ProKnightScore        = static_cast<Score>( 600 * 9 / 10);
const Score ProSilverScore        = static_cast<Score>( 600 * 9 / 10);
const Score HorseScore            = static_cast<Score>(1050 * 9 / 10);
const Score DragonScore           = static_cast<Score>(1550 * 9 / 10);

const Score KingScore             = static_cast<Score>(15000);

const Score CapturePawnScore      = PawnScore      * 2;
const Score CaptureLanceScore     = LanceScore     * 2;
const Score CaptureKnightScore    = KnightScore    * 2;
const Score CaptureSilverScore    = SilverScore    * 2;
const Score CaptureGoldScore      = GoldScore      * 2;
const Score CaptureBishopScore    = BishopScore    * 2;
const Score CaptureRookScore      = RookScore      * 2;
const Score CaptureProPawnScore   = ProPawnScore   + PawnScore;
const Score CaptureProLanceScore  = ProLanceScore  + LanceScore;
const Score CaptureProKnightScore = ProKnightScore + KnightScore;
const Score CaptureProSilverScore = ProSilverScore + SilverScore;
const Score CaptureHorseScore     = HorseScore     + BishopScore;
const Score CaptureDragonScore    = DragonScore    + RookScore;
const Score CaptureKingScore      = KingScore      * 2;

const Score PromotePawnScore      = ProPawnScore   - PawnScore;
const Score PromoteLanceScore     = ProLanceScore  - LanceScore;
const Score PromoteKnightScore    = ProKnightScore - KnightScore;
const Score PromoteSilverScore    = ProSilverScore - SilverScore;
const Score PromoteBishopScore    = HorseScore     - BishopScore;
const Score PromoteRookScore      = DragonScore    - RookScore;

const Score ScoreKnownWin = KingScore;

extern const Score PieceScore[PieceNone];
extern const Score CapturePieceScore[PieceNone];
extern const Score PromotePieceScore[7];

#endif // #ifndef APERY_PIECESCORE_HPP
