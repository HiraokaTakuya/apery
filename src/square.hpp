#ifndef APERY_SQUARE_HPP
#define APERY_SQUARE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "color.hpp"

// 盤面を [0, 80] の整数の index で表す
// I9 = 1一, I1 = 1九, A1 = 9九
//
// A9, B9, C9, D9, E9, F9, G9, H9, I9,
// A8, B8, C8, D8, E8, F8, G8, H8, I8,
// A7, B7, C7, D7, E7, F7, G7, H7, I7,
// A6, B6, C6, D6, E6, F6, G6, H6, I6,
// A5, B5, C5, D5, E5, F5, G5, H5, I5,
// A4, B4, C4, D4, E4, F4, G4, H4, I4,
// A3, B3, C3, D3, E3, F3, G3, H3, I3,
// A2, B2, C2, D2, E2, F2, G2, H2, I2,
// A1, B1, C1, D1, E1, F1, G1, H1, I1,
//
// Bitboard のビットが縦に並んでいて、
// 0 ビット目から順に、以下の位置と対応させる。
enum Square {
	I9, I8, I7, I6, I5, I4, I3, I2, I1,
	H9,	H8, H7, H6, H5, H4, H3, H2, H1,
	G9,	G8, G7, G6, G5, G4, G3, G2, G1,
	F9,	F8, F7, F6, F5, F4, F3, F2, F1,
	E9,	E8, E7, E6, E5, E4, E3, E2, E1,
	D9,	D8, D7, D6, D5, D4, D3, D2, D1,
	C9,	C8, C7, C6, C5, C4, C3, C2, C1,
	B9,	B8, B7, B6, B5, B4, B3, B2, B1,
	A9,	A8, A7, A6, A5, A4, A3, A2, A1,
	SquareNum, // = 81
	B_hand_pawn   = SquareNum     + -1,
	B_hand_lance  = B_hand_pawn   + 18,
	B_hand_knight = B_hand_lance  +  4,
	B_hand_silver = B_hand_knight +  4,
	B_hand_gold   = B_hand_silver +  4,
	B_hand_bishop = B_hand_gold   +  4,
	B_hand_rook   = B_hand_bishop +  2,
	W_hand_pawn   = B_hand_rook   +  2,
	W_hand_lance  = W_hand_pawn   + 18,
	W_hand_knight = W_hand_lance  +  4,
	W_hand_silver = W_hand_knight +  4,
	W_hand_gold   = W_hand_silver +  4,
	W_hand_bishop = W_hand_gold   +  4,
	W_hand_rook   = W_hand_bishop +  2,
	SquareHandNum = W_hand_rook   +  3
};
OverloadEnumOperators(Square);

enum File {
	FileI, FileH, FileG, FileF, FileE, FileD, FileC, FileB, FileA, FileNum
};
OverloadEnumOperators(File);

enum Rank {
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1, RankNum
};
OverloadEnumOperators(Rank);

// 先手のときは BRANK, 後手のときは WRANK より target が前の段にあるなら true を返す。
template <Color US, Rank BRANK, Rank WRANK>
inline bool isInFrontOf(const Rank target) { return (US == Black ? (target < BRANK) : (WRANK < target)); }

template <Color US, Rank BRANK, Rank WRANK>
inline bool isBehind(const Rank target) { return (US == Black ? (BRANK < target) : (target < WRANK)); }

template <Color US, File BFILE, File WFILE>
inline bool isLeftOf(const File target) { return (US == Black ? (BFILE < target) : (target < WFILE)); }

template <Color US, File BFILE, File WFILE>
inline bool isRightOf(const File target) { return (US == Black ? (target < BFILE) : (WFILE < target)); }

enum SquareDelta {
	DeltaNothing = 0, // 同一の Square にあるとき
	DeltaN = -1, DeltaE = -9, DeltaS = 1, DeltaW = 9,
	DeltaNE = DeltaN + DeltaE,
	DeltaSE = DeltaS + DeltaE,
	DeltaSW = DeltaS + DeltaW,
	DeltaNW = DeltaN + DeltaW
};
OverloadEnumOperators(SquareDelta);

inline Square operator + (const Square lhs, const SquareDelta rhs) { return lhs + static_cast<Square>(rhs); }
inline void operator += (Square& lhs, const SquareDelta rhs) { lhs = lhs + static_cast<Square>(rhs); }
inline Square operator - (const Square lhs, const SquareDelta rhs) { return lhs - static_cast<Square>(rhs); }
inline void operator -= (Square& lhs, const SquareDelta rhs) { lhs = lhs - static_cast<Square>(rhs); }

inline bool isInFile(const File f) { return (0 <= f) && (f < FileNum); }
inline bool isInRank(const Rank r) { return (0 <= r) && (r < RankNum); }
// s が Square の中に含まれているか判定
inline bool isInSquare(const Square s) { return (0 <= s) && (s < SquareNum); }
// File, Rank のどちらかがおかしいかも知れない時は、
// こちらを使う。
// こちらの方が遅いが、どんな File, Rank にも対応している。
inline bool isInSquare(const File f, const Rank r) { return isInFile(f) && isInRank(r); }

// 速度が必要な場面で使用するなら、テーブル引きの方が有効だと思う。
inline constexpr Square makeSquare(const File f, const Rank r) {
	return static_cast<Square>(static_cast<int>(f) * 9 + static_cast<int>(r));
}

const Rank SquareToRank[SquareNum] = {
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1,
	Rank9, Rank8, Rank7, Rank6, Rank5, Rank4, Rank3, Rank2, Rank1
};

const File SquareToFile[SquareNum] = {
	FileI, FileI, FileI, FileI, FileI, FileI, FileI, FileI, FileI,
	FileH, FileH, FileH, FileH, FileH, FileH, FileH, FileH, FileH,
	FileG, FileG, FileG, FileG, FileG, FileG, FileG, FileG, FileG,
	FileF, FileF, FileF, FileF, FileF, FileF, FileF, FileF, FileF,
	FileE, FileE, FileE, FileE, FileE, FileE, FileE, FileE, FileE,
	FileD, FileD, FileD, FileD, FileD, FileD, FileD, FileD, FileD,
	FileC, FileC, FileC, FileC, FileC, FileC, FileC, FileC, FileC,
	FileB, FileB, FileB, FileB, FileB, FileB, FileB, FileB, FileB,
	FileA, FileA, FileA, FileA, FileA, FileA, FileA, FileA, FileA
};

// 速度が必要な場面で使用する。
inline Rank makeRank(const Square s) {
	assert(isInSquare(s));
	return SquareToRank[s];
}
inline File makeFile(const Square s) {
	assert(isInSquare(s));
	return SquareToFile[s];
}

// 位置関係、方向
// ボナンザそのまま
// でもあまり使わないので普通の enum と同様に 0 から順に値を付けて行けば良いと思う。
enum Direction {
	DirecMisc     = Binary<  0>::value, // 縦、横、斜めの位置に無い場合
	DirecFile     = Binary< 10>::value, // 縦
	DirecRank     = Binary< 11>::value, // 横
	DirecDiagNESW = Binary<100>::value, // 右上から左下
	DirecDiagNWSE = Binary<101>::value, // 左上から右下
	DirecCross    = Binary< 10>::value, // 縦、横
	DirecDiag     = Binary<100>::value, // 斜め
};
OverloadEnumOperators(Direction);

// 2つの位置関係のテーブル
extern Direction SquareRelation[SquareNum][SquareNum];
inline Direction squareRelation(const Square sq1, const Square sq2) { return SquareRelation[sq1][sq2]; }

// 何かの駒で一手で行ける位置関係についての距離のテーブル。桂馬の位置は距離1とする。
extern int SquareDistance[SquareNum][SquareNum];
inline int squareDistance(const Square sq1, const Square sq2) { return SquareDistance[sq1][sq2]; }

// from, to, ksq が 縦横斜めの同一ライン上にあれば true を返す。
template <bool FROM_KSQ_NEVER_BE_DIRECMISC>
inline bool isAligned(const Square from, const Square to, const Square ksq) {
	const Direction direc = squareRelation(from, ksq);
	if (FROM_KSQ_NEVER_BE_DIRECMISC) {
		assert(direc != DirecMisc);
		return (direc == squareRelation(from, to));
	}
	else {
		return (direc != DirecMisc && direc == squareRelation(from, to));
	}
}

inline char fileToCharUSI(const File f) { return '1' + f; }
// todo: アルファベットが辞書順に並んでいない処理系があるなら対応すること。
inline char rankToCharUSI(const Rank r) {
	static_assert('a' + 1 == 'b', "");
	static_assert('a' + 2 == 'c', "");
	static_assert('a' + 3 == 'd', "");
	static_assert('a' + 4 == 'e', "");
	static_assert('a' + 5 == 'f', "");
	static_assert('a' + 6 == 'g', "");
	static_assert('a' + 7 == 'h', "");
	static_assert('a' + 8 == 'i', "");
	return 'a' + r;
}
inline std::string squareToStringUSI(const Square sq) {
	const Rank r = makeRank(sq);
	const File f = makeFile(sq);
	const char ch[] = {fileToCharUSI(f), rankToCharUSI(r), '\0'};
	return std::string(ch);
}

inline char fileToCharCSA(const File f) { return '1' + f; }
inline char rankToCharCSA(const Rank r) { return '1' + r; }
inline std::string squareToStringCSA(const Square sq) {
	const Rank r = makeRank(sq);
	const File f = makeFile(sq);
	const char ch[] = {fileToCharCSA(f), rankToCharCSA(r), '\0'};
	return std::string(ch);
}

inline File charCSAToFile(const char c) { return static_cast<File>(c - '1'); }
inline Rank charCSAToRank(const char c) { return static_cast<Rank>(c - '1'); }
inline File charUSIToFile(const char c) { return static_cast<File>(c - '1'); }
inline Rank charUSIToRank(const char c) { return static_cast<Rank>(c - 'a'); }

// 後手の位置を先手の位置へ変換
inline constexpr Square inverse(const Square sq) { return SquareNum - 1 - sq; }
// 左右変換
inline constexpr File inverse(const File f) { return FileNum - 1 - f; }
// 上下変換
inline constexpr Rank inverse(const Rank r) { return RankNum - 1 - r; }
// Square の左右だけ変換
inline Square inverseFile(const Square sq) { return makeSquare(inverse(makeFile(sq)), makeRank(sq)); }

inline constexpr Square inverseIfWhite(const Color c, const Square sq) { return (c == Black ? sq : inverse(sq)); }

inline bool canPromote(const Color c, const Rank fromOrToRank) {
#if 1
	static_assert(Black == 0, "");
	static_assert(Rank9 == 0, "");
	return static_cast<bool>(0x1c00007u & (1u << ((c << 4) + fromOrToRank)));
#else
	// 同じ意味。
	return (c == Black ? isInFrontOf<Black, Rank6, Rank4>(fromOrToRank) : isInFrontOf<White, Rank6, Rank4>(fromOrToRank));
#endif
}

#endif // #ifndef APERY_SQUARE_HPP
