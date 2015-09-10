#ifndef APERY_EVALUATE_HPP
#define APERY_EVALUATE_HPP

#include "overloadEnumOperators.hpp"
#include "common.hpp"
#include "square.hpp"
#include "piece.hpp"
#include "pieceScore.hpp"
#include "position.hpp"

// 評価関数テーブルのオフセット。
// f_xxx が味方の駒、e_xxx が敵の駒
// Bonanza の影響で持ち駒 0 の場合のインデックスが存在するが、参照する事は無い。
// todo: 持ち駒 0 の位置を詰めてテーブルを少しでも小さくする。(キャッシュに少しは乗りやすい?)
enum {
	f_hand_pawn   = 0, // 0
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

const int KPPIndexArray[] = {
	f_hand_pawn, e_hand_pawn, f_hand_lance, e_hand_lance, f_hand_knight,
	e_hand_knight, f_hand_silver, e_hand_silver, f_hand_gold, e_hand_gold,
	f_hand_bishop, e_hand_bishop, f_hand_rook, e_hand_rook, /*fe_hand_end,*/
	f_pawn, e_pawn, f_lance, e_lance, f_knight, e_knight, f_silver, e_silver,
	f_gold, e_gold, f_bishop, e_bishop, f_horse, e_horse, f_rook, e_rook,
	f_dragon, e_dragon, fe_end
};

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

template <typename KPPType, typename KKPType, typename KKType> struct EvaluaterBase {
	static const int R_Mid = 8; // 相対位置の中心のindex
	constexpr int MaxWeight() const { return 1 << 20; } // KPE自体が1/8の寄与。更にKPEの遠隔駒の利きが1マスごとに1/2に減衰する分(最大でKEEの際に8マス離れが2枚だから 1/128**2)
														// それだけで 1 << 18 は必要。更に重みを下げる場合、MaxWeightを更に大きくしておく必要がある。
														// なぜか clang で static const int MaxWeight を使っても Undefined symbols for architecture x86_64 と言われる。
	union {
		// 冗長に配列を確保しているが、対称な関係にある時は常に若いindexの方にアクセスすることにする。
		// 例えば kpp だったら、k が優先的に小さくなるようする。左右の対称も含めてアクセス位置を決める。
		// ただし、kkp に関する項目 (kkp, r_kkp_b, r_kkp_h) のみ、p は味方の駒として扱うので、k0 < k1 となるとは限らない。
		struct {
			KPPType kpp[SquareNum][fe_end][fe_end];
			// 相対位置は[file][rank]の順
			KPPType r_kpp_bb[PieceNone][17][17][PieceNone][17][17];
			KPPType r_kpp_hb[fe_hand_end][PieceNone][17][17];
			KPPType xpp[FileNum][fe_end][fe_end];
			KPPType ypp[RankNum][fe_end][fe_end];
			KPPType pp[fe_end][fe_end];
			KPPType r_pp_bb[PieceNone][PieceNone][17][17];
			KPPType r_pp_hb[fe_hand_end][PieceNone];

			// e は Effect の頭文字で利きを表す。(Control = 利き という説もあり。)
			// todo: 玉の利きは全く無視しているけれど、それで良いのか？
			KPPType kpe[SquareNum][fe_end][ColorNum][SquareNum];
			KPPType kee[SquareNum][ColorNum][SquareNum][ColorNum][SquareNum];
			KPPType r_kpe_b[PieceNone][17][17][ColorNum][17][17];
			KPPType r_kpe_h[fe_hand_end][ColorNum][17][17];
			KPPType r_kee[ColorNum][17][17][ColorNum][17][17];
			KPPType xpe[FileNum][fe_end][ColorNum][SquareNum];
			KPPType xee[FileNum][ColorNum][SquareNum][ColorNum][SquareNum];
			KPPType ype[RankNum][fe_end][ColorNum][SquareNum];
			KPPType yee[RankNum][ColorNum][SquareNum][ColorNum][SquareNum];
			KPPType pe[fe_end][ColorNum][SquareNum];
			KPPType ee[ColorNum][SquareNum][ColorNum][SquareNum];
			KPPType r_pe_b[PieceNone][ColorNum][17][17];
			KPPType r_pe_h[fe_hand_end][ColorNum];
			KPPType r_ee[ColorNum][ColorNum][17][17];

			KKPType kkp[SquareNum][SquareNum][fe_end];
			KKPType kp[SquareNum][fe_end];
			KKPType r_kkp_b[17][17][PieceNone][17][17];
			KKPType r_kkp_h[17][17][fe_hand_end];
			KKPType r_kp_b[PieceNone][17][17];
			KKPType r_kp_h[fe_hand_end];

			KKPType kke[SquareNum][SquareNum][ColorNum][SquareNum];
			KKPType ke[SquareNum][ColorNum][SquareNum];
			KKPType r_kke[17][17][ColorNum][17][17];
			KKPType r_ke[ColorNum][17][17];

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

	// todo: これらややこしいし汚いので使わないようにする。
	//       型によっては kkps_begin_index などの値が異なる。
	//       ただ、end - begin のサイズは型によらず一定。
	size_t kpps_begin_index() const { return &kpp[0][0][0] - &oneArrayKPP[0]; }
	size_t kpps_end_index() const { return kpps_begin_index() + (sizeof(kpp)+sizeof(r_kpp_bb)+sizeof(r_kpp_hb)+sizeof(xpp)+sizeof(ypp)+sizeof(pp)+sizeof(r_pp_bb)+sizeof(r_pp_hb)+sizeof(kpe)+sizeof(kee)+sizeof(r_kpe_b)+sizeof(r_kpe_h)+sizeof(r_kee)+sizeof(xpe)+sizeof(xee)+sizeof(ype)+sizeof(yee)+sizeof(pe)+sizeof(ee)+sizeof(r_pe_b)+sizeof(r_pe_h)+sizeof(r_ee))/sizeof(KPPType); }
	size_t kkps_begin_index() const { return &kkp[0][0][0] - &oneArrayKKP[0]; }
	size_t kkps_end_index() const { return kkps_begin_index() + (sizeof(kkp)+sizeof(kp)+sizeof(r_kkp_b)+sizeof(r_kkp_h)+sizeof(r_kp_b)+sizeof(r_kp_h)+sizeof(kke)+sizeof(ke)+sizeof(r_kke)+sizeof(r_ke))/sizeof(KKPType); }
	size_t kks_begin_index() const { return &kk[0][0] - &oneArrayKK[0]; }
	size_t kks_end_index() const { return kks_begin_index() + (sizeof(kk)+sizeof(k)+sizeof(r_kk))/sizeof(KKType); }

	static const int KPPIndicesMax = 3000;
	static const int KKPIndicesMax = 130;
	static const int KKIndicesMax = 7;
	// KPP に関する相対位置などの次元を落とした位置などのインデックスを全て返す。
	// 負のインデックスは、正のインデックスに変換した位置の点数を引く事を意味する。
	// 0 の時だけは正負が不明だが、0 は歩の持ち駒 0 枚を意味していて無効な値なので問題なし。
	// ptrdiff_t はインデックス、int は寄与の大きさ。MaxWeight分のいくつかで表記することにする。
	void kppIndices(std::pair<ptrdiff_t, int> ret[KPPIndicesMax], Square ksq, int i, int j) {
		int retIdx = 0;
		// i == j のKP要素はKKPの方で行うので、こちらでは何も有効なindexを返さない。
		if (i == j) {
			ret[retIdx++] = std::make_pair(std::numeric_limits<ptrdiff_t>::max(), MaxWeight());
			assert(retIdx <= KPPIndicesMax);
			return;
		}
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

		ret[retIdx++] = std::make_pair(&kpp[ksq][i][j] - &oneArrayKPP[0], MaxWeight());
		ret[retIdx++] = std::make_pair(&xpp[makeFile(ksq)][i][j] - &oneArrayKPP[0], MaxWeight());

		assert(i < j);
		if (j < fe_hand_end) {
			// i, j 共に持ち駒
			// 相対位置無し。
			ret[retIdx++] = std::make_pair(&pp[i][j] - &oneArrayKPP[0], MaxWeight());
			ret[retIdx++] = std::make_pair(&ypp[makeRank(ksq)][i][j] - &oneArrayKPP[0], MaxWeight());
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
			ret[retIdx++] = std::make_pair(&r_kpp_hb[i][jpiece][R_Mid + -abs(kfile - jfile)][R_Mid + krank - jrank] - &oneArrayKPP[0], MaxWeight());
			ret[retIdx++] = std::make_pair(&r_pp_hb[i][jpiece] - &oneArrayKPP[0], MaxWeight());

			ret[retIdx++] = std::make_pair(&pp[i][inverseFileIndexIfLefterThanMiddle(j)] - &oneArrayKPP[0], MaxWeight());
			ret[retIdx++] = std::make_pair(&ypp[krank][i][inverseFileIndexIfLefterThanMiddle(j)] - &oneArrayKPP[0], MaxWeight());

			const Color jcolor = pieceToColor(jpiece);
			const PieceType jpt = pieceToPieceType(jpiece);
			Bitboard jtoBB = setMaskBB(ksq).notThisAnd(Position::attacksFrom(jpt, jcolor, jsq, setMaskBB(ksq)));
			while (jtoBB.isNot0()) {
				Square jto = jtoBB.firstOneFromI9();
				if (kfile == FileE && E1 < jto)
					jto = inverseFile(jto);
				const int distance = squareDistance(jsq, jto);
				// distance == 1 で 1/8 で 3bit シフトにする程度の寄与にする。
				ret[retIdx++] = std::make_pair(&kpe[ksq][i][jcolor][jto] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				ret[retIdx++] = std::make_pair(&xpe[kfile][i][jcolor][jto] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				const Rank jtorank = makeRank(jto);
				const File jtofile = makeFile(jto);
				ret[retIdx++] = std::make_pair(&r_kpe_h[i][jcolor][R_Mid + -abs(kfile - jtofile)][R_Mid + krank - jtorank] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				ret[retIdx++] = std::make_pair(&r_pe_h[i][jcolor] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				ret[retIdx++] = std::make_pair(&pe[i][jcolor][jto] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				if (E1 < jto)
					jto = inverseFile(jto);
				ret[retIdx++] = std::make_pair(&ype[krank][i][jcolor][jto] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
			}
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
			bool kfile_ifile_is_inversed = false;
			if (0 < diff_file_ki) {
				diff_file_ki = -diff_file_ki;
				kfile_ifile_is_inversed = true;
			}
			const File diff_file_kj =
				static_cast<File>(diff_file_ki == static_cast<File>(0) ? -abs(kfile - jfile) :
								  kfile_ifile_is_inversed              ? jfile - kfile       : kfile - jfile);
			if (ipiece == jpiece) {
				if (diff_file_kj < diff_file_ki || (diff_file_kj == diff_file_ki && -jrank < -irank))
					ret[retIdx++] = std::make_pair(&r_kpp_bb[jpiece][R_Mid + diff_file_kj][R_Mid + krank - jrank][ipiece][R_Mid + diff_file_ki][R_Mid + krank - irank] - &oneArrayKPP[0], MaxWeight());
				else
					ret[retIdx++] = std::make_pair(&r_kpp_bb[ipiece][R_Mid + diff_file_ki][R_Mid + krank - irank][jpiece][R_Mid + diff_file_kj][R_Mid + krank - jrank] - &oneArrayKPP[0], MaxWeight());
				// 同じ駒の種類の時は、2駒の相対関係は上下がどちらになっても同じ点数であるべき。
				ret[retIdx++] = std::make_pair(&r_pp_bb[ipiece][jpiece][R_Mid + -abs(ifile - jfile)][R_Mid + -abs(irank - jrank)] - &oneArrayKPP[0], MaxWeight());
			}
			else {
				ret[retIdx++] = std::make_pair(&r_kpp_bb[ipiece][R_Mid + diff_file_ki][R_Mid + krank - irank][jpiece][R_Mid + diff_file_kj][R_Mid + krank - jrank] - &oneArrayKPP[0], MaxWeight());
				ret[retIdx++] = std::make_pair(&r_pp_bb[ipiece][jpiece][R_Mid + -abs(ifile - jfile)][R_Mid + irank - jrank] - &oneArrayKPP[0], MaxWeight());
			}

			auto func = [this, &retIdx, &ret](Square ksq, int ij, int ji) {
				const Rank krank = makeRank(ksq);
				const File kfile = makeFile(ksq);
				const int ijbegin = kppIndexBegin(ij);
				const int jibegin = kppIndexBegin(ji);
				const Piece ijpiece = g_kppBoardIndexStartToPiece.value(ijbegin);
				const Piece jipiece = g_kppBoardIndexStartToPiece.value(jibegin);
				const Square ijsq = static_cast<Square>(ij - ijbegin);
				const Square jisq = static_cast<Square>(ji - jibegin);

				const Color jicolor = pieceToColor(jipiece);
				const PieceType jipt = pieceToPieceType(jipiece);
				const Bitboard mask = setMaskBB(ksq) | setMaskBB(ijsq);
				Bitboard jitoBB = mask.notThisAnd(Position::attacksFrom(jipt, jicolor, jisq, mask));
				while (jitoBB.isNot0()) {
					Square jito = jitoBB.firstOneFromI9();
					Square ijsq_tmp = ijsq;
					assert(ksq <= E1);
					if (makeFile(ksq) == FileE) {
						if (E1 < ijsq_tmp) {
							ij = inverseFileIndexOnBoard(ij);
							ijsq_tmp = inverseFile(ijsq_tmp);
							jito = inverseFile(jito);
						}
						else if (makeFile(ijsq_tmp) == FileE)
							jito = inverseFile(jito);
					}
					const Rank ijrank = makeRank(ijsq_tmp);
					const File ijfile = makeFile(ijsq_tmp);
					const int distance = squareDistance(jisq, jito);
					ret[retIdx++] = std::make_pair(&kpe[ksq][ij][jicolor][jito] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
					ret[retIdx++] = std::make_pair(&xpe[makeFile(ksq)][ij][jicolor][jito] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
					const Rank jitorank = makeRank(jito);
					const File jitofile = makeFile(jito);
					{
						int ij_tmp = ij;
						int jito_tmp = jito;
						if (FileE < ijfile) {
							ij_tmp = inverseFileIndexOnBoard(ij_tmp);
							jito_tmp = inverseFile(jito);
						}
						else if (FileE == ijfile && FileE < jitofile)
							jito_tmp = inverseFile(jito);

						ret[retIdx++] = std::make_pair(&ype[makeRank(ksq)][ij_tmp][jicolor][jito_tmp] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
					}

					File diff_file_kij = kfile - ijfile;
					bool kfile_ijfile_is_inversed = false;
					if (0 < diff_file_kij) {
						diff_file_kij = -diff_file_kij;
						kfile_ijfile_is_inversed = true;
					}
					const File diff_file_kjito =
						static_cast<File>(diff_file_kij == static_cast<File>(0) ? -abs(kfile - jitofile) :
										  kfile_ijfile_is_inversed              ? jitofile - kfile       : kfile - jitofile);
					ret[retIdx++] = std::make_pair(&r_kpe_b[ijpiece][R_Mid + diff_file_kij][R_Mid + krank - ijrank][jicolor][R_Mid + diff_file_kjito][R_Mid + krank - jitorank] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
					ret[retIdx++] = std::make_pair(&r_pe_b[ijpiece][jicolor][R_Mid + -abs(ijfile - jitofile)][R_Mid + ijrank - jitorank] - &oneArrayKPP[0], MaxWeight() >> (distance+4));

					int ij_tmp = ij;
					if (FileE < ijfile) {
						ij_tmp = inverseFileIndexOnBoard(ij_tmp);
						jito = inverseFile(jito);
					}
					else if (FileE == ijfile && E1 < jito) {
						jito = inverseFile(jito);
					}
					ret[retIdx++] = std::make_pair(&pe[ij_tmp][jicolor][jito] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
				}
			};
			func(ksq, i, j);
			func(ksq, j, i);
			auto ee_func = [this, &retIdx, &ret](Square ksq, int i, int j) {
				assert(ksq <= E1);
				const Rank krank = makeRank(ksq);
				const File kfile = makeFile(ksq);
				auto color = [](int ij) {
					const int ijbegin = kppIndexBegin(ij);
					const Piece ijpiece = g_kppBoardIndexStartToPiece.value(ijbegin);
					const Color ijcolor = pieceToColor(ijpiece);
					return ijcolor;
				};
				if (color(j) < color(i))
					std::swap(i, j);
				const int ibegin = kppIndexBegin(i);
				const int jbegin = kppIndexBegin(j);
				const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
				const Piece jpiece = g_kppBoardIndexStartToPiece.value(jbegin);
				const Square isq = static_cast<Square>(i - ibegin);
				const Square jsq = static_cast<Square>(j - jbegin);

				const Color icolor = pieceToColor(ipiece);
				const Color jcolor = pieceToColor(jpiece);
				const PieceType ipt = pieceToPieceType(ipiece);
				const PieceType jpt = pieceToPieceType(jpiece);
				const Bitboard imask = setMaskBB(ksq) | setMaskBB(jsq);
				const Bitboard jmask = setMaskBB(ksq) | setMaskBB(isq);
				Bitboard itoBB = imask.notThisAnd(Position::attacksFrom(jpt, icolor, isq, imask));
				Bitboard jtoBB = jmask.notThisAnd(Position::attacksFrom(jpt, jcolor, jsq, jmask));
				while (itoBB.isNot0()) {
					const Square ito = itoBB.firstOneFromI9();
					const int itodistance = squareDistance(isq, ito);
					Bitboard jtoBB_tmp = jtoBB;
					while (jtoBB_tmp.isNot0()) {
						const Square jto = jtoBB_tmp.firstOneFromI9();
						const int jtodistance = squareDistance(jsq, jto);
						const int distance = itodistance + jtodistance - 1;
						{
							Square ito_tmp = ito;
							Square jto_tmp = jto;
							if (kfile == FileE) {
								if (icolor == jcolor) {
									if (std::min(inverseFile(ito_tmp), inverseFile(jto_tmp)) < std::min(ito_tmp, jto_tmp)) {
										ito_tmp = inverseFile(ito_tmp);
										jto_tmp = inverseFile(jto_tmp);
									}
									if (jto_tmp < ito_tmp)
										std::swap(ito_tmp, jto_tmp);
								}
								else {
									if (E1 < ito_tmp) {
										ito_tmp = inverseFile(ito_tmp);
										jto_tmp = inverseFile(jto_tmp);
									}
									else if (makeFile(ito_tmp) == FileE && E1 < jto_tmp)
										jto_tmp = inverseFile(jto_tmp);
								}
							}
							else if (icolor == jcolor && jto_tmp < ito_tmp)
								std::swap(ito_tmp, jto_tmp);
							ret[retIdx++] = std::make_pair(&kee[ksq][icolor][ito_tmp][jcolor][jto_tmp] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
							ret[retIdx++] = std::make_pair(&xee[kfile][icolor][ito_tmp][jcolor][jto_tmp] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
							File diff_file_kito = kfile - makeFile(ito_tmp);
							bool kfile_itofile_is_inversed = false;
							if (0 < diff_file_kito) {
								diff_file_kito = -diff_file_kito;
								kfile_itofile_is_inversed = true;
							}
							File diff_file_kjto =
								static_cast<File>(diff_file_kito == static_cast<File>(0) ? -abs(kfile - makeFile(jto_tmp)) :
												  kfile_itofile_is_inversed              ? makeFile(jto_tmp) - kfile       : kfile - makeFile(jto_tmp));
							Rank diff_rank_kito = krank - makeRank(ito_tmp);
							Rank diff_rank_kjto = krank - makeRank(jto_tmp);
							std::tuple<Color, File, Rank> ituple = std::make_tuple(icolor, diff_file_kito, diff_rank_kito);
							std::tuple<Color, File, Rank> jtuple = std::make_tuple(jcolor, diff_file_kjto, diff_rank_kjto);
							if (jtuple < ituple)
								std::swap(ituple, jtuple);
							ret[retIdx++] = std::make_pair(&r_kee[std::get<0>(ituple)][R_Mid + std::get<1>(ituple)][R_Mid + std::get<2>(ituple)][std::get<0>(jtuple)][R_Mid + std::get<1>(jtuple)][R_Mid + std::get<2>(jtuple)] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
						}
						Square ito_tmp = ito;
						Square jto_tmp = jto;
						if (icolor == jcolor) {
							if (std::min(inverseFile(ito_tmp), inverseFile(jto_tmp)) < std::min(ito_tmp, jto_tmp)) {
								ito_tmp = inverseFile(ito_tmp);
								jto_tmp = inverseFile(jto_tmp);
							}
							if (jto_tmp < ito_tmp)
								std::swap(ito_tmp, jto_tmp);
						}
						else {
							if (E1 < ito_tmp) {
								ito_tmp = inverseFile(ito_tmp);
								jto_tmp = inverseFile(jto_tmp);
							}
							else if (makeFile(ito_tmp) == FileE && E1 < jto_tmp)
								jto_tmp = inverseFile(jto_tmp);
						}
						ret[retIdx++] = std::make_pair(&ee[icolor][ito_tmp][jcolor][jto_tmp] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
						ret[retIdx++] = std::make_pair(&yee[krank][icolor][ito_tmp][jcolor][jto_tmp] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
						const File itofile = makeFile(ito_tmp);
						const Rank itorank = makeRank(ito_tmp);
						const File jtofile = makeFile(jto_tmp);
						const Rank jtorank = makeRank(jto_tmp);
						ret[retIdx++] = std::make_pair(&r_ee[icolor][jcolor][R_Mid + abs(-itofile - jtofile)][R_Mid + itorank - jtorank] - &oneArrayKPP[0], MaxWeight() >> (distance+4));
					}
				}
			};
			ee_func(ksq, i, j);

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
			ret[retIdx++] = std::make_pair(&pp[i][j] - &oneArrayKPP[0], MaxWeight());
			ret[retIdx++] = std::make_pair(&ypp[krank][i][j] - &oneArrayKPP[0], MaxWeight());
		}

		ret[retIdx++] = std::make_pair(std::numeric_limits<ptrdiff_t>::max(), MaxWeight());
		assert(retIdx <= KPPIndicesMax);
	}
	void kkpIndices(std::pair<ptrdiff_t, int> ret[KKPIndicesMax], Square ksq0, Square ksq1, int i) {
		int retIdx = 0;
		if (ksq0 == ksq1) {
			ret[retIdx++] = std::make_pair(std::numeric_limits<ptrdiff_t>::max(), MaxWeight());
			assert(retIdx <= KKPIndicesMax);
			return;
		}
		auto kp_func = [this, &retIdx, &ret](Square ksq, int i, int sign) {
			if (E1 < ksq) {
				ksq = inverseFile(ksq);
				i = inverseFileIndexIfOnBoard(i);
			}
			else if (makeFile(ksq) == FileE)
				i = inverseFileIndexIfLefterThanMiddle(i);
			ret[retIdx++] = std::make_pair(sign*(&kp[ksq][i] - &oneArrayKKP[0]), MaxWeight());
			auto r_kp_func = [this, &retIdx, &ret](Square ksq, int i, int sign) {
				if (i < fe_hand_end) {
					ret[retIdx++] = std::make_pair(sign*(&r_kp_h[i] - &oneArrayKKP[0]), MaxWeight());
				}
				else {
					const int ibegin = kppIndexBegin(i);
					const Square isq = static_cast<Square>(i - ibegin);
					const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
					ret[retIdx++] = std::make_pair(sign*(&r_kp_b[ipiece][R_Mid + -abs(makeFile(ksq) - makeFile(isq))][R_Mid + makeRank(ksq) - makeRank(isq)] - &oneArrayKKP[0]), MaxWeight());

					const PieceType ipt = pieceToPieceType(ipiece);
					const Color icolor = pieceToColor(ipiece);
					Bitboard itoBB = setMaskBB(ksq).notThisAnd(Position::attacksFrom(ipt, icolor, isq, setMaskBB(ksq)));
					while (itoBB.isNot0()) {
						Square ito = itoBB.firstOneFromI9();
						const int distance = squareDistance(isq, ito);
						ret[retIdx++] = std::make_pair(sign*(&r_ke[icolor][R_Mid + -abs(makeFile(ksq) - makeFile(ito))][R_Mid + makeRank(ksq) - makeRank(ito)] - &oneArrayKKP[0]), MaxWeight() >> (distance+4));
					}
				}
			};
			r_kp_func(ksq, i, sign);
			if (f_pawn <= i) {
				const int ibegin = kppIndexBegin(i);
				const Square isq = static_cast<Square>(i - ibegin);
				const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
				const PieceType ipt = pieceToPieceType(ipiece);
				const Color icolor = pieceToColor(ipiece);

				Bitboard itoBB = setMaskBB(ksq).notThisAnd(Position::attacksFrom(ipt, icolor, isq, setMaskBB(ksq)));
				while (itoBB.isNot0()) {
					Square ito = itoBB.firstOneFromI9();
					const int distance = squareDistance(isq, ito);
					if (makeFile(ksq) == FileE && E1 < ito)
						ito = inverseFile(ito);
					ret[retIdx++] = std::make_pair(sign*(&ke[ksq][icolor][ito] - &oneArrayKKP[0]), MaxWeight() >> (distance+4));
				}
			}
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
		ret[retIdx++] = std::make_pair(sign*(&kkp[ksq0][ksq1][i] - &oneArrayKKP[0]), MaxWeight());

		const Rank diff_rank_k0k1 = makeRank(ksq0) - makeRank(ksq1);
		File diff_file_k0k1 = makeFile(ksq0) - makeFile(ksq1);
		if (i < fe_hand_end) {
			if (0 < diff_file_k0k1)
				diff_file_k0k1 = -diff_file_k0k1;
			ret[retIdx++] = std::make_pair(sign*(&r_kkp_h[R_Mid + diff_file_k0k1][R_Mid + diff_rank_k0k1][i] - &oneArrayKKP[0]), MaxWeight());
		}
		else {
			const int ibegin = kppIndexBegin(i);
			const Piece ipiece = g_kppBoardIndexStartToPiece.value(ibegin);
			Square isq = static_cast<Square>(i - ibegin);
			const Rank diff_rank_k0i = makeRank(ksq0) - makeRank(isq);
			File diff_file_k0i = makeFile(ksq0) - makeFile(isq);

			const Color icolor = pieceToColor(ipiece);
			const PieceType ipt = pieceToPieceType(ipiece);
			const Bitboard mask = setMaskBB(ksq0) | setMaskBB(ksq1);
			Bitboard itoBB = mask.notThisAnd(Position::attacksFrom(ipt, icolor, isq, mask));
			while (itoBB.isNot0()) {
				Square ito = itoBB.firstOneFromI9();
				const int distance = squareDistance(isq, ito);
				if (makeFile(ksq0) == FileE && makeFile(ksq1) == FileE && E1 < ito)
					ito = inverseFile(ito);
				ret[retIdx++] = std::make_pair(sign*(&kke[ksq0][ksq1][icolor][ito] - &oneArrayKKP[0]), MaxWeight() >> (distance+4));
				File diff_file_k0k1_tmp = diff_file_k0k1;
				File diff_file_k0ito = makeFile(ksq0) - makeFile(ito);
				Rank diff_rank_k0ito = makeRank(ksq0) - makeRank(ito);
				if (0 < diff_file_k0k1_tmp) {
					diff_file_k0k1_tmp = -diff_file_k0k1_tmp;
					diff_file_k0ito = -diff_file_k0ito;
				}
				else if (0 == diff_file_k0k1_tmp && 0 < diff_file_k0ito)
					diff_file_k0ito = -diff_file_k0ito;
				ret[retIdx++] = std::make_pair(sign*(&r_kke[R_Mid + diff_file_k0k1_tmp][R_Mid + diff_rank_k0k1][icolor][R_Mid + diff_file_k0ito][R_Mid + diff_rank_k0ito] - &oneArrayKKP[0]), MaxWeight() >> (distance+4));
			}

			if (0 < diff_file_k0k1) {
				diff_file_k0k1 = -diff_file_k0k1;
				diff_file_k0i = -diff_file_k0i;
			}
			else if (0 == diff_file_k0k1 && 0 < diff_file_k0i) {
				diff_file_k0i = -diff_file_k0i;
			}
			ret[retIdx++] = std::make_pair(sign*(&r_kkp_b[R_Mid + diff_file_k0k1][R_Mid + diff_rank_k0k1][ipiece][R_Mid + diff_file_k0i][R_Mid + diff_rank_k0i] - &oneArrayKKP[0]), MaxWeight());
		}
		ret[retIdx++] = std::make_pair(std::numeric_limits<ptrdiff_t>::max(), MaxWeight());
		assert(retIdx <= KKPIndicesMax);
	}
	void kkIndices(std::pair<ptrdiff_t, int> ret[KKIndicesMax], Square ksq0, Square ksq1) {
		int retIdx = 0;
		ret[retIdx++] = std::make_pair(&k[std::min(ksq0, inverseFile(ksq0))] - &oneArrayKK[0], MaxWeight());
		ret[retIdx++] = std::make_pair(-(&k[std::min(inverse(ksq1), inverseFile(inverse(ksq1)))] - &oneArrayKK[0]), MaxWeight());

		auto kk_func = [this, &retIdx, &ret](Square ksq0, Square ksq1, int sign) {
			{
				// 常に ksq0 < ksq1 となるテーブルにアクセスする為の変換
				const Square ksq0Arr[] = {
					ksq0,
					inverseFile(ksq0),
				};
				const Square ksq1Arr[] = {
					inverse(ksq1),
					inverse(inverseFile(ksq1)),
				};
				auto ksq0ArrIdx = std::min_element(std::begin(ksq0Arr), std::end(ksq0Arr)) - std::begin(ksq0Arr);
				auto ksq1ArrIdx = std::min_element(std::begin(ksq1Arr), std::end(ksq1Arr)) - std::begin(ksq1Arr);
				if (ksq0Arr[ksq0ArrIdx] <= ksq1Arr[ksq1ArrIdx]) {
					ksq0 = ksq0Arr[ksq0ArrIdx];
					ksq1 = inverse(ksq1Arr[ksq0ArrIdx]);
				}
				else {
					sign = -sign; // ksq0 と ksq1 を入れ替えるので符号反転
					ksq0 = ksq1Arr[ksq1ArrIdx];
					ksq1 = inverse(ksq0Arr[ksq1ArrIdx]);
				}
			}
			const File kfile0 = makeFile(ksq0);
			const Rank krank0 = makeRank(ksq0);
			const File kfile1 = makeFile(ksq1);
			const Rank krank1 = makeRank(ksq1);
			ret[retIdx++] = std::make_pair(sign*(&kk[ksq0][ksq1] - &oneArrayKK[0]), MaxWeight());
			ret[retIdx++] = std::make_pair(sign*(&r_kk[R_Mid + kfile0 - kfile1][R_Mid + krank0 - krank1] - &oneArrayKK[0]), MaxWeight());
			assert(ksq0 <= E1);
			assert(kfile0 - kfile1 <= 0);
		};
		kk_func(ksq0         , ksq1         ,  1);
		kk_func(inverse(ksq1), inverse(ksq0), -1);
		ret[retIdx++] = std::make_pair(std::numeric_limits<ptrdiff_t>::max(), MaxWeight());
		assert(retIdx <= KKIndicesMax);
	}
};

struct Evaluater : public EvaluaterBase<s16, s32, s32> {
	// 探索時に参照する評価関数テーブル
	static s16 KPP[SquareNum][fe_end][fe_end];
	static s32 KKP[SquareNum][SquareNum][fe_end];
	static s32 KK[SquareNum][SquareNum];
#if defined USE_K_FIX_OFFSET
	static const s32 K_Fix_Offset[SquareNum];
#endif

	void clear() { memset(this, 0, sizeof(*this)); }
	static std::string addSlashIfNone(const std::string& str) {
		std::string ret = str;
		if (ret == "")
			ret += ".";
		if (ret.back() != '/')
			ret += "/";
		return ret;
	}
	void init(const std::string& dirName, const bool Synthesized) {
		// 合成された評価関数バイナリがあればそちらを使う。
		if (Synthesized) {
			if (readSynthesized(dirName))
				return;
		}
		clear();
		read(dirName);
		setEvaluate();
	}
#define ALL_SYNTHESIZED_EVAL {									\
		FOO(KPP);												\
		FOO(KKP);												\
		FOO(KK);												\
	}
	static bool readSynthesized(const std::string& dirName) {
#define FOO(x) {														\
			std::ifstream ifs((addSlashIfNone(dirName) + #x "_synthesized.bin").c_str(), std::ios::binary); \
			if (ifs) ifs.read(reinterpret_cast<char*>(x), sizeof(x));	\
			else     return false;										\
		}
		ALL_SYNTHESIZED_EVAL;
#undef FOO
		return true;
	}
	static void writeSynthesized(const std::string& dirName) {
#define FOO(x) {														\
			std::ofstream ofs((addSlashIfNone(dirName) + #x "_synthesized.bin").c_str(), std::ios::binary); \
			ofs.write(reinterpret_cast<char*>(x), sizeof(x));			\
		}
		ALL_SYNTHESIZED_EVAL;
#undef FOO
	}
#undef ALL_SYNTHESIZED_EVAL
#define ALL_BASE_EVAL {							\
		FOO(kpp);								\
		FOO(r_kpp_bb);							\
		FOO(r_kpp_hb);							\
		FOO(xpp);								\
		FOO(ypp);								\
		FOO(pp);								\
		FOO(r_pp_bb);							\
		FOO(r_pp_hb);							\
		FOO(kpe);								\
		FOO(kee);								\
		FOO(r_kpe_b);							\
		FOO(r_kpe_h);							\
		FOO(r_kee);								\
		FOO(xpe);								\
		FOO(xee);								\
		FOO(ype);								\
		FOO(yee);								\
		FOO(pe);								\
		FOO(ee);								\
		FOO(r_pe_b);							\
		FOO(r_pe_h);							\
		FOO(r_ee);								\
		FOO(kkp);								\
		FOO(kp);								\
		FOO(r_kkp_b);							\
		FOO(r_kkp_h);							\
		FOO(r_kp_b);							\
		FOO(r_kp_h);							\
		FOO(kke);								\
		FOO(ke);								\
		FOO(r_kke);								\
		FOO(r_ke);								\
		FOO(kk);								\
		FOO(k);									\
		FOO(r_kk);								\
	}
	void read(const std::string& dirName) {
#define FOO(x) {														\
			std::ifstream ifs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
			ifs.read(reinterpret_cast<char*>(x), sizeof(x));			\
		}
		ALL_BASE_EVAL;
#undef FOO
	}
	void write(const std::string& dirName) {
#define FOO(x) {														\
			std::ofstream ofs((addSlashIfNone(dirName) + #x ".bin").c_str(), std::ios::binary); \
			ofs.write(reinterpret_cast<char*>(x), sizeof(x));			\
		}
		ALL_BASE_EVAL;
#undef FOO
	}
#undef ALL_BASE_EVAL
	void setEvaluate() {
#if !defined LEARN
		SYNCCOUT << "info string start setting eval table" << SYNCENDL;
#endif
#define FOO(indices, oneArray, sum)										\
		for (auto indexAndWeight : indices) {							\
			if (indexAndWeight.first == std::numeric_limits<ptrdiff_t>::max()) break; \
			if (0 <= indexAndWeight.first) sum += static_cast<s64>(oneArray[ indexAndWeight.first]) * indexAndWeight.second; \
			else                           sum -= static_cast<s64>(oneArray[-indexAndWeight.first]) * indexAndWeight.second; \
		}																\
		sum /= MaxWeight();

#if defined _OPENMP
#pragma omp parallel
#endif
		// KPP
		{
#ifdef _OPENMP
#pragma omp for
#endif
			// OpenMP対応したら何故か ksq を Square 型にすると ++ksq が定義されていなくてコンパイルエラーになる。
			for (int ksq = I9; ksq < SquareNum; ++ksq) {
				// indices は更に for ループの外側に置きたいが、OpenMP 使っているとアクセス競合しそうなのでループの中に置く。
				std::pair<ptrdiff_t, int> indices[KPPIndicesMax];
				for (int i = 0; i < fe_end; ++i) {
					for (int j = 0; j < fe_end; ++j) {
						kppIndices(indices, static_cast<Square>(ksq), i, j);
						s64 sum = 0;
						FOO(indices, oneArrayKPP, sum);
						KPP[ksq][i][j] = sum;
					}
				}
			}
		}
		// KKP
		{
#ifdef _OPENMP
#pragma omp for
#endif
			for (int ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
				std::pair<ptrdiff_t, int> indices[KKPIndicesMax];
				for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
					for (int i = 0; i < fe_end; ++i) {
						kkpIndices(indices, static_cast<Square>(ksq0), ksq1, i);
						s64 sum = 0;
						FOO(indices, oneArrayKKP, sum);
						KKP[ksq0][ksq1][i] = sum;
					}
				}
			}
		}
		// KK
		{
#ifdef _OPENMP
#pragma omp for
#endif
			for (int ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
				std::pair<ptrdiff_t, int> indices[KKIndicesMax];
				for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
					kkIndices(indices, static_cast<Square>(ksq0), ksq1);
					s64 sum = 0;
					FOO(indices, oneArrayKK, sum);
					KK[ksq0][ksq1] = sum / 2;
#if defined USE_K_FIX_OFFSET
					KK[ksq0][ksq1] += K_Fix_Offset[ksq0] - K_Fix_Offset[inverse(ksq1)];
#endif
				}
			}
		}
#undef FOO

#if !defined LEARN
		SYNCCOUT << "info string end setting eval table" << SYNCENDL;
#endif
	}
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

#endif // #ifndef APERY_EVALUATE_HPP
