#include "benchmark.hpp"
#include "common.hpp"
#include "usi.hpp"
#include "position.hpp"
#include "search.hpp"

void setPosition(Position& pos, std::istringstream& ssCmd);
void go(const Position& pos, std::istringstream& ssCmd);

// 今はベンチマークというより、PGO ビルドの自動化の為にある。
void benchmark(Position& pos) {
	std::string token;
	LimitsType limits;

	g_options["Threads"] = std::string("1");

	std::ifstream ifs("benchmark.sfen");
	std::string sfen;
	while (std::getline(ifs, sfen)) {
		std::cout << sfen << std::endl;
		std::istringstream ss_sfen(sfen);
		setPosition(pos, ss_sfen);
		std::istringstream ss_go("byoyomi 10000");
		go(pos, ss_go);
	}
}
