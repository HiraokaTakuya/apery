#include "common.hpp"
#include "bitboard.hpp"
#include "init.hpp"
#include "position.hpp"
#include "usi.hpp"
#include "thread.hpp"
#include "tt.hpp"
#include "search.hpp"

#if defined FIND_MAGIC
// Magic Bitboard の Magic Number を求める為のソフト
int main() {
	u64 RookMagic[SquareNum];
	u64 BishopMagic[SquareNum];

	std::cout << "const u64 RookMagic[81] = {" << std::endl;
	for (Square sq = SQ11; sq < SquareNum; ++sq) {
		RookMagic[sq] = findMagic(sq, false);
		std::cout << "\tUINT64_C(0x" << std::hex << RookMagic[sq] << ")," << std::endl;
	}
	std::cout << "};\n" << std::endl;

	std::cout << "const u64 BishopMagic[81] = {" << std::endl;
	for (Square sq = SQ11; sq < SquareNum; ++sq) {
		BishopMagic[sq] = findMagic(sq, true);
		std::cout << "\tUINT64_C(0x" << std::hex << BishopMagic[sq] << ")," << std::endl;
	}
	std::cout << "};\n" << std::endl;

	return 0;
}

#else
// 将棋を指すソフト
int main(int argc, char* argv[]) {
	initTable();
	Position::initZobrist();
	auto s = std::unique_ptr<Searcher>(new Searcher);
	s->init();
	s->doUSICommandLoop(argc, argv);
	s->threads.exit();
}

#endif
