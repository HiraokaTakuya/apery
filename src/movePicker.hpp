/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2016 Hiraoka Takuya

  Apery is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Apery is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef APERY_MOVEPICKER_HPP
#define APERY_MOVEPICKER_HPP

#include "move.hpp"
#include "position.hpp"
#include "search.hpp"

enum GenerateMovePhase {
	MainSearch, PH_TacticalMoves0, PH_Killers, PH_NonTacticalMoves0, PH_NonTacticalMoves1, PH_BadCaptures,
	EvasionSearch, PH_Evasions,
	QSearch, PH_QCaptures0,
	QEvasionSearch, PH_QEvasions,
	ProbCut, PH_TacticalMoves1,
	QRecapture, PH_QCaptures1,
	QSearchNoTT, PH_QCaptures2,
	PH_Stop
};
OverloadEnumOperators(GenerateMovePhase); // ++phase_ の為。

class MovePicker {
public:
	MovePicker(const Position& pos, const Move ttm, const Depth depth,
			   /*const History& history, */SearchStack* searchStack, const Score beta);
	MovePicker(const Position& pos, Move ttm, const Depth depth, /*const History& history, */const Square sq);
	MovePicker(const Position& pos, const Move ttm, /*const History& history, */const PieceType pt);
	Move nextMove();

private:
	void scoreCaptures();
	template <bool IsDrop> void scoreNonCapturesMinusPro();
	void scoreEvasions();
	void goNextPhase();
	ExtMove* firstMove() { return &legalMoves_[1]; } // [0] は番兵
	ExtMove* currMove() const { return currMove_; }
	ExtMove* lastMove() const { return lastMove_; }
	ExtMove* lastNonCapture() const { return lastNonCapture_; }
	ExtMove* endBadCaptures() const { return endBadCaptures_; }

	const Position& pos() const { return pos_; }

	GenerateMovePhase phase() const { return phase_; }
	//const History& history() const { return history_; }

	const Position& pos_;
	//const History& history_;
	SearchStack* ss_;
	Depth depth_;
	Move ttMove_; // transposition table move
	ExtMove killerMoves_[2];
	Square recaptureSquare_;
	int captureThreshold_; // int で良いのか？
	GenerateMovePhase phase_;
	ExtMove* currMove_;
	ExtMove* lastMove_;
	ExtMove* lastNonCapture_;
	ExtMove* endBadCaptures_;
	// std::array にした方が良さそう。
	ExtMove legalMoves_[MaxLegalMoves];
};

#endif // #ifndef APERY_MOVEPICKER_HPP
