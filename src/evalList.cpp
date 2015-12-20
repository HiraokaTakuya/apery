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
	auto func = [&nlist, this](const Hand hand, const HandPiece hp, const int list0_index, const int list1_index, const Color c) {
		for (u32 i = 1; i <= hand.numOf(hp); ++i) {
			list0[nlist] = list0_index + i;
			list1[nlist] = list1_index + i;
			const Square squarehand = HandPieceToSquareHand[c][hp] + static_cast<Square>(i);
			listToSquareHand[nlist] = squarehand;
			squareHandToList[squarehand] = nlist;
			++nlist;
		}
	};
	func(handB, HPawn  , f_hand_pawn  , e_hand_pawn  , Black);
	func(handW, HPawn  , e_hand_pawn  , f_hand_pawn  , White);
	func(handB, HLance , f_hand_lance , e_hand_lance , Black);
	func(handW, HLance , e_hand_lance , f_hand_lance , White);
	func(handB, HKnight, f_hand_knight, e_hand_knight, Black);
	func(handW, HKnight, e_hand_knight, f_hand_knight, White);
	func(handB, HSilver, f_hand_silver, e_hand_silver, Black);
	func(handW, HSilver, e_hand_silver, f_hand_silver, White);
	func(handB, HGold  , f_hand_gold  , e_hand_gold  , Black);
	func(handW, HGold  , e_hand_gold  , f_hand_gold  , White);
	func(handB, HBishop, f_hand_bishop, e_hand_bishop, Black);
	func(handW, HBishop, e_hand_bishop, f_hand_bishop, White);
	func(handB, HRook  , f_hand_rook  , e_hand_rook  , Black);
	func(handW, HRook  , e_hand_rook  , f_hand_rook  , White);

	Bitboard bb = pos.bbOf(King).notThisAnd(pos.occupiedBB());
	while (bb.isNot0()) {
		const Square sq = bb.firstOneFromSQ11();
		const Piece pc = pos.piece(sq);
		listToSquareHand[nlist] = sq;
		squareHandToList[sq] = nlist;
		list0[nlist  ] = kppArray[pc         ] + sq;
		list1[nlist++] = kppArray[inverse(pc)] + inverse(sq);
	}
}
