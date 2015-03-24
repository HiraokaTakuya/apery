#ifndef APERY_BOOK_HPP
#define APERY_BOOK_HPP

#include "position.hpp"
#include "mt64bit.hpp"

struct BookEntry {
	Key key;
	u16 fromToPro;
	u16 count;
	Score score;
};

class Book : private std::ifstream {
public:
	Book() : random_(std::chrono::system_clock::now().time_since_epoch().count()) {}
	std::tuple<Move, Score> probe(const Position& pos, const std::string& fName, const bool pickBest);
	static void init();
	static Key bookKey(const Position& pos);

private:
	bool open(const char* fName);
	void binary_search(const Key key);

	static MT64bit mt64bit_; // 定跡のhash生成用なので、seedは固定でデフォルト値を使う。
	MT64bit random_; // 時刻をseedにして色々指すようにする。
	std::string fileName_;
	size_t size_;

	static Key ZobPiece[PieceNone][SquareNum];
	static Key ZobHand[HandPieceNum][19];
	static Key ZobTurn;
};

void makeBook(Position& pos, std::istringstream& ssCmd);

#endif // #ifndef APERY_BOOK_HPP
