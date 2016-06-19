#ifndef APERY_SQUARE_HPP
#define APERY_SQUARE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "color.hpp"

// 盤面を [0, 80] の整数の index で表す
// Bitboard のビットが縦に並んでいて、
// 0 ビット目から順に、以下の位置と対応させる。
// SQ11 = 1一, SQ19 = 1九, SQ99 = 9九
enum Square {
	SQ11, SQ12, SQ13, SQ14, SQ15, SQ16, SQ17, SQ18, SQ19,
	SQ21, SQ22, SQ23, SQ24, SQ25, SQ26, SQ27, SQ28, SQ29,
	SQ31, SQ32, SQ33, SQ34, SQ35, SQ36, SQ37, SQ38, SQ39,
	SQ41, SQ42, SQ43, SQ44, SQ45, SQ46, SQ47, SQ48, SQ49,
	SQ51, SQ52, SQ53, SQ54, SQ55, SQ56, SQ57, SQ58, SQ59,
	SQ61, SQ62, SQ63, SQ64, SQ65, SQ66, SQ67, SQ68, SQ69,
	SQ71, SQ72, SQ73, SQ74, SQ75, SQ76, SQ77, SQ78, SQ79,
	SQ81, SQ82, SQ83, SQ84, SQ85, SQ86, SQ87, SQ88, SQ89,
	SQ91, SQ92, SQ93, SQ94, SQ95, SQ96, SQ97, SQ98, SQ99,
	SquareNum, // = 81
	SquareNoLeftNum = SQ61,
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

// 筋
enum File {
	File1, File2, File3, File4, File5, File6, File7, File8, File9, FileNum,
	FileNoLeftNum = File6
};
OverloadEnumOperators(File);
inline int abs(const File f) { return std::abs(static_cast<int>(f)); }

// 段
enum Rank {
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9, RankNum
};
OverloadEnumOperators(Rank);
inline int abs(const Rank r) { return std::abs(static_cast<int>(r)); }

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
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9,
	Rank1, Rank2, Rank3, Rank4, Rank5, Rank6, Rank7, Rank8, Rank9
};

const File SquareToFile[SquareNum] = {
	File1, File1, File1, File1, File1, File1, File1, File1, File1,
	File2, File2, File2, File2, File2, File2, File2, File2, File2,
	File3, File3, File3, File3, File3, File3, File3, File3, File3,
	File4, File4, File4, File4, File4, File4, File4, File4, File4,
	File5, File5, File5, File5, File5, File5, File5, File5, File5,
	File6, File6, File6, File6, File6, File6, File6, File6, File6,
	File7, File7, File7, File7, File7, File7, File7, File7, File7,
	File8, File8, File8, File8, File8, File8, File8, File8, File8,
	File9, File9, File9, File9, File9, File9, File9, File9, File9
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
	else
		return (direc != DirecMisc && direc == squareRelation(from, to));
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
	static_assert(Rank1 == 0, "");
	return static_cast<bool>(0x1c00007u & (1u << ((c << 4) + fromOrToRank)));
#else
	// 同じ意味。
	return (c == Black ? isInFrontOf<Black, Rank4, Rank6>(fromOrToRank) : isInFrontOf<White, Rank4, Rank6>(fromOrToRank));
#endif
}

#endif // #ifndef APERY_SQUARE_HPP
