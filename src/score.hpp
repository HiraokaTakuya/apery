#ifndef APERY_SCORE_HPP
#define APERY_SCORE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"

using Ply = int;

const Ply MaxPly = 128;
const Ply MaxPlyPlus4 = MaxPly + 4;

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
