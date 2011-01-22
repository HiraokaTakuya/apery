#include "evaluate.hpp"
#include "position.hpp"
#include "search.hpp"
#include "thread.hpp"

int K00Sum[SquareNum][SquareNum];
s16 KPP[SquareNum][Apery::fe_end][Apery::fe_end];
s32 KKP[SquareNum][SquareNum][Apery::fe_end];

EvaluateHashTable g_evalTable;

const int kppArray[31] = {0,               Apery::f_pawn,   Apery::f_lance,  Apery::f_knight,
						  Apery::f_silver, Apery::f_bishop, Apery::f_rook,   Apery::f_gold,   
						  0,               Apery::f_gold,   Apery::f_gold,   Apery::f_gold,
						  Apery::f_gold,   Apery::f_horse,  Apery::f_dragon,
						  0,
						  0,               Apery::e_pawn,   Apery::e_lance,  Apery::e_knight,
						  Apery::e_silver, Apery::e_bishop, Apery::e_rook,   Apery::e_gold,   
						  0,               Apery::e_gold,   Apery::e_gold,   Apery::e_gold,
						  Apery::e_gold,   Apery::e_horse,  Apery::e_dragon};

const int kppHandArray[ColorNum][HandPieceNum] = {
	{Apery::f_hand_pawn, Apery::f_hand_lance, Apery::f_hand_knight, Apery::f_hand_silver,
	 Apery::f_hand_gold, Apery::f_hand_bishop, Apery::f_hand_rook},
	{Apery::e_hand_pawn, Apery::e_hand_lance, Apery::e_hand_knight, Apery::e_hand_silver,
	 Apery::e_hand_gold, Apery::e_hand_bishop, Apery::e_hand_rook}
};

namespace {
	Score doapc(const Position& pos, const int index[2]) {
		const Square sq_bk = pos.kingSquare(Black);
		const Square sq_wk = pos.kingSquare(White);
		const int* list0 = pos.cplist0();
		const int* list1 = pos.cplist1();

		Score sum = kkp(sq_bk, sq_wk, index[0]);
		const auto* pkppb = KPP[sq_bk         ][index[0]];
		const auto* pkppw = KPP[inverse(sq_wk)][index[1]];
		for (int i = 0; i < pos.nlist(); ++i) {
			sum += pkppb[list0[i]];
			sum -= pkppw[list1[i]];
		}

		return sum;
	}

#if defined INANIWA_SHIFT
	Score inaniwaScoreBody(const Position& pos) {
		Score score = ScoreZero;
		if (g_inaniwaFlag == InaniwaIsBlack) {
			if (pos.piece(B9) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(H9) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(A7) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(I7) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(C7) == WKnight) { score += 400 * Apery::FVScale; }
			if (pos.piece(G7) == WKnight) { score += 400 * Apery::FVScale; }
			if (pos.piece(B5) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(H5) == WKnight) { score += 700 * Apery::FVScale; }
			if (pos.piece(D5) == WKnight) { score += 100 * Apery::FVScale; }
			if (pos.piece(F5) == WKnight) { score += 100 * Apery::FVScale; }
			if (pos.piece(E3) == BPawn)   { score += 200 * Apery::FVScale; }
			if (pos.piece(E4) == BPawn)   { score += 200 * Apery::FVScale; }
			if (pos.piece(E5) == BPawn)   { score += 200 * Apery::FVScale; }
		}
		else {
			assert(g_inaniwaFlag == InaniwaIsWhite);
			if (pos.piece(B1) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(H1) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(A3) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(I3) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(C3) == BKnight) { score -= 400 * Apery::FVScale; }
			if (pos.piece(G3) == BKnight) { score -= 400 * Apery::FVScale; }
			if (pos.piece(B5) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(H5) == BKnight) { score -= 700 * Apery::FVScale; }
			if (pos.piece(D5) == BKnight) { score -= 100 * Apery::FVScale; }
			if (pos.piece(F5) == BKnight) { score -= 100 * Apery::FVScale; }
			if (pos.piece(E7) == WPawn)   { score -= 200 * Apery::FVScale; }
			if (pos.piece(E6) == WPawn)   { score -= 200 * Apery::FVScale; }
			if (pos.piece(E5) == WPawn)   { score -= 200 * Apery::FVScale; }
		}
		return score;
	}
	inline Score inaniwaScore(const Position& pos) {
		if (g_inaniwaFlag == NotInaniwa) return ScoreZero;
		return inaniwaScoreBody(pos);
	}
#endif

	bool calcDifference(Position& pos, SearchStack* ss) {
#if defined INANIWA_SHIFT
		if (g_inaniwaFlag != NotInaniwa) return false;
#endif
		Move lastMove;
		if ((ss-1)->staticEvalRaw == INT_MAX || (lastMove = (ss-1)->currentMove).pieceTypeFrom() == King) {
			return false;
		}

		assert(lastMove != Move::moveNull());

		const int listIndex = pos.cl().listindex[0];
		auto diff = doapc(pos, pos.cl().clistpair[0].newlist);
		if (pos.cl().size == 1) {
			pos.plist0()[listIndex] = pos.cl().clistpair[0].oldlist[0];
			pos.plist1()[listIndex] = pos.cl().clistpair[0].oldlist[1];
			diff -= doapc(pos, pos.cl().clistpair[0].oldlist);
		}
		else {
			assert(pos.cl().size == 2);
			diff += doapc(pos, pos.cl().clistpair[1].newlist);
			diff -= kpp(pos.kingSquare(Black)         , pos.cl().clistpair[0].newlist[0], pos.cl().clistpair[1].newlist[0]);
			diff += kpp(inverse(pos.kingSquare(White)), pos.cl().clistpair[0].newlist[1], pos.cl().clistpair[1].newlist[1]);
			const int listIndex_cap = pos.cl().listindex[1];
			pos.plist0()[listIndex_cap] = pos.cl().clistpair[1].oldlist[0];
			pos.plist1()[listIndex_cap] = pos.cl().clistpair[1].oldlist[1];

			pos.plist0()[listIndex] = pos.cl().clistpair[0].oldlist[0];
			pos.plist1()[listIndex] = pos.cl().clistpair[0].oldlist[1];
			diff -= doapc(pos, pos.cl().clistpair[0].oldlist);

			diff -= doapc(pos, pos.cl().clistpair[1].oldlist);
			diff += kpp(pos.kingSquare(Black)         , pos.cl().clistpair[0].oldlist[0], pos.cl().clistpair[1].oldlist[0]);
			diff -= kpp(inverse(pos.kingSquare(White)), pos.cl().clistpair[0].oldlist[1], pos.cl().clistpair[1].oldlist[1]);
			pos.plist0()[listIndex_cap] = pos.cl().clistpair[1].newlist[0];
			pos.plist1()[listIndex_cap] = pos.cl().clistpair[1].newlist[1];
		}
		pos.plist0()[listIndex] = pos.cl().clistpair[0].newlist[0];
		pos.plist1()[listIndex] = pos.cl().clistpair[0].newlist[1];

		diff += pos.materialDiff() * Apery::FVScale;

		ss->staticEvalRaw = diff + (ss-1)->staticEvalRaw;

		return true;
	}

	int make_list_unUseDiff(const Position& pos, int list0[EvalList::ListSize], int list1[EvalList::ListSize], int nlist) {
		Square sq;
		Bitboard bb;

#define FOO(posBB, f_pt, e_pt)											\
		bb = (posBB) & pos.bbOf(Black);									\
		FOREACH_BB(bb, sq, {											\
				list0[nlist] = (f_pt) + sq;								\
				list1[nlist] = (e_pt) + inverse(sq);					\
				nlist    += 1;											\
			});															\
																		\
		bb = (posBB) & pos.bbOf(White);									\
		FOREACH_BB(bb, sq, {											\
				list0[nlist] = (e_pt) + sq;								\
				list1[nlist] = (f_pt) + inverse(sq);					\
				nlist    += 1;											\
			});

		FOO(pos.bbOf(Pawn  ), Apery::f_pawn  , Apery::e_pawn  );
		FOO(pos.bbOf(Lance ), Apery::f_lance , Apery::e_lance );
		FOO(pos.bbOf(Knight), Apery::f_knight, Apery::e_knight);
		FOO(pos.bbOf(Silver), Apery::f_silver, Apery::e_silver);
		const Bitboard goldsBB = pos.goldsBB();
		FOO(goldsBB         , Apery::f_gold  , Apery::e_gold  );
		FOO(pos.bbOf(Bishop), Apery::f_bishop, Apery::e_bishop);
		FOO(pos.bbOf(Horse ), Apery::f_horse , Apery::e_horse );
		FOO(pos.bbOf(Rook  ), Apery::f_rook  , Apery::e_rook  );
		FOO(pos.bbOf(Dragon), Apery::f_dragon, Apery::e_dragon);

#undef FOO

		return nlist;
	}

	Score evaluateBody(Position& pos, SearchStack* ss) {
		if (calcDifference(pos, ss)) {
			const auto score = ss->staticEvalRaw;
			assert(evaluateUnUseDiff(pos) == (pos.turn() == Black ? score : -score));
			return score;
		}

		const Square sq_bk = pos.kingSquare(Black);
		const Square sq_wk = pos.kingSquare(White);
		const int* list0 = pos.plist0();
		const int* list1 = pos.plist1();

		const auto* ppkppb = KPP[sq_bk         ];
		const auto* ppkppw = KPP[inverse(sq_wk)];

		Score score = static_cast<Score>(K00Sum[sq_bk][sq_wk]);
		// loop 開始を i = 1 からにして、i = 0 の分のKKPを先に足す。
		score += KKP[sq_bk][sq_wk][list0[0]];
		for (int i = 1; i < pos.nlist(); ++i) {
			const int k0 = list0[i];
			const int k1 = list1[i];
			const auto* pkppb = ppkppb[k0];
			const auto* pkppw = ppkppw[k1];
			for (int j = 0; j < i; ++j) {
				const int l0 = list0[j];
				const int l1 = list1[j];
				score += pkppb[l0];
				score -= pkppw[l1];
			}
			score += KKP[sq_bk][sq_wk][k0];
		}

		score += pos.material() * Apery::FVScale;
#if defined INANIWA_SHIFT
		score += inaniwaScore(pos);
#endif
		ss->staticEvalRaw = score;

		assert(evaluateUnUseDiff(pos) == (pos.turn() == Black ? score : -score));
		return score;
	}
}

Score evaluateUnUseDiff(const Position& pos) {
	int list0[EvalList::ListSize];
	int list1[EvalList::ListSize];

	const Hand handB = pos.hand(Black);
	const Hand handW = pos.hand(White);

	const Square sq_bk = pos.kingSquare(Black);
	const Square sq_wk = pos.kingSquare(White);
	int nlist = 0;
	int hand0_index[ColorNum] = {0, 0};

#define FOO(hand, HP, list0_index, list1_index, hand0_shift)		\
	for (u32 i = 1; i <= hand.numOf<HP>(); ++i) {					\
		list0[nlist] = list0_index + i;								\
		list1[nlist] = list1_index + i;								\
		++nlist;													\
		hand0_index[Black] |= 1 << hand0_shift;						\
		hand0_index[White] |= 1 << (hand0_shift ^ 1);				\
	}

	FOO(handB, HPawn  , Apery::f_hand_pawn  , Apery::e_hand_pawn  ,  0);
	FOO(handW, HPawn  , Apery::e_hand_pawn  , Apery::f_hand_pawn  ,  1);
	FOO(handB, HLance , Apery::f_hand_lance , Apery::e_hand_lance ,  2);
	FOO(handW, HLance , Apery::e_hand_lance , Apery::f_hand_lance ,  3);
	FOO(handB, HKnight, Apery::f_hand_knight, Apery::e_hand_knight,  4);
	FOO(handW, HKnight, Apery::e_hand_knight, Apery::f_hand_knight,  5);
	FOO(handB, HSilver, Apery::f_hand_silver, Apery::e_hand_silver,  6);
	FOO(handW, HSilver, Apery::e_hand_silver, Apery::f_hand_silver,  7);
	FOO(handB, HGold  , Apery::f_hand_gold  , Apery::e_hand_gold  ,  8);
	FOO(handW, HGold  , Apery::e_hand_gold  , Apery::f_hand_gold  ,  9);
	FOO(handB, HBishop, Apery::f_hand_bishop, Apery::e_hand_bishop, 10);
	FOO(handW, HBishop, Apery::e_hand_bishop, Apery::f_hand_bishop, 11);
	FOO(handB, HRook  , Apery::f_hand_rook  , Apery::e_hand_rook  , 12);
	FOO(handW, HRook  , Apery::e_hand_rook  , Apery::f_hand_rook  , 13);
#undef FOO

	nlist = make_list_unUseDiff(pos, list0, list1, nlist);

	const auto* ppkppb = KPP[sq_bk         ];
	const auto* ppkppw = KPP[inverse(sq_wk)];

	Score score = static_cast<Score>(K00Sum[sq_bk][sq_wk]);
	for (int i = 0; i < nlist; ++i) {
		const int k0 = list0[i];
		const int k1 = list1[i];
		const auto* pkppb = ppkppb[k0];
		const auto* pkppw = ppkppw[k1];
		for (int j = 0; j < i; ++j) {
			const int l0 = list0[j];
			const int l1 = list1[j];
			score += pkppb[l0];
			score -= pkppw[l1];
		}
		score += KKP[sq_bk][sq_wk][k0];
	}

	score += pos.material() * Apery::FVScale;

#if defined INANIWA_SHIFT
	score += inaniwaScore(pos);
#endif

	if (pos.turn() == White) {
		score = -score;
	}

	return score;
}

Score evaluate(Position& pos, SearchStack* ss) {
	if (ss->staticEvalRaw != INT_MAX) {
		// null move の次の手の時のみ、ここに入る。
		assert((pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) == evaluateUnUseDiff(pos));
		return (pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) / Apery::FVScale;
	}

	const u32 keyHigh32 = static_cast<u32>(pos.getKey() >> 32);
	const Key keyExcludeTurn = pos.getKeyExcludeTurn();
	// ポインタで取得してはいけない。lockless hash なので key と score を同時に取得する。
	EvaluateHashEntry entry = *g_evalTable[keyExcludeTurn];
	entry.decode();
	if (entry.key() == keyHigh32) {
		ss->staticEvalRaw = entry.score();
		assert((pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) == evaluateUnUseDiff(pos));
		return (pos.turn() == Black ? entry.score() : -entry.score()) / Apery::FVScale;
	}

	const Score score = evaluateBody(pos, ss);
	entry.save(pos.getKey(), score);
	entry.encode();
	*g_evalTable[keyExcludeTurn] = entry;
	return (pos.turn() == Black ? score : -score) / Apery::FVScale;
}
