#include <iostream>
#include <fstream>
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <map>
#include <cassert>
#include <climits>

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

enum Square {
	A9, B9, C9, D9, E9, F9, G9, H9, I9,
	A8, B8, C8, D8, E8, F8, G8, H8, I8,
	A7, B7, C7, D7, E7, F7, G7, H7, I7,
	A6, B6, C6, D6, E6, F6, G6, H6, I6,
	A5, B5, C5, D5, E5, F5, G5, H5, I5,
	A4, B4, C4, D4, E4, F4, G4, H4, I4,
	A3, B3, C3, D3, E3, F3, G3, H3, I3,
	A2, B2, C2, D2, E2, F2, G2, H2, I2,
	A1, B1, C1, D1, E1, F1, G1, H1, I1,
	nsquare
};
OverloadEnumOperators(Square);

enum FVIndex {
	f_hand_pawn   =    0,
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
	kkp_end         = 738,
	pos_n           = fe_end * ( fe_end + 1 ) / 2
};
OverloadEnumOperators(FVIndex);

const FVIndex Hand0FVIndex[14] = {f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance, f_hand_knight, e_hand_knight, f_hand_silver, e_hand_silver,
								  f_hand_gold, e_hand_gold, f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook};

const FVIndex Hand0KKPFVIndex[7] = {kkp_hand_pawn, kkp_hand_lance, kkp_hand_knight, kkp_hand_silver,
									kkp_hand_gold, kkp_hand_bishop, kkp_hand_rook};

const FVIndex BoardKKPFVIndex[9] = {kkp_pawn, kkp_lance, kkp_knight, kkp_silver,
									kkp_gold, kkp_bishop, kkp_horse, kkp_rook, kkp_dragon};

struct KKPToKP : public std::map<FVIndex, std::pair<FVIndex, FVIndex> > {
	KKPToKP() {
		(*this)[kkp_hand_pawn  ] = std::make_pair(f_hand_pawn  , e_hand_pawn  );
		(*this)[kkp_hand_lance ] = std::make_pair(f_hand_lance , e_hand_lance );
		(*this)[kkp_hand_knight] = std::make_pair(f_hand_knight, e_hand_knight);
		(*this)[kkp_hand_silver] = std::make_pair(f_hand_silver, e_hand_silver);
		(*this)[kkp_hand_gold  ] = std::make_pair(f_hand_gold  , e_hand_gold  );
		(*this)[kkp_hand_bishop] = std::make_pair(f_hand_bishop, e_hand_bishop);
		(*this)[kkp_hand_rook  ] = std::make_pair(f_hand_rook  , e_hand_rook  );
		(*this)[kkp_pawn       ] = std::make_pair(f_pawn       , e_pawn       );
		(*this)[kkp_lance      ] = std::make_pair(f_lance      , e_lance      );
		(*this)[kkp_knight     ] = std::make_pair(f_knight     , e_knight     );
		(*this)[kkp_silver     ] = std::make_pair(f_silver     , e_silver     );
		(*this)[kkp_gold       ] = std::make_pair(f_gold       , e_gold       );
		(*this)[kkp_bishop     ] = std::make_pair(f_bishop     , e_bishop     );
		(*this)[kkp_horse      ] = std::make_pair(f_horse      , e_horse      );
		(*this)[kkp_rook       ] = std::make_pair(f_rook       , e_rook       );
		(*this)[kkp_dragon     ] = std::make_pair(f_dragon     , e_dragon     );
	}
};

KKPToKP g_kkpToKP;

// 入力はKPPの内KPの要素が16bitに収まらない値が付いている場合があり、32bitに変更している。
// KPPType, KKPType を16bitに変更すればBonanzaのfv.binにも使える。
using KPPType    = int32_t;
using KKPType    = int32_t;
using MidKPPType = int32_t;
using MidKKPType = int32_t;
using MidKKType  = int32_t;
using MidKPType  = int32_t;
using OutKPPType = int16_t;
using OutKKPType = int32_t;
using OutKKType  = int32_t;
using OutKPType  = int32_t;
KPPType pc_on_sq[nsquare][fe_end*(fe_end+1)/2];
KKPType kkp[nsquare][nsquare][kkp_end];

MidKPPType mid_pc_on_sq[nsquare][fe_end*(fe_end+1)/2];
MidKKPType mid_kkp[nsquare][nsquare][kkp_end];
MidKKPType mid_new_kkp[nsquare][nsquare][fe_end];
MidKKType  mid_kk[nsquare][nsquare];
MidKPType  mid_kp[nsquare][fe_end];

OutKPPType out_pc_on_sq[nsquare][fe_end*(fe_end+1)/2];
//OutKKPType out_kkp[nsquare][nsquare][kkp_end];
OutKKPType out_new_kkp[nsquare][nsquare][fe_end];
OutKKType  out_kk[nsquare][nsquare];
MidKPType  out_kp[nsquare][fe_end];

#define PcOnSq(k,i)         pc_on_sq[k][(i)*((i)+3)/2]
#define PcPcOnSq(k,i,j)     pc_on_sq[k][(i)*((i)+1)/2+(j)]
#define PcPcOnSqAny(k,i,j) ( i >= j ? PcPcOnSq(k,i,j) : PcPcOnSq(k,j,i) )
#define MidPcOnSq(k,i)         mid_pc_on_sq[k][(i)*((i)+3)/2]
#define MidPcPcOnSq(k,i,j)     mid_pc_on_sq[k][(i)*((i)+1)/2+(j)]
#define MidPcPcOnSqAny(k,i,j) ( i >= j ? MidPcPcOnSq(k,i,j) : MidPcPcOnSq(k,j,i) )
#define Inv(sq)             (nsquare-1-sq)

inline FVIndex hand0Index(const FVIndex handIndex) {
	return *(std::upper_bound(std::begin(Hand0FVIndex), std::end(Hand0FVIndex), handIndex) - 1);
}
inline bool handIs0(const FVIndex handIndex) {
	return std::find(std::begin(Hand0FVIndex), std::end(Hand0FVIndex), handIndex) != std::end(Hand0FVIndex);
}

// 評価関数を38回固定ループにする為に、持ち駒0枚の評価値を他の評価値に足しこみ、持ち駒複数枚の評価値を差分化する。
void convertFV() {
	// K    : 玉
	// B    : 盤上の駒
	// 0    : 持ち駒 0 枚
	// N, M : 持ち駒 0 枚以外

	// K0, K00 を全て足し込んだテーブルを作成
	for (Square ksqb = A9; ksqb < nsquare; ++ksqb) {
		for (Square ksqw = A9; ksqw < nsquare; ++ksqw) {
			int score = 0;
			for (int i = 0; i < 14; ++i) {
				const int k0 = Hand0FVIndex[i];
				for (int j = 0; j <= i; ++j) {
					const int l0 = Hand0FVIndex[j];
					score += PcPcOnSqAny(ksqb, k0, l0);
					// 本当は list0, list1 という風にした方が良いけど、
					// 全部足すので何でも良い。
					score -= PcPcOnSqAny(Inv(ksqw), k0, l0);
				}
			}
			mid_kk[ksqb][ksqw] = score;
		}
	}

	// KK0 も全て足し込む
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			for (FVIndex elem : Hand0KKPFVIndex) {
				mid_kk[ksqB][ksqW] += kkp[ksqB     ][ksqW     ][elem];
				mid_kk[ksqB][ksqW] -= kkp[Inv(ksqW)][Inv(ksqB)][elem];
			}
		}
	}

	// K0B を KB に足し込む。
	// 持ち駒 0 枚以外のとき、KB に余分な値が足し込まれているので、KNB で余分を引く。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand = f_hand_pawn; ihand < fe_hand_end; ++ihand) {
			for (FVIndex iboard = fe_hand_end; iboard < fe_end; ++iboard) {
				if (handIs0(ihand)) {
					MidPcOnSq(ksq, iboard) += PcPcOnSqAny(ksq, ihand, iboard);
				}
				else {
					const FVIndex ihand0 = hand0Index(ihand);
					MidPcPcOnSqAny(ksq, ihand, iboard) -= PcPcOnSqAny(ksq, ihand0, iboard);
				}
			}
		}
	}

	// K0N を KN に足し込む。
	// K00 を KN から引く。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand0 : Hand0FVIndex) {
			for (FVIndex jhand = f_hand_pawn; jhand < fe_hand_end; ++jhand) {
				if (handIs0(jhand)  ) { continue; }
				const FVIndex jhand0 = hand0Index(jhand);
				if (ihand0 == jhand0) { continue; } // 同じ種類の持ち駒の 0 枚と それ以外の枚数は共存しない。
				MidPcOnSq(ksq, jhand) += PcPcOnSqAny(ksq, ihand0, jhand);
				MidPcOnSq(ksq, jhand) -= PcPcOnSqAny(ksq, ihand0, jhand0);
			}
		}
	}

	// KN に K0N が足し込まれている。
	// KM に K0M が足し込まれている。
	// よって、KNM から K0N, K0M を引く。
	// 更に KN, KM から K00 を 2重に引いたので、K00 を KNM に足す。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand = f_hand_pawn; ihand < fe_hand_end; ++ihand) {
			if (handIs0(ihand)) { continue; }
			const FVIndex ihand0 = hand0Index(ihand);
			for (FVIndex jhand = f_hand_pawn; jhand < ihand0; ++jhand) {
				if (handIs0(jhand)) { continue; }
				const FVIndex jhand0 = hand0Index(jhand);
				MidPcPcOnSqAny(ksq, ihand, jhand) -= PcPcOnSqAny(ksq, ihand, jhand0) + PcPcOnSqAny(ksq, ihand0, jhand);
				MidPcPcOnSqAny(ksq, ihand, jhand) += PcPcOnSqAny(ksq, ihand0, jhand0);
			}
		}
	}

	// K0 を mid_kk テーブルに足しこんでいるので、KN から K0 を引く。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand0 : Hand0FVIndex) {
			for (int i = 18; 0 < i; --i) {
				if (4 < i && f_hand_lance  <= ihand0) continue;
				if (2 < i && f_hand_bishop <= ihand0) continue;
				MidPcOnSq(ksq, ihand0 + i) -= PcOnSq(ksq, ihand0);
			}
		}
	}

	// 味方の同じ種類の駒の枚数違いは、本来無い組み合わせなので、0 にしないといけない。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand = f_hand_pawn; ihand < fe_hand_end; ++ihand) {
			const FVIndex ihand0 = hand0Index(ihand);
			for (FVIndex jhand = f_hand_pawn; jhand < ihand; ++jhand) {
				const FVIndex jhand0 = hand0Index(jhand);
				// 同じ種類の持ち駒のときかつ、駒の枚数が違うとき、0 にする。
				if (ihand0 == jhand0 && ihand != jhand) {
					MidPcPcOnSq(ksq, ihand, jhand) = 0;
				}
			}
		}
	}

	// KPP 差分値化
	// KBN を KB(N-1) との差分値にする。(1 < N のとき)
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex iboard = fe_hand_end; iboard < fe_end; ++iboard) {
			for (FVIndex jhand0 : Hand0FVIndex) {
				for (int j = 18; 1 < j; --j) {
					if (4 < j && f_hand_lance  <= jhand0) continue;
					if (2 < j && f_hand_bishop <= jhand0) continue;
					const FVIndex jhand = jhand0 + j;
					MidPcPcOnSq(ksq, iboard, jhand) -= MidPcPcOnSq(ksq, iboard, jhand - 1);
				}
			}
		}
	}

	// KP 差分値化
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand0 : Hand0FVIndex) {
			for (int i = 18; 1 < i; --i) {
				if (4 < i && f_hand_lance  <= ihand0) continue;
				if (2 < i && f_hand_bishop <= ihand0) continue;
				MidPcOnSq(ksq, ihand0 + i) -= MidPcOnSq(ksq, ihand0 + i - 1);
			}
		}
	}

	// KP を KP 用の配列に格納
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex i = f_hand_pawn; i < fe_end; ++i) {
			mid_kp[ksq][i] = MidPcOnSq(ksq, i);
		}
	}

	// KMN を差分化する。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex ihand0 : Hand0FVIndex) {
			for (int i = 18; 0 < i; --i) {
				if (4 < i && f_hand_lance  <= ihand0) continue;
				if (2 < i && f_hand_bishop <= ihand0) continue;
				const FVIndex ihand = ihand0 + i;
				for (FVIndex jhand0 : Hand0FVIndex) {
					if (ihand0 <= jhand0) continue;
					for (int j = 18; 0 < j; --j) {
						if (4 < j && f_hand_lance  <= jhand0) continue;
						if (2 < j && f_hand_bishop <= jhand0) continue;
						const FVIndex jhand = jhand0 + j;
						if      (1 < i && 1 < j)
							MidPcPcOnSq(ksq, ihand, jhand) += -MidPcPcOnSq(ksq, ihand - 1, jhand) - MidPcPcOnSq(ksq, ihand, jhand - 1) + MidPcPcOnSq(ksq, ihand - 1, jhand - 1);
						else if (1 < i         )
							MidPcPcOnSq(ksq, ihand, jhand) += -MidPcPcOnSq(ksq, ihand - 1, jhand);
						else if (1 < j         )
							MidPcPcOnSq(ksq, ihand, jhand) += -MidPcPcOnSq(ksq, ihand, jhand - 1);
					}
				}
			}
		}
	}

	// KP を評価関数で参照しなくて済むように、
	// KP を KKP に足しこむ。
	// 更に、KK に KK0 を押し込んだので、KKN から KK0 を引いておく。
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			auto handcopy = [&](const int handnum, const FVIndex kkp_hand_index) {
				for (int i = 0; i < handnum; ++i) {
					if (i != 0) {
						mid_kkp[ksqB][ksqW][kkp_hand_index + i] -= kkp[ksqB][ksqW][kkp_hand_index];
					}
				}
			};

			handcopy(19, kkp_hand_pawn  );
			handcopy( 5, kkp_hand_lance );
			handcopy( 5, kkp_hand_knight);
			handcopy( 5, kkp_hand_silver);
			handcopy( 5, kkp_hand_gold  );
			handcopy( 3, kkp_hand_bishop);
			handcopy( 3, kkp_hand_rook  );
		}
	}

	// KP = 0
	// 評価値の差分計算時に KP が邪魔にならないように 0 にしておく。
	// 対局時は KP は KKP に足し込んでおけば良いが、
	// 学習に使う際に KP と KKP それぞれの値を知りたい場合もあるので、
	// KP を KKP に足し込まず、別途ファイルに出力しておく。
	for (Square ksq = A9; ksq < nsquare; ++ksq) {
		for (FVIndex i = f_hand_pawn; i < fe_end; ++i) {
			MidPcOnSq(ksq, i) = 0;
		}
	}

	// KK0 = 0
	// 特に必要ではないが、一応やっておく。
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			for (FVIndex elem : Hand0KKPFVIndex) {
				mid_kkp[ksqB][ksqW][elem] = 0;
			}
		}
	}

	// KKP 差分値化
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			for (FVIndex elem : Hand0KKPFVIndex) {
				for (int i = 18; 1 < i; --i) {
					if (4 < i && kkp_hand_lance  <= elem) continue;
					if (2 < i && kkp_hand_bishop <= elem) continue;
					mid_kkp[ksqB][ksqW][elem + i] -= mid_kkp[ksqB][ksqW][elem + i - 1];
				}
			}
		}
	}

	// KKP を KPP と同じインデックスで参照するようにする。
	// 持ち駒について。
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			for (FVIndex elem : Hand0KKPFVIndex) {
				const auto fe_pair = g_kkpToKP[elem];
				for (int i = 0; i < 19; ++i) {
					if (4 < i && kkp_hand_lance  <= elem) continue;
					if (2 < i && kkp_hand_bishop <= elem) continue;
					mid_new_kkp[ksqB][ksqW][fe_pair.first  + i] =  mid_kkp[ksqB     ][ksqW     ][elem + i];
					mid_new_kkp[ksqB][ksqW][fe_pair.second + i] = -mid_kkp[Inv(ksqW)][Inv(ksqB)][elem + i];
				}
			}
		}
	}
	// 盤上の駒について。
	for (Square ksqB = A9; ksqB < nsquare; ++ksqB) {
		for (Square ksqW = A9; ksqW < nsquare; ++ksqW) {
			for (FVIndex elem : BoardKKPFVIndex) {
				const auto fe_pair = g_kkpToKP[elem];
				for (Square sq = A9; sq < nsquare; ++sq) {
					if (!(((elem == kkp_pawn || elem == kkp_lance) && sq < A8) || (elem == kkp_knight && sq < A7)))
						mid_new_kkp[ksqB][ksqW][fe_pair.first  + sq] =  mid_kkp[ksqB     ][ksqW     ][elem + sq     ];
					if (!(((elem == kkp_pawn || elem == kkp_lance) && Inv(sq) < A8) || (elem == kkp_knight && Inv(sq) < A7)))
						mid_new_kkp[ksqB][ksqW][fe_pair.second + sq] = -mid_kkp[Inv(ksqW)][Inv(ksqB)][elem + Inv(sq)];
				}
			}
		}
	}
}

int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::cout << "USAGE: " << argv[0] << " <input fv> <output fv38>\n" << std::endl;
		return 0;
	}
	std::cout << "size of KPPType is " << sizeof(KPPType) << std::endl;
	std::cout << "size of KKPType is " << sizeof(KKPType) << std::endl;
	std::ifstream ifs(argv[1], std::ios::binary);
	std::ofstream ofs(argv[2], std::ios::binary);
	ifs.read(reinterpret_cast<char*>(&pc_on_sq[0][0]), sizeof(pc_on_sq));
	ifs.read(reinterpret_cast<char*>(&kkp[0][0][0]  ), sizeof(kkp     ));

#if 1
	// 誤差軽減の為 KPP, KKP で -2 ~ 2 の間の評価値は 0 にする。
	for (auto it = &(** std::begin(pc_on_sq)); it != &(** std::end(pc_on_sq)); ++it) {
		if (abs(*it) <= 2) *it = 0;
	}
	for (auto it = &(***std::begin(kkp)); it != &(***std::end(kkp)); ++it) {
		if (abs(*it) <= 2) *it = 0;
	}
#endif

	// 入力と出力の型のサイズが違っても良いように、memcpy ではなく std::copy を使う。
	std::copy(&(** std::begin(pc_on_sq)), &(** std::end(pc_on_sq)), &(** std::begin(mid_pc_on_sq)));
	std::copy(&(***std::begin(kkp     )), &(***std::end(kkp     )), &(***std::begin(mid_kkp     )));

	convertFV();

	// ここで桁溢れが起きないか調べた方が良い。
	std::copy(&(** std::begin(mid_pc_on_sq)), &(** std::end(mid_pc_on_sq)), &(** std::begin(out_pc_on_sq)));
//	std::copy(&(***std::begin(mid_kkp     )), &(***std::end(mid_kkp     )), &(***std::begin(out_kkp     )));
	std::copy(&(***std::begin(mid_new_kkp )), &(***std::end(mid_new_kkp )), &(***std::begin(out_new_kkp )));
	std::copy(&(** std::begin(mid_kk      )), &(** std::end(mid_kk      )), &(** std::begin(out_kk      )));
	std::copy(&(** std::begin(mid_kp      )), &(** std::end(mid_kp      )), &(** std::begin(out_kp      )));

	ofs.write(reinterpret_cast<char*>(&out_pc_on_sq[0][0]  ), sizeof(out_pc_on_sq));
	ofs.write(reinterpret_cast<char*>(&out_new_kkp[0][0][0]), sizeof(out_new_kkp ));
	ofs.write(reinterpret_cast<char*>(&out_kk[0][0]        ), sizeof(out_kk      ));
	ofs.write(reinterpret_cast<char*>(&out_kp[0][0]        ), sizeof(out_kp      ));
}
