#ifndef EVALUATE_HPP
#define EVALUATE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "square.hpp"
#include "piece.hpp"
#include "pieceScore.hpp"

namespace Apery {
	// 評価関数テーブルのオフセット。
	// f_xxx が味方の駒、e_xxx が敵の駒
	// Bonanza の影響で持ち駒 0 の場合のインデックスが存在するが、参照する事は無い。
	// todo: 持ち駒 0 の位置を詰めてテーブルを少しでも小さくする。(キャッシュに少しは乗りやすい?)
	enum { f_hand_pawn   = 0, // 0
		   e_hand_pawn   = f_hand_pawn   + 19,
		   f_hand_lance  = e_hand_pawn   + 19,
		   e_hand_lance  = f_hand_lance  +  5,
		   f_hand_knight = e_hand_lance  +  5,
		   e_hand_knight = f_hand_knight +  5,
		   f_hand_silver = e_hand_knight +  5,
		   e_hand_silver = f_hand_silver +  5,
		   f_hand_gold   = e_hand_silver +  5,
		   e_hand_gold   = f_hand_gold   +  5,
		   f_hand_bishop = e_hand_gold   +  5,
		   e_hand_bishop = f_hand_bishop +  3,
		   f_hand_rook   = e_hand_bishop +  3,
		   e_hand_rook   = f_hand_rook   +  3,
		   fe_hand_end   = e_hand_rook   +  3,

		   f_pawn        = fe_hand_end,
		   e_pawn        = f_pawn        + 81,
		   f_lance       = e_pawn        + 81,
		   e_lance       = f_lance       + 81,
		   f_knight      = e_lance       + 81,
		   e_knight      = f_knight      + 81,
		   f_silver      = e_knight      + 81,
		   e_silver      = f_silver      + 81,
		   f_gold        = e_silver      + 81,
		   e_gold        = f_gold        + 81,
		   f_bishop      = e_gold        + 81,
		   e_bishop      = f_bishop      + 81,
		   f_horse       = e_bishop      + 81,
		   e_horse       = f_horse       + 81,
		   f_rook        = e_horse       + 81,
		   e_rook        = f_rook        + 81,
		   f_dragon      = e_rook        + 81,
		   e_dragon      = f_dragon      + 81,
		   fe_end        = e_dragon      + 81
	};

	const int FVScale = 32;

	const int KPPIndexArray[] = { f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance, f_hand_knight,
								  e_hand_knight, f_hand_silver, e_hand_silver, f_hand_gold, e_hand_gold,
								  f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook, /*fe_hand_end,*/
								  f_pawn, e_pawn, f_lance, e_lance, f_knight, e_knight, f_silver, e_silver,
								  f_gold, e_gold, f_bishop, e_bishop, f_horse, e_horse, f_rook, e_rook,
								  f_dragon, e_dragon, fe_end };

	inline Square kppIndexToSquare(const int i) {
		const auto it = std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i);
		return static_cast<Square>(i - *(it - 1));
	}
}

// 評価関数用テーブル

extern int K00Sum[SquareNum][SquareNum];
// KPP: King-Piece-Piece 玉と2駒の関係を評価するためのテーブル
// Bonanza の三角テーブルを矩形テーブルに直したもの。
extern s16 KPP[SquareNum][Apery::fe_end][Apery::fe_end];
// KKP: King-King-Piece 2玉と駒の関係を評価するためのテーブル
extern s32 KKP[SquareNum][SquareNum][Apery::fe_end];

inline Score kpp(const Square sq, const int i) {
	return static_cast<Score>(KPP[sq][i][i]);
}
inline Score kpp(const Square sq, const int i, const int j) {
	return static_cast<Score>(KPP[sq][i][j]);
}
inline Score kkp(const Square sq1, const Square sq2, const int i) {
	return static_cast<Score>(KKP[sq1][sq2][i]);
}

extern const int kppArray[31];
extern const int kkpArray[15];
extern const int kppHandArray[ColorNum][HandPieceNum];

class Position;
struct SearchStack;

//const size_t EvaluateTableSize = 0x1000000; // 134MB
const size_t EvaluateTableSize = 0x80000000; // 17GB

// 64bit 変数1つなのは理由があって、
// データを取得している最中に他のスレッドから書き換えられることが無くなるから。
// lockless hash と呼ばれる。
// 128bit とかだったら、64bitずつ2回データを取得しないといけないので、
// key と score が対応しない可能性がある。
// transposition table は正にその問題を抱えているが、
// 静的評価値のように差分評価をする訳ではないので、問題になることは少ない。
// 64bitに収まらない場合や、transposition table なども安全に扱いたいなら、
// lockする、SSEやAVX命令を使う、チェックサムを持たせる、key を複数の変数に分けて保持するなどの方法がある。
// 32bit OS 場合、64bit 変数を 32bitずつ2回データ取得するので、下位32bitを上位32bitでxorして
// データ取得時に壊れていないか確認する。
// 31- 0 keyhigh32
// 63-32 score
struct EvaluateHashEntry {
	u32 key() const     { return static_cast<u32>(word); }
	Score score() const { return static_cast<Score>(static_cast<s64>(word) >> 32); }
	void save(const Key k, const Score s) {
		word = static_cast<u64>(k >> 32) | static_cast<u64>(static_cast<s64>(s) << 32);
	}
#if defined __x86_64__
	void encode() {}
	void decode() {}
#else
	void encode() { word ^= word >> 32; }
	void decode() { word ^= word >> 32; }
#endif
	u64 word;
};

struct EvaluateHashTable : HashTable<EvaluateHashEntry, EvaluateTableSize> {};

Score evaluateUnUseDiff(const Position& pos);
Score evaluate(Position& pos, SearchStack* ss);

extern EvaluateHashTable g_evalTable;

#endif // #ifndef EVALUATE_HPP
