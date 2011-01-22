#include "evalList.hpp"
#include "position.hpp"
#include "evaluate.hpp"

const Square HandPieceToSquareHand[ColorNum][HandPieceNum] = {
	{B_hand_pawn, B_hand_lance, B_hand_knight, B_hand_silver, B_hand_gold, B_hand_bishop, B_hand_rook},
	{W_hand_pawn, W_hand_lance, W_hand_knight, W_hand_silver, W_hand_gold, W_hand_bishop, W_hand_rook}
};

void EvalList::set(const Position& pos) {
	const Hand handB = pos.hand(Black);
	const Hand handW = pos.hand(White);

	int nlist = 0;
#define FOO(hand, HP, list0_index, list1_index, c)						\
	for (u32 i = 1; i <= hand.numOf<HP>(); ++i) {						\
		list0[nlist] = list0_index + i;									\
		list1[nlist] = list1_index + i;									\
		const Square squarehand = HandPieceToSquareHand[c][HP] + static_cast<Square>(i); \
		listToSquareHand[nlist] = squarehand;							\
		squareHandToList[squarehand] = nlist;							\
		++nlist;														\
	}

	FOO(handB, HPawn  , Apery::f_hand_pawn  , Apery::e_hand_pawn  , Black);
	FOO(handW, HPawn  , Apery::e_hand_pawn  , Apery::f_hand_pawn  , White);
	FOO(handB, HLance , Apery::f_hand_lance , Apery::e_hand_lance , Black);
	FOO(handW, HLance , Apery::e_hand_lance , Apery::f_hand_lance , White);
	FOO(handB, HKnight, Apery::f_hand_knight, Apery::e_hand_knight, Black);
	FOO(handW, HKnight, Apery::e_hand_knight, Apery::f_hand_knight, White);
	FOO(handB, HSilver, Apery::f_hand_silver, Apery::e_hand_silver, Black);
	FOO(handW, HSilver, Apery::e_hand_silver, Apery::f_hand_silver, White);
	FOO(handB, HGold  , Apery::f_hand_gold  , Apery::e_hand_gold  , Black);
	FOO(handW, HGold  , Apery::e_hand_gold  , Apery::f_hand_gold  , White);
	FOO(handB, HBishop, Apery::f_hand_bishop, Apery::e_hand_bishop, Black);
	FOO(handW, HBishop, Apery::e_hand_bishop, Apery::f_hand_bishop, White);
	FOO(handB, HRook  , Apery::f_hand_rook  , Apery::e_hand_rook  , Black);
	FOO(handW, HRook  , Apery::e_hand_rook  , Apery::f_hand_rook  , White);
#undef FOO

	Bitboard bb = pos.bbOf(King).notThisAnd(pos.occupiedBB());
	while (bb.isNot0()) {
		const Square sq = bb.firstOneFromI9();
		const Piece pc = pos.piece(sq);
		listToSquareHand[nlist] = sq;
		squareHandToList[sq] = nlist;
		list0[nlist  ] = kppArray[pc         ] + sq;
		list1[nlist++] = kppArray[inverse(pc)] + inverse(sq);
	}
}
