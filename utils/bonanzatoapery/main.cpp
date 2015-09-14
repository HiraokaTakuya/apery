#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <cinttypes>
#include <cstdlib>

#define OverloadEnumOperators(T)										\
	inline void operator += (T& lhs, const int rhs) { lhs  = static_cast<T>(static_cast<int>(lhs) + rhs); } \
	inline void operator += (T& lhs, const T   rhs) { lhs += static_cast<int>(rhs); } \
	inline void operator -= (T& lhs, const int rhs) { lhs  = static_cast<T>(static_cast<int>(lhs) - rhs); } \
	inline void operator -= (T& lhs, const T   rhs) { lhs -= static_cast<int>(rhs); } \
	inline void operator *= (T& lhs, const int rhs) { lhs  = static_cast<T>(static_cast<int>(lhs) * rhs); } \
	inline void operator /= (T& lhs, const int rhs) { lhs  = static_cast<T>(static_cast<int>(lhs) / rhs); } \
	inline constexpr T operator + (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) + rhs); } \
	inline constexpr T operator + (const T   lhs, const T   rhs) { return lhs + static_cast<int>(rhs); } \
	inline constexpr T operator - (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) - rhs); } \
	inline constexpr T operator - (const T   lhs, const T   rhs) { return lhs - static_cast<int>(rhs); } \
	inline constexpr T operator * (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) * rhs); } \
	inline constexpr T operator * (const int lhs, const T   rhs) { return rhs * lhs; } \
	inline constexpr T operator * (const T   lhs, const T   rhs) { return lhs * static_cast<int>(rhs); } \
	inline constexpr T operator / (const T   lhs, const int rhs) { return static_cast<T>(static_cast<int>(lhs) / rhs); } \
	inline constexpr T operator - (const T   rhs) { return static_cast<T>(-static_cast<int>(rhs)); } \
	inline T operator ++ (T& lhs) { lhs += 1; return lhs; } /* 前置 */	\
	inline T operator -- (T& lhs) { lhs -= 1; return lhs; } /* 前置 */	\
	inline T operator ++ (T& lhs, int) { const T temp = lhs; lhs += 1; return temp; } /* 後置 */ \
	/* inline T operator -- (T& lhs, int) { const T temp = lhs; lhs -= 1; return temp; } */ /* 後置 */

using KPPType    = int16_t;
using KKPType    = int32_t;
using KKType     = int32_t;
using KPType     = int32_t;
using OutKPPType = int16_t;
using OutKKPType = int32_t;
using OutKKType  = int32_t;
using OutKPType  = int32_t;

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

inline bool isInFile(const File f) { return (0 <= f) && (f < FileNum); }
inline bool isInRank(const Rank r) { return (0 <= r) && (r < RankNum); }
// s が Square の中に含まれているか判定
inline bool isInSquare(const Square s) { return (0 <= s) && (s < SquareNum); }
// File, Rank のどちらかがおかしいかも知れない時は、
// こちらを使う。
// こちらの方が遅いが、どんな File, Rank にも対応している。
inline bool isInSquare(const File f, const Rank r) { return isInFile(f) && isInRank(r); }

// 速度が必要な場面で使用するなら、テーブル引きの方が有効だと思う。
inline Square makeSquare(const File f, const Rank r) {
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
}
namespace Bonanza {
	// Bonanzaの
	// 評価関数テーブルのオフセット。
	// f_xxx が味方の駒、e_xxx が敵の駒
	enum { f_hand_pawn   =    0,
		   e_hand_pawn   =   19,
		   f_hand_lance  =   38,
		   e_hand_lance  =   43,
		   f_hand_knight =   48,
		   e_hand_knight =   53,
		   f_hand_silver =   58,
		   e_hand_silver =   63,
		   f_hand_gold   =   68,
		   e_hand_gold   =   73,
		   f_hand_bishop =   78,
		   e_hand_bishop =   81,
		   f_hand_rook   =   84,
		   e_hand_rook   =   87,
		   fe_hand_end   =   90,

		   f_pawn        =   81,
		   e_pawn        =  162,
		   f_lance       =  225,
		   e_lance       =  306,
		   f_knight      =  360,
		   e_knight      =  441,
		   f_silver      =  504,
		   e_silver      =  585,
		   f_gold        =  666,
		   e_gold        =  747,
		   f_bishop      =  828,
		   e_bishop      =  909,
		   f_horse       =  990,
		   e_horse       = 1071,
		   f_rook        = 1152,
		   e_rook        = 1233,
		   f_dragon      = 1314,
		   e_dragon      = 1395,
		   fe_end        = 1476,

		   kkp_hand_pawn   =   0,
		   kkp_hand_lance  =  19,
		   kkp_hand_knight =  24,
		   kkp_hand_silver =  29,
		   kkp_hand_gold   =  34,
		   kkp_hand_bishop =  39,
		   kkp_hand_rook   =  42,
		   kkp_hand_end    =  45,
		   kkp_pawn        =  36,
		   kkp_lance       = 108,
		   kkp_knight      = 171,
		   kkp_silver      = 252,
		   kkp_gold        = 333,
		   kkp_bishop      = 414,
		   kkp_horse       = 495,
		   kkp_rook        = 576,
		   kkp_dragon      = 657,
		   kkp_end         = 738 };

	const int pos_n  = fe_end * ( fe_end + 1 ) / 2;

	enum Square {
		// 必要なものだけ定義
		A9 = 0, A8 = 9, A7 = 18, I3 = 62, I2 = 71, I1 = 80, SquareNum = 81
	};
	OverloadEnumOperators(Square);
}

inline Bonanza::Square inverse(const Bonanza::Square sq) { return Bonanza::SquareNum - 1 - sq; }

// Square のそれぞれの位置の enum の値を、
// Apery 方式と Bonanza 方式で変換。
// ちなみに Apery は ponanza 方式を採用している。
Bonanza::Square squareAperyToBonanza(const Square sq) {
	return static_cast<Bonanza::Square>(8 - makeFile(sq)) + static_cast<Bonanza::Square>(makeRank(sq) * 9);
}

Square squareBonanzaToApery(const Bonanza::Square sqBona) {
	const Rank bonaRank = static_cast<Rank>(sqBona / 9);
	const File bonaFile = static_cast<File>(8 - (sqBona % 9));

	return makeSquare(bonaFile, bonaRank);
}

OutKKType KK[SquareNum][SquareNum];
OutKPPType KPP[SquareNum][Apery::fe_end][Apery::fe_end];
OutKKPType KKP[SquareNum][SquareNum][Apery::fe_end];
OutKPType KP[SquareNum][Apery::fe_end];

void kppBonanzaToApery(const std::vector<KPPType>& kpptmp, const Bonanza::Square sqb1,
					   const Bonanza::Square sqb2, const int fe, const int feB)
{
	const Square sq1 = squareBonanzaToApery(sqb1);
	const Square sq2 = (fe <= Apery::e_hand_rook ? static_cast<Square>(sqb2) : squareBonanzaToApery(sqb2));

#define KPPTMP(k,i,j) ((j <= i) ?										\
					   kpptmp[(k)*Bonanza::pos_n+((i)*((i)+1)/2+(j))] : kpptmp[(k)*Bonanza::pos_n+((j)*((j)+1)/2+(i))])

	for (Bonanza::Square sqb3 = Bonanza::A9; sqb3 < Bonanza::SquareNum; ++sqb3) {
		const Square sq3 = squareBonanzaToApery(sqb3);
		// ここから持ち駒
		if (static_cast<int>(sqb3) <= 18) {
			KPP[sq1][fe + sq2][Apery::f_hand_pawn   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_pawn   + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_pawn   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_pawn   + sqb3);
		}
		if (static_cast<int>(sqb3) <= 4) {
			KPP[sq1][fe + sq2][Apery::f_hand_lance  + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_lance  + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_lance  + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_lance  + sqb3);
			KPP[sq1][fe + sq2][Apery::f_hand_knight + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_knight + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_knight + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_knight + sqb3);
			KPP[sq1][fe + sq2][Apery::f_hand_silver + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_silver + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_silver + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_silver + sqb3);
			KPP[sq1][fe + sq2][Apery::f_hand_gold   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_gold   + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_gold   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_gold   + sqb3);
		}
		if (static_cast<int>(sqb3) <= 2) {
			KPP[sq1][fe + sq2][Apery::f_hand_bishop + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_bishop + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_bishop + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_bishop + sqb3);
			KPP[sq1][fe + sq2][Apery::f_hand_rook   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_hand_rook   + sqb3);
			KPP[sq1][fe + sq2][Apery::e_hand_rook   + sqb3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_hand_rook   + sqb3);
		}

		// ここから盤上の駒
		if (Bonanza::A8 <= sqb3) {
			KPP[sq1][fe + sq2][Apery::f_pawn   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_pawn   + sqb3);
			KPP[sq1][fe + sq2][Apery::f_lance  + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_lance  + sqb3);
		}
		if (sqb3 <= Bonanza::I2) {
			KPP[sq1][fe + sq2][Apery::e_pawn   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_pawn   + sqb3);
			KPP[sq1][fe + sq2][Apery::e_lance  + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_lance  + sqb3);
		}
		if (Bonanza::A7 <= sqb3) {
			KPP[sq1][fe + sq2][Apery::f_knight + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_knight + sqb3);
		}
		if (sqb3 <= Bonanza::I3) {
			KPP[sq1][fe + sq2][Apery::e_knight + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_knight + sqb3);
		}
		KPP[sq1][fe + sq2][Apery::f_silver + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_silver + sqb3);
		KPP[sq1][fe + sq2][Apery::e_silver + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_silver + sqb3);
		KPP[sq1][fe + sq2][Apery::f_gold   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_gold   + sqb3);
		KPP[sq1][fe + sq2][Apery::e_gold   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_gold   + sqb3);
		KPP[sq1][fe + sq2][Apery::f_bishop + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_bishop + sqb3);
		KPP[sq1][fe + sq2][Apery::e_bishop + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_bishop + sqb3);
		KPP[sq1][fe + sq2][Apery::f_horse  + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_horse  + sqb3);
		KPP[sq1][fe + sq2][Apery::e_horse  + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_horse  + sqb3);
		KPP[sq1][fe + sq2][Apery::f_rook   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_rook   + sqb3);
		KPP[sq1][fe + sq2][Apery::e_rook   + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_rook   + sqb3);
		KPP[sq1][fe + sq2][Apery::f_dragon + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::f_dragon + sqb3);
		KPP[sq1][fe + sq2][Apery::e_dragon + sq3] = KPPTMP(sqb1, feB + sqb2, Bonanza::e_dragon + sqb3);
	}
#undef KPPTMP
}
void convert(int argc, char* argv[]) {
	std::ifstream ifs(argv[1], std::ios::binary);
	if (!ifs) {
		std::cout << "I cannot read " << argv[1] << std::endl;
		exit(EXIT_FAILURE);
	}
	std::vector<KPPType> kpptmp(Bonanza::SquareNum * Bonanza::pos_n);
	std::vector<KKPType> kkptmp(Bonanza::SquareNum * Bonanza::SquareNum * Bonanza::fe_end);
	KKType kktmp[Bonanza::SquareNum][Bonanza::SquareNum];
	std::vector<KPType> kptmp(Bonanza::SquareNum * Bonanza::fe_end);

	ifs.read(reinterpret_cast<char*>(&kpptmp[0]),
			 sizeof(KPPType) * static_cast<int>(Bonanza::SquareNum * Bonanza::pos_n));
	ifs.read(reinterpret_cast<char*>(&kkptmp[0])  ,
			 sizeof(KKPType) * static_cast<int>(Bonanza::SquareNum * Bonanza::SquareNum * Bonanza::fe_end));
	ifs.read(reinterpret_cast<char*>(&kktmp[0][0]), sizeof(kktmp));
	ifs.read(reinterpret_cast<char*>(&kptmp[0])  ,
			 sizeof(KPType) * static_cast<int>(Bonanza::SquareNum * Bonanza::fe_end));

	for (Bonanza::Square sqb1 = Bonanza::A9; sqb1 < Bonanza::SquareNum; ++sqb1) {
		for (Bonanza::Square sqb2 = Bonanza::A9; sqb2 < Bonanza::SquareNum; ++sqb2) {
			// ここから持ち駒
			// sqb2 は持ち駒の枚数を表す。
			if (static_cast<int>(sqb2) <= 18) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_pawn  , Bonanza::f_hand_pawn  );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_pawn  , Bonanza::e_hand_pawn  );
			}
			if (static_cast<int>(sqb2) <= 4) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_lance , Bonanza::f_hand_lance );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_lance , Bonanza::e_hand_lance );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_knight, Bonanza::f_hand_knight);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_knight, Bonanza::e_hand_knight);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_silver, Bonanza::f_hand_silver);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_silver, Bonanza::e_hand_silver);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_gold  , Bonanza::f_hand_gold  );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_gold  , Bonanza::e_hand_gold  );
			}
			if (static_cast<int>(sqb2) <= 2) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_bishop, Bonanza::f_hand_bishop);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_bishop, Bonanza::e_hand_bishop);
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_hand_rook  , Bonanza::f_hand_rook  );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_hand_rook  , Bonanza::e_hand_rook  );
			}

			// ここから盤上の駒
			// sqb2 は Bonanza 方式での盤上の位置を表す
			if (Bonanza::A8 <= sqb2) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_pawn  , Bonanza::f_pawn  );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_lance , Bonanza::f_lance );
			}
			if (sqb2 <= Bonanza::I2) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_pawn  , Bonanza::e_pawn  );
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_lance , Bonanza::e_lance );
			}
			if (Bonanza::A7 <= sqb2) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_knight, Bonanza::f_knight);
			}
			if (sqb2 <= Bonanza::I3) {
				kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_knight, Bonanza::e_knight);
			}
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_silver, Bonanza::f_silver);
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_silver, Bonanza::e_silver);
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_gold  , Bonanza::f_gold  );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_gold  , Bonanza::e_gold  );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_bishop, Bonanza::f_bishop);
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_bishop, Bonanza::e_bishop);
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_horse , Bonanza::f_horse );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_horse , Bonanza::e_horse );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_rook  , Bonanza::f_rook  );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_rook  , Bonanza::e_rook  );
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::f_dragon, Bonanza::f_dragon);
			kppBonanzaToApery(kpptmp, sqb1, sqb2, Apery::e_dragon, Bonanza::e_dragon);
		}
	}

	// kkp の Square 変換
	for (Bonanza::Square sqb1 = Bonanza::A9; sqb1 < Bonanza::SquareNum; ++sqb1) {
		const Square sq1 = squareBonanzaToApery(sqb1);
		for (Bonanza::Square sqb2 = Bonanza::A9; sqb2 < Bonanza::SquareNum; ++sqb2) {
			const Square sq2 = squareBonanzaToApery(sqb2);

			KK[sq1][sq2] = kktmp[sqb1][sqb2];
#if defined USE_K_FIX_OFFSET
			KK[sq1][sq2] += K_Fix_Offset[sq1] - K_Fix_Offset[inverse(sq2)];
#endif

			for (Bonanza::Square sqb3 = Bonanza::A9; sqb3 < Bonanza::SquareNum; ++sqb3) {
				const Square sq3 = squareBonanzaToApery(sqb3);

				// ここから持ち駒
				// sqb3 は持ち駒の枚数を表す。
				if (static_cast<int>(sqb3) <= 18) {
					KKP[sq1][sq2][Apery::f_hand_pawn   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_pawn   + sqb3)];

					KKP[sq1][sq2][Apery::e_hand_pawn   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_pawn   + sqb3)];

					// KP に関しては sqb2 のループ全くいらないから無駄だらけ。手抜きコピペ実装。
					KP[sq1][Apery::f_hand_pawn   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_pawn   + sqb3];
					KP[sq1][Apery::e_hand_pawn   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_pawn   + sqb3];
				}
				if (static_cast<int>(sqb3) <= 4) {
					KKP[sq1][sq2][Apery::f_hand_lance  + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_lance  + sqb3)];
					KKP[sq1][sq2][Apery::f_hand_knight + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_knight + sqb3)];
					KKP[sq1][sq2][Apery::f_hand_silver + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_silver + sqb3)];
					KKP[sq1][sq2][Apery::f_hand_gold   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_gold   + sqb3)];

					KKP[sq1][sq2][Apery::e_hand_lance  + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_lance  + sqb3)];
					KKP[sq1][sq2][Apery::e_hand_knight + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_knight + sqb3)];
					KKP[sq1][sq2][Apery::e_hand_silver + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_silver + sqb3)];
					KKP[sq1][sq2][Apery::e_hand_gold   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_gold   + sqb3)];

					KP[sq1][Apery::f_hand_lance  + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_lance  + sqb3];
					KP[sq1][Apery::e_hand_lance  + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_lance  + sqb3];
					KP[sq1][Apery::f_hand_knight + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_knight + sqb3];
					KP[sq1][Apery::e_hand_knight + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_knight + sqb3];
					KP[sq1][Apery::f_hand_silver + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_silver + sqb3];
					KP[sq1][Apery::e_hand_silver + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_silver + sqb3];
					KP[sq1][Apery::f_hand_gold   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_gold   + sqb3];
					KP[sq1][Apery::e_hand_gold   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_gold   + sqb3];
				}
				if (static_cast<int>(sqb3) <= 2) {
					KKP[sq1][sq2][Apery::f_hand_bishop + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_bishop + sqb3)];
					KKP[sq1][sq2][Apery::f_hand_rook   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_rook   + sqb3)];

					KKP[sq1][sq2][Apery::e_hand_bishop + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_bishop + sqb3)];
					KKP[sq1][sq2][Apery::e_hand_rook   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_rook   + sqb3)];

					KP[sq1][Apery::f_hand_bishop + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_bishop + sqb3];
					KP[sq1][Apery::e_hand_bishop + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_bishop + sqb3];
					KP[sq1][Apery::f_hand_rook   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_hand_rook   + sqb3];
					KP[sq1][Apery::e_hand_rook   + sqb3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_hand_rook   + sqb3];
				}

				// ここから盤上の駒
				// sqb3 は Bonanza 方式での盤上の位置を表す
				if (Bonanza::A8 <= sqb3) {
					KKP[sq1][sq2][Apery::f_pawn   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_pawn   + sqb3)];
					KKP[sq1][sq2][Apery::f_lance  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_lance  + sqb3)];

					KP[sq1][Apery::f_pawn   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_pawn   + sqb3];
					KP[sq1][Apery::f_lance  + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_lance  + sqb3];
				}
				if (Bonanza::A8 <= inverse(sqb3)) {
					KKP[sq1][sq2][Apery::e_pawn   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_pawn   + sqb3)];
					KKP[sq1][sq2][Apery::e_lance  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_lance  + sqb3)];

					KP[sq1][Apery::e_pawn   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_pawn   + sqb3];
					KP[sq1][Apery::e_lance  + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_lance  + sqb3];
				}
				if (Bonanza::A7 <= sqb3) {
					KKP[sq1][sq2][Apery::f_knight + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_knight + sqb3)];

					KP[sq1][Apery::f_knight + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_knight + sqb3];
				}
				if (Bonanza::A7 <= inverse(sqb3)) {
					KKP[sq1][sq2][Apery::e_knight + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_knight + sqb3)];

					KP[sq1][Apery::e_knight + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_knight + sqb3];
				}
				KKP[sq1][sq2][Apery::f_silver + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_silver + sqb3)];
				KKP[sq1][sq2][Apery::f_gold   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_gold   + sqb3)];
				KKP[sq1][sq2][Apery::f_bishop + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_bishop + sqb3)];
				KKP[sq1][sq2][Apery::f_horse  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_horse  + sqb3)];
				KKP[sq1][sq2][Apery::f_rook   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_rook   + sqb3)];
				KKP[sq1][sq2][Apery::f_dragon + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_dragon + sqb3)];

				KKP[sq1][sq2][Apery::e_silver + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_silver + sqb3)];
				KKP[sq1][sq2][Apery::e_gold   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_gold   + sqb3)];
				KKP[sq1][sq2][Apery::e_bishop + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_bishop + sqb3)];
				KKP[sq1][sq2][Apery::e_horse  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_horse  + sqb3)];
				KKP[sq1][sq2][Apery::e_rook   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_rook   + sqb3)];
				KKP[sq1][sq2][Apery::e_dragon + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_dragon + sqb3)];

				KP[sq1][Apery::f_silver + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_silver + sqb3];
				KP[sq1][Apery::f_gold   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_gold   + sqb3];
				KP[sq1][Apery::f_bishop + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_bishop + sqb3];
				KP[sq1][Apery::f_horse  + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_horse  + sqb3];
				KP[sq1][Apery::f_rook   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_rook   + sqb3];
				KP[sq1][Apery::f_dragon + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::f_dragon + sqb3];

				KP[sq1][Apery::e_silver + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_silver + sqb3];
				KP[sq1][Apery::e_gold   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_gold   + sqb3];
				KP[sq1][Apery::e_bishop + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_bishop + sqb3];
				KP[sq1][Apery::e_horse  + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_horse  + sqb3];
				KP[sq1][Apery::e_rook   + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_rook   + sqb3];
				KP[sq1][Apery::e_dragon + sq3] = kptmp[sqb1 * Bonanza::fe_end + Bonanza::e_dragon + sqb3];
			}
		}
	}
	// 評価関数バイナリをファイル生成したければ、ここでファイルに出力する。
	std::string dir = argv[2];
	if (dir.back() != '/') dir += "/";

	const std::string oKPPFile = dir + "kpps.kpp.bin";
	std::ofstream ofs_kpp(oKPPFile.c_str(), std::ios::binary);
	if (!ofs_kpp) {
		std::cout << "I cannot write " << oKPPFile << std::endl;
		exit(EXIT_FAILURE);
	}
	ofs_kpp.write(reinterpret_cast<char*>(&KPP[0][0][0]), sizeof(KPP));
	const std::string oKKPFile = dir + "kkps.kkp.bin";
	std::ofstream ofs_kkp(oKKPFile.c_str(), std::ios::binary);
	if (!ofs_kkp) {
		std::cout << "I cannot write " << oKKPFile << std::endl;
		exit(EXIT_FAILURE);
	}
	ofs_kkp.write(reinterpret_cast<char*>(&KKP[0][0][0]), sizeof(KKP));
	const std::string oKKFile = dir + "kks.kk.bin";
	std::ofstream ofs_kk(oKKFile.c_str(), std::ios::binary);
	if (!ofs_kk) {
		std::cout << "I cannot write " << oKKFile << std::endl;
		exit(EXIT_FAILURE);
	}
	ofs_kk.write(reinterpret_cast<char*>(&KK[0][0]    ), sizeof(KK ));
	const std::string oKPFile = dir + "kkps.kp.bin";
	std::ofstream ofs_kp(oKPFile.c_str(), std::ios::binary);
	if (!ofs_kp) {
		std::cout << "I cannot write " << oKPFile << std::endl;
		exit(EXIT_FAILURE);
	}
	ofs_kp.write(reinterpret_cast<char*>(&KP[0][0]    ), sizeof(KP ));

}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cout << "USAGE: " << argv[0] << " <input bonanza fv38 file> <output apery fv38 dir>\n" << std::endl;
		return 0;
	}
	std::cout << "size of    KPPType is " << sizeof(   KPPType) << std::endl;
	std::cout << "size of    KKPType is " << sizeof(   KKPType) << std::endl;
	std::cout << "size of     KKType is " << sizeof(    KKType) << std::endl;
	std::cout << "size of     KPType is " << sizeof(    KPType) << std::endl;
	std::cout << "size of OutKPPType is " << sizeof(OutKPPType) << std::endl;
	std::cout << "size of OutKKPType is " << sizeof(OutKKPType) << std::endl;
	std::cout << "size of  OutKKType is " << sizeof( OutKKType) << std::endl;
	std::cout << "size of  OutKPType is " << sizeof( OutKPType) << std::endl;
	convert(argc, argv);
}
