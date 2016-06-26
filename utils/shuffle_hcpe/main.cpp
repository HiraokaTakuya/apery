#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <cinttypes>

struct HuffmanCodedPosAndEval {
	char c[32];
	int16_t e;
};
static_assert(sizeof(HuffmanCodedPosAndEval) == 34, "");

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cout << "USAGE: " << argv[0] << " <input hcpe> <output shuffled hcpe>\n" << std::endl;
		return 0;
	}
	std::ifstream ifs(argv[1], std::ios::binary);
	const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg); // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする
	std::vector<HuffmanCodedPosAndEval> buf(fileSize/sizeof(HuffmanCodedPosAndEval));
	ifs.read(reinterpret_cast<char*>(buf.data()), fileSize);
	std::mt19937_64 mt(std::chrono::system_clock::now().time_since_epoch().count());
	std::shuffle(std::begin(buf), std::end(buf), mt);
	std::ofstream ofs(argv[2], std::ios::binary);
	ofs.write(reinterpret_cast<char*>(buf.data()), fileSize);
}
