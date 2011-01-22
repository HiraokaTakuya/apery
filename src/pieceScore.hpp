#ifndef PIECESCORE_HPP
#define PIECESCORE_HPP

#include "score.hpp"
#include "piece.hpp"

const Score PawnScore             = static_cast<Score>( 100 * 0.9);
const Score LanceScore            = static_cast<Score>( 350 * 0.9);
const Score KnightScore           = static_cast<Score>( 450 * 0.9);
const Score SilverScore           = static_cast<Score>( 550 * 0.9);
const Score GoldScore             = static_cast<Score>( 600 * 0.9);
const Score BishopScore           = static_cast<Score>( 950 * 0.9);
const Score RookScore             = static_cast<Score>(1100 * 0.9);
const Score ProPawnScore          = static_cast<Score>( 600 * 0.9);
const Score ProLanceScore         = static_cast<Score>( 600 * 0.9);
const Score ProKnightScore        = static_cast<Score>( 600 * 0.9);
const Score ProSilverScore        = static_cast<Score>( 600 * 0.9);
const Score HorseScore            = static_cast<Score>(1050 * 0.9);
const Score DragonScore           = static_cast<Score>(1550 * 0.9);

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

#endif // #ifndef PIECESCORE_HPP
