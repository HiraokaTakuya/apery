#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <chrono>
#include <cinttypes>

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::cout << "USAGE: " << argv[0] << " <input teacher file> <output shuffled teacher file>\n" << std::endl;
		return 0;
	}

	// 教師データは4行で1組になっているので、4行の連続は維持したままでシャッフルする。
	// ファイルが大き過ぎてメモリに乗らないので、逐次読み込んで書き込む。
	const int EntryLineNum = 4; // 4行1組。
	std::ifstream ifs(argv[1], std::ios::binary);
	std::ofstream ofs(argv[2], std::ios::binary);

	std::vector<std::pair<int64_t, int64_t>> indices; // first: 4 lines index,  second: char index
	{
		char c;
		int ei = 0;
		indices.emplace_back(0, 0);
		for (int64_t ci = 0; ifs.get(c); ++ci)
			if (c == '\n') {
				++ei;
				if (ei == EntryLineNum) {
					ei = 0;
					indices.emplace_back(indices.size(), ci+1);
				}
			}
		// ファイルの最後はエントリーとして不要なので削除
		indices.erase(std::end(indices)-1, std::end(indices));
	}
	ifs.clear();
	ifs.seekg(0, std::ios::beg);
	std::mt19937_64 mt(std::chrono::system_clock::now().time_since_epoch().count());
	std::shuffle(std::begin(indices), std::end(indices), mt);
	std::string str[EntryLineNum];
	for (auto lcpair : indices) {
		ifs.seekg(lcpair.second, std::ios::beg);
		for (int i = 0; i < EntryLineNum; ++i)
			std::getline(ifs, str[i]);
		for (int i = 0; i < EntryLineNum; ++i)
			ofs << str[i] << "\n";
	}
}
