#ifndef APERY_EVALLIST_HPP
#define APERY_EVALLIST_HPP

#include "square.hpp"
#include "piece.hpp"

class Position;

struct EvalList {
	static const int ListSize = 38;

	int list0[ListSize];
	int list1[ListSize];
	Square listToSquareHand[ListSize];
	int squareHandToList[SquareHandNum];

	void set(const Position& pos);
};

extern const Square HandPieceToSquareHand[ColorNum][HandPieceNum];

#endif // #ifndef APERY_EVALLIST_HPP
