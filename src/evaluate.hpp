#ifndef EVALUATE_HPP
#define EVALUATE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "square.hpp"
#include "piece.hpp"
#include "pieceScore.hpp"

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
inline int kppIndexBegin(const int i) {
	return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 1);
}
inline bool kppIndexIsBlack(const int i) {
	// f_xxx と e_xxx が交互に配列に格納されているので、インデックスが偶数の時は Black
	return !((std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 1) - std::begin(KPPIndexArray) & 1);
}
inline int kppBlackIndexToWhiteBegin(const int i) {
	assert(kppIndexIsBlack(i));
	return *std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i);
}
inline int kppWhiteIndexToBlackBegin(const int i) {
	return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - 2);
}
inline int kppIndexToOpponentBegin(const int i, const bool isBlack) {
	return *(std::upper_bound(std::begin(KPPIndexArray), std::end(KPPIndexArray), i) - static_cast<int>(!isBlack) * 2);
}
inline int kppIndexToOpponentBegin(const int i) {
	// todo: 高速化
	return kppIndexToOpponentBegin(i, kppIndexIsBlack(i));
}

inline int inverseFileIndexIfLefterThanMiddle(const int index) {
	if (index < fe_hand_end) return index;
	const int begin = kppIndexBegin(index);
	const Square sq = static_cast<Square>(index - begin);
	if (sq <= E1) return index;
	return static_cast<int>(begin + inverseFile(sq));
};
inline int inverseFileIndexIfOnBoard(const int index) {
	if (index < fe_hand_end) return index;
	const int begin = kppIndexBegin(index);
	const Square sq = static_cast<Square>(index - begin);
	return static_cast<int>(begin + inverseFile(sq));
};
inline int inverseFileIndexOnBoard(const int index) {
	assert(f_pawn <= index);
	const int begin = kppIndexBegin(index);
	const Square sq = static_cast<Square>(index - begin);
	return static_cast<int>(begin + inverseFile(sq));
};

struct KPPBoardIndexStartToPiece : public std::unordered_map<int, Piece> {
	KPPBoardIndexStartToPiece() {
		(*this)[f_pawn  ] = BPawn;
		(*this)[e_pawn  ] = WPawn;
		(*this)[f_lance ] = BLance;
		(*this)[e_lance ] = WLance;
		(*this)[f_knight] = BKnight;
		(*this)[e_knight] = WKnight;
		(*this)[f_silver] = BSilver;
		(*this)[e_silver] = WSilver;
		(*this)[f_gold  ] = BGold;
		(*this)[e_gold  ] = WGold;
		(*this)[f_bishop] = BBishop;
		(*this)[e_bishop] = WBishop;
		(*this)[f_horse ] = BHorse;
		(*this)[e_horse ] = WHorse;
		(*this)[f_rook  ] = BRook;
		(*this)[e_rook  ] = WRook;
		(*this)[f_dragon] = BDragon;
		(*this)[e_dragon] = WDragon;
	}
	Piece value(const int i) const {
		const auto it = find(i);
		if (it == std::end(*this))
			return PieceNone;
		return it->second;
	}
};
extern KPPBoardIndexStartToPiece g_kppBoardIndexStartToPiece;

// 探索時に参照する評価関数テーブル
extern s32 KK[SquareNum][SquareNum];
extern s16 KPP[SquareNum][fe_end][fe_end];
extern s32 KKP[SquareNum][SquareNum][fe_end];

extern const s32 K_Fix_Offset[SquareNum];

class Position;

template<typename KPPType, typename KKPType, typename KKType> struct Evaluater {
	static const int R_Mid = 8; // 相対位置の中心のindex
	union {
		// 冗長に配列を確保しているが、対称な関係にある時は常に若いindexの方にアクセスすることにする。
		// 例えば kpp だったら、k が優先的に小さくなるようする。左右の対称も含めてアクセス位置を決める。
		// ただし、kkp に関する項目 (kkp, r_kkp_b, r_kkp_h) のみ、p は味方の駒として扱うので、k0 < k1 となるとは限らない。
		struct {
			KPPType kpp[SquareNum][fe_end][fe_end];
			// 相対位置は[file][rank]の順
			KPPType r_kpp_bb[PieceNone][17][17][PieceNone][17][17];
			KPPType r_kpp_hb[fe_hand_end][PieceNone][17][17];
			KPPType pp[fe_end][fe_end];
			KPPType r_pp_bb[PieceNone][PieceNone][17][17];
			KPPType r_pp_hb[fe_hand_end][PieceNone];

			KKPType kkp[SquareNum][SquareNum][fe_end];
			KKPType kp[SquareNum][fe_end];
			KKPType r_kkp_b[17][17][PieceNone][17][17];
			KKPType r_kkp_h[17][17][fe_hand_end];
			KKPType r_kp_b[PieceNone][17][17];
			KKPType r_kp_h[fe_hand_end];

			KKType kk[SquareNum][SquareNum];
			KKType k[SquareNum];
			KKType r_kk[17][17];
		};
		// これらは↑のstructに配列としてアクセスする為のもの。
		// 配列の要素数は上のstructのサイズから分かるはずだが無名structなのでsizeof()使いにくいから使わない。
		// 先頭さえ分かれば良いので要素数1で良い。
		KPPType oneArrayKPP[1];
		KKPType oneArrayKKP[1];
		KKType oneArrayKK[1];
	};
	// todo: これらは学習時しか使わないのにメモリ使うから設計が間違っている。
	KPPType kpp_raw[SquareNum][fe_end][fe_end];
	KKPType kkp_raw[SquareNum][SquareNum][fe_end];
	KKType  kk_raw[SquareNum][SquareNum];

	// todo: これらややこしいし汚いので使わないようにする。
	//       型によっては kkps_begin_index などの値が異なる。
	//       ただ、end - begin のサイズは型によらず一定。
	size_t kpps_begin_index() const { return &kpp[0][0][0] - &oneArrayKPP[0]; }
	size_t kpps_end_index() const { return kpps_begin_index() + (sizeof(kpp)+sizeof(r_kpp_bb)+sizeof(r_kpp_hb)+sizeof(pp)+sizeof(r_pp_bb)+sizeof(r_pp_hb))/sizeof(KPPType); }
	size_t kkps_begin_index() const { return &kkp[0][0][0] - &oneArrayKKP[0]; }
	size_t kkps_end_index() const { return kkps_begin_index() + (sizeof(kkp)+sizeof(kp)+sizeof(r_kkp_b)+sizeof(r_kkp_h)+sizeof(r_kp_b)+sizeof(r_kp_h))/sizeof(KKPType); }
	size_t kks_begin_index() const { return &kk[0][0] - &oneArrayKK[0]; }
	size_t kks_end_index() const { return kks_begin_index() + (sizeof(kk)+sizeof(k)+sizeof(r_kk))/sizeof(KKType); }

	std::array<ptrdiff_t, 5> kppIndices(Square ksq, int i, int j) {
		// 最後の要素は常に max 値 が入ることとする。
		std::array<ptrdiff_t, 5> ret;
		int retIdx = 0;
		// 無効なインデックスは最大値にしておく。
		std::fill(std::begin(ret), std::end(ret), std::numeric_limits<ptrdiff_t>::max());
		// i == j のKP要素はKKPの方で行うので、こちらでは何も有効なindexを返さない。
		if (i == j) return ret;
		if (j < i) std::swap(i, j);

		if (E1 < ksq) {
			ksq = inverseFile(ksq);
			i = inverseFileIndexIfOnBoard(i);
			j = inverseFileIndexIfOnBoard(j);
			if (j < i) std::swap(i, j);
		}
		else if (makeFile(ksq) == FileE) {
			assert(i < j);
			if (f_pawn <= i) {
				const int ibegin = kppIndexBegin(i);
				const Square isq = static_cast<Square>(i - ibegin);
				if (E1 < isq) {
					i = ibegin + inverseFile(isq);
					j = inverseFileIndexOnBoard(j);
				}
				else if (makeFile(isq) == FileE) {
					j = inverseFileIndexIfLefterThanMiddle(j);
				}
			}
		}
		if (j < i) std::swap(i, j);

		ret[retIdx++] = &kpp[ksq][i][j] - &oneArrayKPP[0];

		assert(i < j);
		if (j < fe_hand_end) {
			// i, j 共に持ち駒
			// 相対位置無し。
			ret[retIdx++] = &pp[i][j] - &oneArrayKPP[0];
		}
		else if (i < fe_hand_end) {
			// i 持ち駒、 j 盤上
			const int jbegin = kppIndexBegin(j);
			const Piece jpiece = g_kppBoardIndexStartToPiece.value(jbegin);
			const Square jsq = static_cast<Square>(j - jbegin);
			const Rank krank = makeRank(ksq);
			const File kfile = makeFile(ksq);
			const Rank jrank = makeRank(jsq);
			const File jfile = makeFile(jsq);
			ret[retIdx++] = &r_kpp_hb[i][jpiece][R_Mid + -abs(kfile - jfile)][R_Mid + krank - jrank] - &oneArrayKPP[0];
			ret[retIdx++] = &r_pp_hb[i][jpiece] - &oneArrayKPP[0];

			ret[retIdx++] = &pp[i][inverseFileIndexIfLefterThanMiddle(j)] - &oneArrayKPP[0];
		}
		else {
			// i, j 共に盤上
			const int ibegin = kppIndexBegin(i);
			const int jbegin = kppIndexBegin(j);
			const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
			const Piece jpiece = g_kppBoardIndexStartToPiece.value(jbegin);
			const Square isq = static_cast<Square>(i - ibegin);
			const Square jsq = static_cast<Square>(j - jbegin);
			const Rank krank = makeRank(ksq);
			const File kfile = makeFile(ksq);
			const Rank irank = makeRank(isq);
			const File ifile = makeFile(isq);
			const Rank jrank = makeRank(jsq);
			const File jfile = makeFile(jsq);
			File diff_file_ki = kfile - ifile;
			const bool kfile_ifile_is_inversed = (0 < diff_file_ki);
			if (kfile_ifile_is_inversed)
				diff_file_ki = -diff_file_ki;
			const File diff_file_kj =
				static_cast<File>(diff_file_ki == static_cast<File>(0) ? -abs(kfile - jfile) :
								  kfile_ifile_is_inversed              ? jfile - kfile       : kfile - jfile);
			ret[retIdx++] = &r_kpp_bb[ipiece][R_Mid + diff_file_ki][R_Mid + krank - irank][jpiece][R_Mid + diff_file_kj][R_Mid + krank - jrank] - &oneArrayKPP[0];
			ret[retIdx++] = &r_pp_bb[ipiece][jpiece][R_Mid + -abs(ifile - jfile)][R_Mid + irank - jrank] - &oneArrayKPP[0];

			if (ifile == FileE) {
				// ppに関してiが5筋なのでjだけ左右反転しても構わない。
				j = inverseFileIndexIfLefterThanMiddle(j);
				if (j < i) std::swap(i, j);
			}
			else if ((E1 < isq)
					 || (ibegin == jbegin && inverseFile(jsq) < isq))
			{
				// ppに関してiを左右反転するのでjも左右反転する。
				i = inverseFileIndexOnBoard(i);
				j = inverseFileIndexOnBoard(j);
				if (j < i) std::swap(i, j);
			}
			ret[retIdx++] = &pp[i][j] - &oneArrayKPP[0];
		}

		assert(*(std::end(ret)-1) == std::numeric_limits<ptrdiff_t>::max());
		return ret;
	}
	std::array<ptrdiff_t, 7> kkpIndices(Square ksq0, Square ksq1, int i) {
		std::array<ptrdiff_t, 7> ret;
		int retIdx = 0;
		// 無効なインデックスは最大値にしておく。
		std::fill(std::begin(ret), std::end(ret), std::numeric_limits<ptrdiff_t>::max());
		if (ksq0 == ksq1) return ret;
		auto kp_func = [this, &retIdx, &ret](Square ksq, int i, int sign) {
			auto r_kp_func = [this, &retIdx, &ret](Square ksq, int i, int sign) {
				if (i < fe_hand_end) {
					ret[retIdx++] = sign*(&r_kp_h[i] - &oneArrayKKP[0]);
				}
				else {
					const int ibegin = kppIndexBegin(i);
					const Square isq = static_cast<Square>(i - ibegin);
					const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
					ret[retIdx++] = sign*(&r_kp_b[ipiece][R_Mid + -abs(makeFile(ksq) - makeFile(isq))][R_Mid + makeRank(ksq) - makeRank(isq)] - &oneArrayKKP[0]);
				}
			};
			const int ibegin = kppIndexBegin(i);
			if (E1 < ksq) {
				ret[retIdx++] = sign*(&kp[inverseFile(ksq)][inverseFileIndexIfOnBoard(i)] - &oneArrayKKP[0]);
			}
			else if (makeFile(ksq) == FileE) {
				ret[retIdx++] = sign*(&kp[ksq][inverseFileIndexIfLefterThanMiddle(i)] - &oneArrayKKP[0]);
			}
			else {
				ret[retIdx++] = sign*(&kp[ksq][i] - &oneArrayKKP[0]);
			}
			r_kp_func(ksq, i, sign);
		};

		kp_func(ksq0, i, 1);
		{
			const int begin = kppIndexBegin(i);
			const int opp_begin = kppIndexToOpponentBegin(i);
			const int tmp_i = (begin < fe_hand_end ? opp_begin + (i - begin) : opp_begin + inverse(static_cast<Square>(i - begin)));
			kp_func(inverse(ksq1), tmp_i, -1);
		}

		int sign = 1;
		if (!kppIndexIsBlack(i)) {
			const Square tmp = ksq0;
			ksq0 = inverse(ksq1);
			ksq1 = inverse(tmp);
			const int ibegin = kppIndexBegin(i);
			const int opp_ibegin = kppWhiteIndexToBlackBegin(i);
			i = opp_ibegin + (i < fe_hand_end ? i - ibegin : inverse(static_cast<Square>(i - ibegin)));
			sign = -1;
		}
		if (E1 < ksq0) {
			ksq0 = inverseFile(ksq0);
			ksq1 = inverseFile(ksq1);
			i = inverseFileIndexIfOnBoard(i);
		}
		else if (makeFile(ksq0) == FileE && E1 < ksq1) {
			ksq1 = inverseFile(ksq1);
			i = inverseFileIndexIfOnBoard(i);
		}
		else if (makeFile(ksq0) == FileE && makeFile(ksq1) == FileE) {
			i = inverseFileIndexIfLefterThanMiddle(i);
		}
		ret[retIdx++] = sign*(&kkp[ksq0][ksq1][i] - &oneArrayKKP[0]);

		const Rank diff_rank_k0k1 = makeRank(ksq0) - makeRank(ksq1);
		File diff_file_k0k1 = makeFile(ksq0) - makeFile(ksq1);
		if (i < fe_hand_end) {
			if (0 < diff_file_k0k1)
				diff_file_k0k1 = -diff_file_k0k1;
			ret[retIdx++] = sign*(&r_kkp_h[R_Mid + diff_file_k0k1][R_Mid + diff_rank_k0k1][i] - &oneArrayKKP[0]);
		}
		else {
			const int ibegin = kppIndexBegin(i);
			const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
			Square isq = static_cast<Square>(i - ibegin);
			const Rank diff_rank_k0i = makeRank(ksq0) - makeRank(isq);
			File diff_file_k0i = makeFile(ksq0) - makeFile(isq);
			if (0 < diff_file_k0k1) {
				diff_file_k0k1 = -diff_file_k0k1;
				diff_file_k0i = -diff_file_k0i;
			}
			else if (0 == diff_file_k0k1 && 0 < diff_file_k0i) {
				diff_file_k0i = -diff_file_k0i;
			}
			ret[retIdx++] = sign*(&r_kkp_b[R_Mid + diff_file_k0k1][R_Mid + diff_rank_k0k1][ipiece][R_Mid + diff_file_k0i][R_Mid + diff_rank_k0i] - &oneArrayKKP[0]);
		}
		assert(*(std::end(ret)-1) == std::numeric_limits<ptrdiff_t>::max());
		return ret;
	}
	std::array<ptrdiff_t, 5> kkIndices(Square ksq0, Square ksq1) {
		std::array<ptrdiff_t, 5> ret;
		int retIdx = 0;
		// 無効なインデックスは最大値にしておく。
		std::fill(std::begin(ret), std::end(ret), std::numeric_limits<ptrdiff_t>::max());
		ret[retIdx++] = &k[std::min(ksq0, inverseFile(ksq0))] - &oneArrayKK[0];
		ret[retIdx++] = -(&k[std::min(inverse(ksq1), inverseFile(inverse(ksq1)))] - &oneArrayKK[0]);

		if (std::min(inverseFile(ksq0), inverse(inverseFile(ksq1))) < std::min(ksq0, inverse(ksq1))) {
			// inverseFile する。
			ksq0 = inverseFile(ksq0);
			ksq1 = inverseFile(ksq1);
		}
		if (ksq0 < inverse(ksq1)) {
			const File kfile0 = makeFile(ksq0);
			const Rank krank0 = makeRank(ksq0);
			const File kfile1 = makeFile(ksq1);
			const Rank krank1 = makeRank(ksq1);
			ret[retIdx++] = &kk[ksq0][ksq1] - &oneArrayKK[0];
			ret[retIdx++] = &r_kk[R_Mid + kfile0 - kfile1][R_Mid + krank0 - krank1] - &oneArrayKK[0];
			assert(ksq0 <= E1);
			assert(kfile0 - kfile1 <= 0);
		}
		else {
			// 常に ksq0 < ksq1 となるテーブルにアクセスする為、
			// ksq0, ksq1 を入れ替えて inverse する。
			const Square ksqInv0 = inverse(ksq1);
			const Square ksqInv1 = inverse(ksq0);
			const File kfileInv0 = makeFile(ksqInv0);
			const Rank krankInv0 = makeRank(ksqInv0);
			const File kfileInv1 = makeFile(ksqInv1);
			const Rank krankInv1 = makeRank(ksqInv1);
			ret[retIdx++] = -(&kk[ksqInv0][ksqInv1] - &oneArrayKK[0]);
			ret[retIdx++] = -(&r_kk[R_Mid + kfileInv0 - kfileInv1][R_Mid + krankInv0 - krankInv1] - &oneArrayKK[0]);
			assert(ksqInv0 <= E1);
			assert(kfileInv0 - kfileInv1 <= 0);
		}
		assert(*(std::end(ret)-1) == std::numeric_limits<ptrdiff_t>::max());
		return ret;
	}
	void clear() { memset(this, 0, sizeof(*this)); } // float 型とかだと規格的に 0 は保証されなかった気がするが実用上問題ないだろう。
	void init() {
		clear();
		read();
		setEvaluate();
	}
	void read() {
#define FOO(x) {														\
			std::ifstream ifs("" #x ".bin", std::ios::binary); \
			if (ifs) ifs.read(reinterpret_cast<char*>(x), sizeof(x));	\
		}

		FOO(kpp);
		FOO(r_kpp_bb);
		FOO(r_kpp_hb);
		FOO(pp);
		FOO(r_pp_bb);
		FOO(r_pp_hb);
		FOO(kp);
		FOO(kkp);
		FOO(r_kkp_b);
		FOO(r_kkp_h);
		FOO(r_kp_b);
		FOO(r_kp_h);
		FOO(kk);
		FOO(k);
		FOO(r_kk);

#undef FOO
	}
	void write() {
#define FOO(x) {														\
			std::ofstream ofs("" #x ".bin", std::ios::binary);			\
			if (ofs) ofs.write(reinterpret_cast<char*>(x), sizeof(x));	\
		}

		FOO(kpp);
		FOO(r_kpp_bb);
		FOO(r_kpp_hb);
		FOO(pp);
		FOO(r_pp_bb);
		FOO(r_pp_hb);
		FOO(kp);
		FOO(kkp);
		FOO(r_kkp_b);
		FOO(r_kkp_h);
		FOO(r_kp_b);
		FOO(r_kp_h);
		FOO(kk);
		FOO(k);
		FOO(r_kk);

#undef FOO
	}
	void setEvaluate() {
		// todo: 実行時に確認しているのはダサいのでコンパイル時にチェックすること。
		assert(sizeof(KPPType) == sizeof(KPP[0][0][0]));
		assert(sizeof(KKPType) == sizeof(KKP[0][0][0]));
		assert(sizeof(KKType ) == sizeof(KK [0][0]   ));

#define FOO(indices, oneArray, sum)										\
		for (auto index : indices) {									\
			if (index == std::numeric_limits<ptrdiff_t>::max()) break;	\
			if (0 <= index) sum += oneArray[ index];					\
			else            sum -= oneArray[-index];					\
		}

		// KPP
		for (Square ksq = I9; ksq < SquareNum; ++ksq) {
			for (int i = 0; i < fe_end; ++i) {
				for (int j = 0; j < fe_end; ++j) {
					auto indices = kppIndices(ksq, i, j);
					s32 sum = 0;
					FOO(indices, oneArrayKPP, sum);
					KPP[ksq][i][j] = sum;
				}
			}
		}
		// KKP
		for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
			for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
				for (int i = 0; i < fe_end; ++i) {
					auto indices = kkpIndices(ksq0, ksq1, i);
					s32 sum = 0;
					FOO(indices, oneArrayKKP, sum);
					KKP[ksq0][ksq1][i] = sum;
				}
			}
		}
		// KK
		for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
			for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
				auto indices = kkIndices(ksq0, ksq1);
				s32 sum = 0;
				FOO(indices, oneArrayKK, sum);
				KK[ksq0][ksq1] = sum;
#if defined USE_K_FIX_OFFSET
				KK[ksq0][ksq1] += K_Fix_Offset[ksq0] - K_Fix_Offset[inverse(ksq1)];
#endif
			}
		}
#undef FOO
	}
	// 学習用の型以外でこれは使わないこと。
	void incParam(const Position& pos, const double dinc);
};

extern const int kppArray[31];
extern const int kkpArray[15];
extern const int kppHandArray[ColorNum][HandPieceNum];

class Position;
struct SearchStack;

const size_t EvaluateTableSize = 0x1000000; // 134MB
//const size_t EvaluateTableSize = 0x80000000; // 17GB

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
