#include "common.hpp"
#include "init.hpp"
#include "mt64bit.hpp"
#include "evaluate.hpp"
#include "book.hpp"
#include "search.hpp"

namespace {
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

	// square のマスにおける、障害物を調べる必要がある場所を調べて Bitboard で返す。
	Bitboard rookBlockMaskCalc(const Square square) {
		Bitboard result = squareFileMask(square) ^ squareRankMask(square);
		if (makeFile(square) != FileA) { result &= ~fileMask<FileA>(); }
		if (makeFile(square) != FileI) { result &= ~fileMask<FileI>(); }
		if (makeRank(square) != Rank1) { result &= ~rankMask<Rank1>(); }
		if (makeRank(square) != Rank9) { result &= ~rankMask<Rank9>(); }
		return result;
	}

	// square のマスにおける、障害物を調べる必要がある場所を調べて Bitboard で返す。
	Bitboard bishopBlockMaskCalc(const Square square) {
		const Rank rank = makeRank(square);
		const File file = makeFile(square);
		Bitboard result = allZeroBB();
		for (Square sq = I9; sq < SquareNum; ++sq) {
			const Rank r = makeRank(sq);
			const File f = makeFile(sq);
			if (abs(rank - r) == abs(file - f))
				result.setBit(sq);
		}
		result &= ~(rankMask<Rank1>() | rankMask<Rank9>() | fileMask<FileA>() | fileMask<FileI>());
		result.clearBit(square);

		return result;
	}

	// square のマスにおける、障害物を調べる必要がある場所を Bitboard で返す。
	// lance の前方だけを調べれば良さそうだけど、Rank8 ~ Rank2 の状態をそのまま index に使いたいので、
	// 縦方向全て(端を除く)の occupied を全て調べる。
	Bitboard lanceBlockMask(const Square square) {
		return squareFileMask(square) & ~(rankMask<Rank1>() | rankMask<Rank9>());
	}

	// Rook or Bishop の利きの範囲を調べて bitboard で返す。
	// occupied  障害物があるマスが 1 の bitboard
	Bitboard attackCalc(const Square square, const Bitboard& occupied, const bool isBishop) {
		SquareDelta deltaArray[2][4] = {{DeltaN, DeltaS, DeltaE, DeltaW}, {DeltaNE, DeltaSE, DeltaSW, DeltaNW}};
		Bitboard result = allZeroBB();
		for (SquareDelta delta : deltaArray[isBishop]) {
			for (Square sq = square + delta;
				 isInSquare(sq) && abs(makeRank(sq - delta) - makeRank(sq)) <= 1;
				 sq += delta)
			{
				result.setBit(sq);
				if (occupied.isSet(sq))
					break;
			}
		}

		return result;
	}

	// lance の利きを返す。
	// 香車の利きは常にこれを使っても良いけど、もう少し速くする為に、テーブル化する為だけに使う。
	// occupied  障害物があるマスが 1 の bitboard
	Bitboard lanceAttackCalc(const Color c, const Square square, const Bitboard& occupied) {
		return rookAttack(square, occupied) & inFrontMask(c, makeRank(square));
	}

	// index, bits の情報を元にして、occupied の 1 のbit を いくつか 0 にする。
	// index の値を, occupied の 1のbit の位置に変換する。
	// index   (0, 1<<bits] の範囲のindex
	// bits    bit size
	// blockMask   利きのあるマスが 1 のbitboard
	// result  occupied
	Bitboard indexToOccupied(const int index, const int bits, const Bitboard& blockMask) {
		Bitboard tmpBlockMask = blockMask;
		Bitboard result = allZeroBB();
		for (int i = 0; i < bits; ++i) {
			const Square sq = tmpBlockMask.firstOneFromI9();
			if (index & (1 << i))
				result.setBit(sq);
		}
		return result;
	}

	void initAttacks(const bool isBishop)
	{
		auto* attacks     = (isBishop ? BishopAttack      : RookAttack     );
		auto* attackIndex = (isBishop ? BishopAttackIndex : RookAttackIndex);
		auto* blockMask   = (isBishop ? BishopBlockMask   : RookBlockMask  );
		auto* shift       = (isBishop ? BishopShiftBits   : RookShiftBits  );
#if defined HAVE_BMI2
#else
		auto* magic       = (isBishop ? BishopMagic       : RookMagic      );
#endif
		int index = 0;
		for (Square sq = I9; sq < SquareNum; ++sq) {
			blockMask[sq] = (isBishop ? bishopBlockMaskCalc(sq) : rookBlockMaskCalc(sq));
			attackIndex[sq] = index;

			Bitboard occupied[1 << 14];
			const int num1s = (isBishop ? BishopBlockBits[sq] : RookBlockBits[sq]);
			for (int i = 0; i < (1 << num1s); ++i) {
				occupied[i] = indexToOccupied(i, num1s, blockMask[sq]);
#if defined HAVE_BMI2
				attacks[index + occupiedToIndex(occupied[i] & blockMask[sq], blockMask[sq])] = attackCalc(sq, occupied[i], isBishop);
#else
				attacks[index + occupiedToIndex(occupied[i], magic[sq], shift[sq])] = attackCalc(sq, occupied[i], isBishop);
#endif
			}
			index += 1 << (64 - shift[sq]);
		}
	}

	// LanceBlockMask, LanceAttack の値を設定する。
	void initLanceAttacks() {
		for (Color c = Black; c < ColorNum; ++c) {
			for (Square sq = I9; sq < SquareNum; ++sq) {
				const Bitboard blockMask = lanceBlockMask(sq);
				const int num1s = blockMask.popCount(); // 常に 7
				for (int i = 0; i < (1 << num1s); ++i) {
					Bitboard occupied = indexToOccupied(i, num1s, blockMask);
					LanceAttack[c][sq][i] = lanceAttackCalc(c, sq, occupied);
				}
			}
		}
	}

	void initKingAttacks() {
		for (Square sq = I9; sq < SquareNum; ++sq)
			KingAttack[sq] = rookAttack(sq, allOneBB()) | bishopAttack(sq, allOneBB());
	}

	void initGoldAttacks() {
		for (Color c = Black; c < ColorNum; ++c)
			for (Square sq = I9; sq < SquareNum; ++sq)
				GoldAttack[c][sq] = (kingAttack(sq) & inFrontMask(c, makeRank(sq))) | rookAttack(sq, allOneBB());
	}

	void initSilverAttacks() {
		for (Color c = Black; c < ColorNum; ++c)
			for (Square sq = I9; sq < SquareNum; ++sq)
				SilverAttack[c][sq] = (kingAttack(sq) & inFrontMask(c, makeRank(sq))) | bishopAttack(sq, allOneBB());
	}

	void initKnightAttacks() {
		for (Color c = Black; c < ColorNum; ++c) {
			for (Square sq = I9; sq < SquareNum; ++sq) {
				KnightAttack[c][sq] = allZeroBB();
				const Bitboard bb = pawnAttack(c, sq);
				if (bb.isNot0())
					KnightAttack[c][sq] = bishopStepAttacks(bb.constFirstOneFromI9()) & inFrontMask(c, makeRank(sq));
			}
		}
	}

	void initPawnAttacks() {
		for (Color c = Black; c < ColorNum; ++c)
			for (Square sq = I9; sq < SquareNum; ++sq)
				PawnAttack[c][sq] = silverAttack(c, sq) ^ bishopAttack(sq, allOneBB());
	}

	void initSquareRelation() {
		for (Square sq1 = I9; sq1 < SquareNum; ++sq1) {
			const File file1 = makeFile(sq1);
			const Rank rank1 = makeRank(sq1);
			for (Square sq2 = I9; sq2 < SquareNum; ++sq2) {
				const File file2 = makeFile(sq2);
				const Rank rank2 = makeRank(sq2);
				SquareRelation[sq1][sq2] = DirecMisc;
				if (sq1 == sq2) continue;

				if      (file1 == file2)
					SquareRelation[sq1][sq2] = DirecFile;
				else if (rank1 == rank2)
					SquareRelation[sq1][sq2] = DirecRank;
				else if (static_cast<int>(rank1 - rank2) == static_cast<int>(file1 - file2))
					SquareRelation[sq1][sq2] = DirecDiagNESW;
				else if (static_cast<int>(rank1 - rank2) == static_cast<int>(file2 - file1))
					SquareRelation[sq1][sq2] = DirecDiagNWSE;
			}
		}
	}

	// 障害物が無いときの利きの Bitboard
	// RookAttack, BishopAttack, LanceAttack を設定してから、この関数を呼ぶこと。
	void initAttackToEdge() {
		for (Square sq = I9; sq < SquareNum; ++sq) {
			RookAttackToEdge[sq] = rookAttack(sq, allZeroBB());
			BishopAttackToEdge[sq] = bishopAttack(sq, allZeroBB());
			LanceAttackToEdge[Black][sq] = lanceAttack(Black, sq, allZeroBB());
			LanceAttackToEdge[White][sq] = lanceAttack(White, sq, allZeroBB());
		}
	}

	void initBetweenBB() {
		for (Square sq1 = I9; sq1 < SquareNum; ++sq1) {
			for (Square sq2 = I9; sq2 < SquareNum; ++sq2) {
				BetweenBB[sq1][sq2] = allZeroBB();
				if (sq1 == sq2) continue;
				const Direction direc = squareRelation(sq1, sq2);
				if      (direc & DirecCross)
					BetweenBB[sq1][sq2] = rookAttack(sq1, setMaskBB(sq2)) & rookAttack(sq2, setMaskBB(sq1));
				else if (direc & DirecDiag)
					BetweenBB[sq1][sq2] = bishopAttack(sq1, setMaskBB(sq2)) & bishopAttack(sq2, setMaskBB(sq1));
			}
		}
	}

	void initCheckTable() {
		for (Color c = Black; c < ColorNum; ++c) {
			const Color opp = oppositeColor(c);
			for (Square sq = I9; sq < SquareNum; ++sq) {
				GoldCheckTable[c][sq] = allZeroBB();
				Bitboard checkBB = goldAttack(opp, sq);
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					GoldCheckTable[c][sq] |= goldAttack(opp, checkSq);
				}
				GoldCheckTable[c][sq].andEqualNot(setMaskBB(sq) | goldAttack(opp, sq));
			}
		}

		for (Color c = Black; c < ColorNum; ++c) {
			const Color opp = oppositeColor(c);
			for (Square sq = I9; sq < SquareNum; ++sq) {
				SilverCheckTable[c][sq] = allZeroBB();

				Bitboard checkBB = silverAttack(opp, sq);
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					SilverCheckTable[c][sq] |= silverAttack(opp, checkSq);
				}
				const Bitboard TRank789BB = (c == Black ? inFrontMask<Black, Rank6>() : inFrontMask<White, Rank4>());
				checkBB = goldAttack(opp, sq);
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					// 移動元が敵陣である位置なら、金に成って王手出来る。
					SilverCheckTable[c][sq] |= (silverAttack(opp, checkSq) & TRank789BB);
				}

				const Bitboard TRank6BB = (c == Black ? rankMask<Rank6>() : rankMask<Rank4>());
				// 移動先が3段目で、4段目に移動したときも、成ることが出来る。
				checkBB = goldAttack(opp, sq) & TRank789BB;
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					SilverCheckTable[c][sq] |= (silverAttack(opp, checkSq) & TRank6BB);
				}
				SilverCheckTable[c][sq].andEqualNot(setMaskBB(sq) | silverAttack(opp, sq));
			}
		}

		for (Color c = Black; c < ColorNum; ++c) {
			const Color opp = oppositeColor(c);
			for (Square sq = I9; sq < SquareNum; ++sq) {
				KnightCheckTable[c][sq] = allZeroBB();

				Bitboard checkBB = knightAttack(opp, sq);
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					KnightCheckTable[c][sq] |= knightAttack(opp, checkSq);
				}
				const Bitboard TRank789BB = (c == Black ? inFrontMask<Black, Rank6>() : inFrontMask<White, Rank4>());
				checkBB = goldAttack(opp, sq) & TRank789BB;
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					KnightCheckTable[c][sq] |= knightAttack(opp, checkSq);
				}
			}
		}

		for (Color c = Black; c < ColorNum; ++c) {
			const Color opp = oppositeColor(c);
			for (Square sq = I9; sq < SquareNum; ++sq) {
				LanceCheckTable[c][sq] = lanceAttackToEdge(opp, sq);

				const Bitboard TRank789BB = (c == Black ? inFrontMask<Black, Rank6>() : inFrontMask<White, Rank4>());
				Bitboard checkBB = goldAttack(opp, sq) & TRank789BB;
				while (checkBB.isNot0()) {
					const Square checkSq = checkBB.firstOneFromI9();
					LanceCheckTable[c][sq] |= lanceAttackToEdge(opp, checkSq);
				}
				LanceCheckTable[c][sq].andEqualNot(setMaskBB(sq) | pawnAttack(opp, sq));
			}
		}
	}

	void kppBonanzaToApery(const std::vector<s16>& kpptmp, const Bonanza::Square sqb1,
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

#define FVS Apery::FVScale
	const int KingSquareScore[SquareNum] = {
		2000*FVS, 1700*FVS, 1350*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS,
		1800*FVS, 1500*FVS, 1250*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS, 
		1800*FVS, 1500*FVS, 1250*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS, 
		1700*FVS, 1400*FVS, 1150*FVS,  900*FVS,  550*FVS,  250*FVS,    0*FVS,    0*FVS,    0*FVS, 
		1600*FVS, 1300*FVS, 1050*FVS,  800*FVS,  450*FVS,  150*FVS,    0*FVS,    0*FVS,    0*FVS, 
		1700*FVS, 1400*FVS, 1150*FVS,  900*FVS,  550*FVS,  250*FVS,    0*FVS,    0*FVS,    0*FVS, 
		1800*FVS, 1500*FVS, 1250*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS, 
		1900*FVS, 1600*FVS, 1350*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS, 
		2000*FVS, 1700*FVS, 1350*FVS, 1000*FVS,  650*FVS,  350*FVS,  100*FVS,    0*FVS,    0*FVS
	};
#undef FVS

	void initFV() {
		std::ifstream ifs("../bin/20141122/fv38.bin", std::ios::binary);
		std::vector<s16> kpptmp(Bonanza::SquareNum * Bonanza::pos_n);
		std::vector<s32> kkptmp(Bonanza::SquareNum * Bonanza::SquareNum * Bonanza::fe_end);
		s32 k00sumtmp[Bonanza::SquareNum][Bonanza::SquareNum];

		ifs.read(reinterpret_cast<char*>(&kpptmp[0]),
				 sizeof(s16) * static_cast<int>(Bonanza::SquareNum * Bonanza::pos_n));
		ifs.read(reinterpret_cast<char*>(&kkptmp[0])  ,
				 sizeof(s32) * static_cast<int>(Bonanza::SquareNum * Bonanza::SquareNum * Bonanza::fe_end));
		ifs.read(reinterpret_cast<char*>(&k00sumtmp[0][0]), sizeof(k00sumtmp));

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

				K00Sum[sq1][sq2] = k00sumtmp[sqb1][sqb2];
#if defined USE_KING_SQUARE_SCORE
				K00Sum[sq1][sq2] += KingSquareScore[sq1] - KingSquareScore[inverse(sq2)];
#endif

				for (Bonanza::Square sqb3 = Bonanza::A9; sqb3 < Bonanza::SquareNum; ++sqb3) {
					const Square sq3 = squareBonanzaToApery(sqb3);

					// ここから持ち駒
					// sqb3 は持ち駒の枚数を表す。
					if (static_cast<int>(sqb3) <= 18) {
						KKP[sq1][sq2][Apery::f_hand_pawn   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_pawn   + sqb3)];

						KKP[sq1][sq2][Apery::e_hand_pawn   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_pawn   + sqb3)];
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
					}
					if (static_cast<int>(sqb3) <= 2) {
						KKP[sq1][sq2][Apery::f_hand_bishop + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_bishop + sqb3)];
						KKP[sq1][sq2][Apery::f_hand_rook   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_hand_rook   + sqb3)];

						KKP[sq1][sq2][Apery::e_hand_bishop + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_bishop + sqb3)];
						KKP[sq1][sq2][Apery::e_hand_rook   + sqb3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_hand_rook   + sqb3)];
					}

					// ここから盤上の駒
					// sqb3 は Bonanza 方式での盤上の位置を表す
					if (Bonanza::A8 <= sqb3) {
						KKP[sq1][sq2][Apery::f_pawn   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_pawn   + sqb3)];
						KKP[sq1][sq2][Apery::f_lance  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_lance  + sqb3)];
					}
					if (Bonanza::A8 <= inverse(sqb3)) {
						KKP[sq1][sq2][Apery::e_pawn   + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_pawn   + sqb3)];
						KKP[sq1][sq2][Apery::e_lance  + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_lance  + sqb3)];
					}
					if (Bonanza::A7 <= sqb3) {
						KKP[sq1][sq2][Apery::f_knight + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::f_knight + sqb3)];
					}
					if (Bonanza::A7 <= inverse(sqb3)) {
						KKP[sq1][sq2][Apery::e_knight + sq3] = kkptmp[(sqb1 * Bonanza::SquareNum + sqb2) * Bonanza::fe_end + (Bonanza::e_knight + sqb3)];
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
				}
			}
		}
#if 0
		// fv_apery.bin を生成したければ、ここでファイルに出力する。
		std::ofstream ofs("../bin/fv_apery.bin", std::ios::binary);
		ofs.write(reinterpret_cast<char*>(&KPP[0][0][0]), sizeof(KPP   ));
		ofs.write(reinterpret_cast<char*>(&KKP[0][0][0]), sizeof(KKP   ));
		ofs.write(reinterpret_cast<char*>(&K00Sum[0][0]), sizeof(K00Sum));
#endif
	}

//	void initFV() {
//		std::ifstream ifs("../bin/fv_apery.bin", std::ios::binary);
//		ifs.read(reinterpret_cast<char*>(&KPP[0][0][0]), sizeof(KPP   ));
//		ifs.read(reinterpret_cast<char*>(&KKP[0][0][0]), sizeof(KKP   ));
//		ifs.read(reinterpret_cast<char*>(&K00Sum[0][0]), sizeof(K00Sum));
//	}
}

void initTable() {
	initAttacks(false);
	initAttacks(true);
	initKingAttacks();
	initGoldAttacks();
	initSilverAttacks();
	initPawnAttacks();
	initKnightAttacks();
	initLanceAttacks();
	initSquareRelation();
	initAttackToEdge();
	initBetweenBB();
	initCheckTable();

	initFV();

	Book::init();
	initSearchTable();
}

#if defined FIND_MAGIC
// square の位置の rook, bishop それぞれのMagic Bitboard に使用するマジックナンバーを見つける。
// isBishop  : true なら bishop, false なら rook のマジックナンバーを見つける。
u64 findMagic(const Square square, const bool isBishop) {
	Bitboard occupied[1<<14];
	Bitboard attack[1<<14];
	Bitboard attackUsed[1<<14];
	Bitboard mask = (isBishop ? bishopBlockMaskCalc(square) : rookBlockMaskCalc(square));
	int num1s = (isBishop ? BishopBlockBits[square] : RookBlockBits[square]);

	// n bit の全ての数字 (利きのあるマスの全ての 0 or 1 の組み合わせ)
	for (int i = 0; i < (1 << num1s); ++i) {
		occupied[i] = indexToOccupied(i, num1s, mask);
		attack[i] = attackCalc(square, occupied[i], isBishop);
	}

	for (u64 k = 0; k < UINT64_C(100000000); ++k) {
		const u64 magic = g_mt64bit.randomFewBits();
		bool fail = false;

		// これは無くても良いけど、少しマジックナンバーが見つかるのが早くなるはず。
		if (count1s((mask.merge() * magic) & UINT64_C(0xfff0000000000000)) < 6)
			continue;

		std::fill(std::begin(attackUsed), std::end(attackUsed), allZeroBB());

		for (int i = 0; !fail && i < (1 << num1s); ++i) {
			const int shiftBits = (isBishop ? BishopShiftBits[square] : RookShiftBits[square]);
			const u64 index = occupiedToIndex(occupied[i], magic, shiftBits);
			if      (attackUsed[index] == allZeroBB())
				attackUsed[index] = attack[i];
			else if (attackUsed[index] != attack[i])
				fail = true;
		}
		if (!fail)
			return magic;
	}

	std::cout << "/***Failed***/\t";
	return 0;
}
#endif // #if defined FIND_MAGIC
