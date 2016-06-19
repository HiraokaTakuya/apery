#include "movePicker.hpp"
#include "generateMoves.hpp"
#include "thread.hpp"

MovePicker::MovePicker(const Position& pos, const Move ttm, const Depth depth,
					   const History& history, SearchStack* searchStack, const Score beta)
	: pos_(pos), history_(history), depth_(depth)
{
	assert(Depth0 < depth);

	legalMoves_[0].score = INT_MAX; // 番兵のセット
	currMove_ = lastMove_ = firstMove();
	captureThreshold_ = 0;
	endBadCaptures_ = legalMoves_ + MaxLegalMoves - 1;
	ss_ = searchStack;

	if (pos.inCheck())
		phase_ = EvasionSearch;
	else {
		phase_ = MainSearch;

		killerMoves_[0].move = searchStack->killers[0];
		killerMoves_[1].move = searchStack->killers[1];

		if (ss_ != nullptr && ss_->staticEval < beta - CapturePawnScore && depth < 3 * OnePly)
			captureThreshold_ = -CapturePawnScore;
		else if (ss_ != nullptr && beta < ss_->staticEval)
			captureThreshold_ = beta - ss_->staticEval;
	}

	ttMove_ = (!ttm.isNone() && pos.moveIsPseudoLegal(ttm) ? ttm : Move::moveNone());
	lastMove_ += (!ttMove_.isNone());
}

// 静止探索で呼ばれる。
MovePicker::MovePicker(const Position& pos, Move ttm, const Depth depth, const History& history, const Square sq)
	: pos_(pos), history_(history), currMove_(firstMove()), lastMove_(firstMove())
{
	assert(depth <= Depth0);
	legalMoves_[0].score = INT_MAX; // 番兵のセット

	if (pos.inCheck())
		phase_ = QEvasionSearch;
	// todo: ここで Stockfish は qcheck がある。
	else if (DepthQNoTT < depth)
		phase_ = QSearch;
	else if (DepthQRecaptures < depth) {
		phase_ = QSearchNoTT;
		ttm = Move::moveNone();
	}
	else {
		phase_ = QRecapture;
		recaptureSquare_ = sq;
		ttm = Move::moveNone();
	}

	ttMove_ = (!ttm.isNone() && pos.moveIsPseudoLegal(ttm) ? ttm : Move::moveNone());
	lastMove_ += !ttMove_.isNone();
}

MovePicker::MovePicker(const Position& pos, const Move ttm, const History& history, const PieceType pt)
	: pos_(pos), history_(history), currMove_(firstMove()), lastMove_(firstMove())
{
	assert(!pos.inCheck());

	legalMoves_[0].score = INT_MAX; // 番兵のセット
	phase_ = ProbCut;

	captureThreshold_ = pos.capturePieceScore(pt);
	ttMove_ = ((!ttm.isNone() && pos.moveIsPseudoLegal(ttm)) ? ttm : Move::moveNone());

	if (!ttMove_.isNone() && (!ttMove_.isCapture() || pos.see(ttMove_) <= captureThreshold_))
		ttMove_ = Move::moveNone();

	lastMove_ += !ttMove_.isNone();
}

Move MovePicker::nextMove() {
	MoveStack* ms;
	Move move;
	do {
		// lastMove() に達したら次の phase に移る。
		while (currMove() == lastMove())
			goNextPhase();

		switch (phase()) {

		case MainSearch: case EvasionSearch: case QSearch: case QEvasionSearch: case ProbCut:
			++currMove_;
			return ttMove_;

		case PH_TacticalMoves0:
			ms = pickBest(currMove_++, lastMove());
			if (ms->move != ttMove_) {
				assert(captureThreshold_ <= 0);
				if (captureThreshold_ <= pos().see(ms->move))
					return ms->move;
				// 後ろから SEE の点数が高い順に並ぶようにする。
				(endBadCaptures_--)->move = ms->move;
			}
			break;

		case PH_Killers:
			move = (currMove_++)->move;
			if (!move.isNone()
				&& move != ttMove_
				&& pos().moveIsPseudoLegal(move)
				&& pos().piece(move.to()) == Empty)
			{
				return move;
			}
			break;

		case PH_NonTacticalMoves0:
		case PH_NonTacticalMoves1:
			move = (currMove_++)->move;
			if (move != ttMove_
				&& move != killerMoves_[0].move
				&& move != killerMoves_[1].move)
			{
				return move;
			}
			break;

		case PH_BadCaptures:
			return (currMove_--)->move;

		case PH_Evasions: case PH_QEvasions: case PH_QCaptures0: case PH_QCaptures2:
			move = pickBest(currMove_++, lastMove())->move;
			if (move != ttMove_)
				return move;
			break;

		case PH_TacticalMoves1:
			ms = pickBest(currMove_++, lastMove());
			// todo: see が確実に駒打ちじゃないから、内部で駒打ちか判定してるのは少し無駄。
			if (ms->move != ttMove_ && captureThreshold_ < pos().see(ms->move))
				return ms->move;
			break;

		case PH_QCaptures1:
			move = pickBest(currMove_++, lastMove())->move;
			assert(move.to() == recaptureSquare_);
			return move;

		case PH_Stop:
			return Move::moveNone();
		default:
			UNREACHABLE;
		}
	} while (true);
}

const Score LVATable[PieceTypeNum] = {
	Score(0), Score(1), Score(2), Score(3), Score(4), Score(7), Score(8), Score(6), Score(10000),
	Score(5), Score(5), Score(5), Score(5), Score(9), Score(10)
};
inline Score LVA(const PieceType pt) { return LVATable[pt]; }

void MovePicker::scoreCaptures() {
	for (MoveStack* curr = currMove(); curr != lastMove(); ++curr) {
		const Move move = curr->move;
		curr->score = Position::pieceScore(pos().piece(move.to())) - LVA(move.pieceTypeFrom());
		if (move.isPromotion())
			++curr->score;
	}
}

template <bool IsDrop> void MovePicker::scoreNonCapturesMinusPro() {
	for (MoveStack* curr = currMove(); curr != lastMove(); ++curr) {
		const Move move = curr->move;
		curr->score =
			history().value(IsDrop,
							colorAndPieceTypeToPiece(pos().turn(),
													 (IsDrop ? move.pieceTypeDropped() : move.pieceTypeFrom())),
							move.to());
		if (!IsDrop && move.isPromotion())
			++curr->score;
	}
}

void MovePicker::scoreEvasions() {
	for (MoveStack* curr = currMove(); curr != lastMove(); ++curr) {
		const Move move = curr->move;
		const Score seeScore = pos().seeSign(move);
		if (seeScore < 0)
			curr->score = seeScore - History::MaxScore;
		else if (move.isCaptureOrPromotion()) {
			curr->score = pos().capturePieceScore(pos().piece(move.to())) + History::MaxScore;
			if (move.isPromotion()) {
				const PieceType pt = pieceToPieceType(pos().piece(move.from()));
				curr->score += pos().promotePieceScore(pt);
			}
		}
		else
			curr->score = history().value(move.isDrop(), colorAndPieceTypeToPiece(pos().turn(), move.pieceTypeFromOrDropped()), move.to());
	}
}

struct HasPositiveScore { bool operator () (const MoveStack& ms) { return 0 < ms.score; } };

void MovePicker::goNextPhase() {
	currMove_ = firstMove(); // legalMoves_[0] は番兵
	++phase_;

	switch (phase()) {
	case PH_TacticalMoves0: case PH_TacticalMoves1:
		lastMove_ = generateMoves<CapturePlusPro>(currMove(), pos());
		scoreCaptures();
		return;

	case PH_Killers:
		currMove_ = killerMoves_;
		lastMove_ = currMove() + 2;
		return;

	case PH_NonTacticalMoves0:
		lastMove_ = generateMoves<NonCaptureMinusPro>(currMove(), pos());
		scoreNonCapturesMinusPro<false>();
		currMove_ = lastMove();
		lastNonCapture_ = lastMove_ = generateMoves<Drop>(currMove(), pos());
		scoreNonCapturesMinusPro<true>();
		currMove_ = firstMove();
		lastMove_ = std::partition(currMove(), lastNonCapture(), HasPositiveScore());
		// 要素数は10個くらいまでであることが多い。要素数が少ないので、insertionSort() を使用する。
		insertionSort<MoveStack*, true>(currMove(), lastMove());
		return;

	case PH_NonTacticalMoves1:
		currMove_ = lastMove();
		lastMove_ = lastNonCapture();
		if (static_cast<Depth>(3 * OnePly) <= depth_)
			std::sort(currMove(), lastMove(), std::greater<MoveStack>());
		return;

	case PH_BadCaptures:
		currMove_ = legalMoves_ + MaxLegalMoves - 1;
		lastMove_ = endBadCaptures_;
		return;

	case PH_Evasions:
	case PH_QEvasions:
		lastMove_ = generateMoves<Evasion>(currMove(), pos());
		if (currMove() + 1 < lastMove())
			scoreEvasions();
		return;

	case PH_QCaptures0: case PH_QCaptures2:
		lastMove_ = generateMoves<CapturePlusPro>(firstMove(), pos());
		scoreCaptures();
		return;

	case PH_QCaptures1:
		lastMove_ = generateMoves<Recapture>(firstMove(), pos(), recaptureSquare_);
		scoreCaptures();
		return;

	case EvasionSearch: case QSearch: case QEvasionSearch: case QRecapture: case ProbCut: case QSearchNoTT:
		// これが無いと、MainSearch の後に EvasionSearch が始まったりしてしまう。
		phase_ = PH_Stop;
		// fallthrough

	case PH_Stop:
		lastMove_ = currMove() + 1;
		return;

	default: UNREACHABLE;
	}
}
