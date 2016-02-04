#include "book.hpp"
#include "position.hpp"
#include "move.hpp"
#include "usi.hpp"
#include "thread.hpp"
#include "search.hpp"

MT64bit Book::mt64bit_; // 定跡のhash生成用なので、seedは固定でデフォルト値を使う。
Key Book::ZobPiece[PieceNone][SquareNum];
Key Book::ZobHand[HandPieceNum][19]; // 持ち駒の同一種類の駒の数ごと
Key Book::ZobTurn;

void Book::init() {
	for (Piece p = Empty; p < PieceNone; ++p) {
		for (Square sq = SQ11; sq < SquareNum; ++sq)
			ZobPiece[p][sq] = mt64bit_.random();
	}
	for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp) {
		for (int num = 0; num < 19; ++num)
			ZobHand[hp][num] = mt64bit_.random();
	}
	ZobTurn = mt64bit_.random();
}

bool Book::open(const char* fName) {
	fileName_ = "";

	if (is_open())
		close();

	std::ifstream::open(fName, std::ifstream::in | std::ifstream::binary | std::ios::ate);

	if (!is_open())
		return false;

	size_ = tellg() / sizeof(BookEntry);

	if (!good()) {
		std::cerr << "Failed to open book file " << fName  << std::endl;
		exit(EXIT_FAILURE);
	}

	fileName_ = fName;
	return true;
}

void Book::binary_search(const Key key) {
	size_t low = 0;
	size_t high = size_ - 1;
	size_t mid;
	BookEntry entry;

	while (low < high && good()) {
		mid = (low + high) / 2;

		assert(mid >= low && mid < high);

		// std::ios_base::beg はストリームの開始位置を指す。
		// よって、ファイルの開始位置から mid * sizeof(BookEntry) バイト進んだ位置を指す。
		seekg(mid * sizeof(BookEntry), std::ios_base::beg);
		read(reinterpret_cast<char*>(&entry), sizeof(entry));

		if (key <= entry.key)
			high = mid;
		else
			low = mid + 1;
	}

	assert(low == high);

	seekg(low * sizeof(BookEntry), std::ios_base::beg);
}

Key Book::bookKey(const Position& pos) {
	Key key = 0;
	Bitboard bb = pos.occupiedBB();

	while (bb.isNot0()) {
		const Square sq = bb.firstOneFromSQ11();
		key ^= ZobPiece[pos.piece(sq)][sq];
	}
	const Hand hand = pos.hand(pos.turn());
	for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp)
		key ^= ZobHand[hp][hand.numOf(hp)];
	if (pos.turn() == White)
		key ^= ZobTurn;
	return key;
}

std::tuple<Move, Score> Book::probe(const Position& pos, const std::string& fName, const bool pickBest) {
	BookEntry entry;
	u16 best = 0;
	u32 sum = 0;
	Move move = Move::moveNone();
	const Key key = bookKey(pos);
	const Score min_book_score = static_cast<Score>(static_cast<int>(pos.searcher()->options["Min_Book_Score"]));
	Score score = ScoreZero;

	if (fileName_ != fName && !open(fName.c_str()))
		return std::make_tuple(Move::moveNone(), ScoreNone);

	binary_search(key);

	// 現在の局面における定跡手の数だけループする。
	while (read(reinterpret_cast<char*>(&entry), sizeof(entry)), entry.key == key && good()) {
		best = std::max(best, entry.count);
		sum += entry.count;

		// 指された確率に従って手が選択される。
		// count が大きい順に並んでいる必要はない。
		if (min_book_score <= entry.score
			&& ((random_.random() % sum < entry.count)
				|| (pickBest && entry.count == best)))
		{
			const Move tmp = Move(entry.fromToPro);
			const Square to = tmp.to();
			if (tmp.isDrop()) {
				const PieceType ptDropped = tmp.pieceTypeDropped();
				move = makeDropMove(ptDropped, to);
			}
			else {
				const Square from = tmp.from();
				const PieceType ptFrom = pieceToPieceType(pos.piece(from));
				const bool promo = tmp.isPromotion();
				if (promo)
					move = makeCapturePromoteMove(ptFrom, from, to, pos);
				else
					move = makeCaptureMove(ptFrom, from, to, pos);
			}
			score = entry.score;
		}
	}

	return std::make_tuple(move, score);
}

inline bool countCompare(const BookEntry& b1, const BookEntry& b2) {
	return b1.count < b2.count;
}

#if !defined MINIMUL
// 以下のようなフォーマットが入力される。
// <棋譜番号> <日付> <先手名> <後手名> <0:引き分け, 1:先手勝ち, 2:後手勝ち> <総手数> <棋戦名前> <戦形>
// <CSA1行形式の指し手>
//
// (例)
// 1 2003/09/08 羽生善治 谷川浩司 2 126 王位戦 その他の戦型
// 7776FU3334FU2726FU4132KI
//
// 勝った方の手だけを定跡として使うこととする。
// 出現回数がそのまま定跡として使う確率となる。
// 基本的には棋譜を丁寧に選別した上で定跡を作る必要がある。
// MAKE_SEARCHED_BOOK を on にしていると、定跡生成に非常に時間が掛かる。
void makeBook(Position& pos, std::istringstream& ssCmd) {
	std::string fileName;
	ssCmd >> fileName;
	std::ifstream ifs(fileName.c_str(), std::ios::binary);
	if (!ifs) {
		std::cout << "I cannot open " << fileName << std::endl;
		return;
	}
	std::string line;
	std::map<Key, std::vector<BookEntry> > bookMap;

	while (std::getline(ifs, line)) {
		std::string elem;
		std::stringstream ss(line);
		ss >> elem; // 棋譜番号を飛ばす。
		ss >> elem; // 対局日を飛ばす。
		ss >> elem; // 先手
		const std::string sente = elem;
		ss >> elem; // 後手
		const std::string gote = elem;
		ss >> elem; // (0:引き分け,1:先手の勝ち,2:後手の勝ち)
		const Color winner = (elem == "1" ? Black : elem == "2" ? White : ColorNum);
		// 勝った方の指し手を記録していく。
		// 又は稲庭戦法側を記録していく。
		const Color saveColor = winner;

		if (!std::getline(ifs, line)) {
			std::cout << "!!! header only !!!" << std::endl;
			return;
		}
		pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
		StateStackPtr SetUpStates = StateStackPtr(new std::stack<StateInfo>());
		while (!line.empty()) {
			const std::string moveStrCSA = line.substr(0, 6);
			const Move move = csaToMove(pos, moveStrCSA);
			if (move.isNone()) {
				pos.print();
				std::cout << "!!! Illegal move = " << moveStrCSA << " !!!" << std::endl;
				break;
			}
			line.erase(0, 6); // 先頭から6文字削除
			if (pos.turn() == saveColor) {
				// 先手、後手の内、片方だけを記録する。
				const Key key = Book::bookKey(pos);
				bool isFind = false;
				if (bookMap.find(key) != bookMap.end()) {
					for (std::vector<BookEntry>::iterator it = bookMap[key].begin();
						 it != bookMap[key].end();
						 ++it)
					{
						if (it->fromToPro == move.proFromAndTo()) {
							++it->count;
							if (it->count < 1)
								--it->count; // 数えられる数の上限を超えたので元に戻す。
							isFind = true;
						}
					}
				}
				if (isFind == false) {
#if defined MAKE_SEARCHED_BOOK
					SetUpStates->push(StateInfo());
					pos.doMove(move, SetUpStates->top());

					std::istringstream ssCmd("byoyomi 1000");
					go(pos, ssCmd);

					pos.undoMove(move);
					SetUpStates->pop();

					// doMove してから search してるので点数が反転しているので直す。
					const Score score = -pos.thisThread()->rootMoves[0].score_;
#else
					const Score score = ScoreZero;
#endif
					// 未登録の手
					BookEntry be;
					be.score = score;
					be.key = key;
					be.fromToPro = static_cast<u16>(move.proFromAndTo());
					be.count = 1;
					bookMap[key].push_back(be);
				}
			}
			SetUpStates->push(StateInfo());
			pos.doMove(move, SetUpStates->top());
		}
	}

	// BookEntry::count の値で降順にソート
	for (auto& elem : bookMap) {
		std::sort(elem.second.rbegin(), elem.second.rend(), countCompare);
	}

#if 0
	// 2 回以上棋譜に出現していない手は削除する。
	for (auto& elem : bookMap) {
		auto& second = elem.second;
		auto erase_it = std::find_if(second.begin(), second.end(), [](decltype(*second.begin())& second_elem) { return second_elem.count < 2; });
		second.erase(erase_it, second.end());
	}
#endif

#if 0
	// narrow book
	for (auto& elem : bookMap) {
		auto& second = elem.second;
		auto erase_it = std::find_if(second.begin(), second.end(), [&](decltype(*second.begin())& second_elem) { return second_elem.count < second[0].count / 2; });
		second.erase(erase_it, second.end());
	}
#endif

	std::ofstream ofs("book.bin", std::ios::binary);
	for (auto& elem : bookMap) {
		for (auto& elel : elem.second)
			ofs.write(reinterpret_cast<char*>(&(elel)), sizeof(BookEntry));
	}

	std::cout << "book making was done" << std::endl;
}
#endif
