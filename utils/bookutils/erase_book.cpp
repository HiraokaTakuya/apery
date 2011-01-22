#include <iostream>
#include <fstream>
#include <vector>
#include <cinttypes>

struct BookEntry {
	uint64_t key;
	uint16_t fromToPro;
	uint16_t count;
	int32_t score;
};

int main(int argc, char *argv[]) {
	std::ifstream ifs("../../bin/book.bin");
	const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg);     // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする

	if (fileSize % sizeof(BookEntry) != 0) {
		std::cout << "arere?" << std::endl;
		return 0;
	}
	std::vector<BookEntry> vec(fileSize/sizeof(BookEntry));
	ifs.read(reinterpret_cast<char*>(&vec[0]), fileSize);

	while (true) {
		auto it = std::begin(vec);
		for (; it != std::end(vec); ++it) {
			if (it->key == 18050028689871886544ull && it->fromToPro == 8386) { vec.erase(it); break; } // 2726fu3334fu7776fu 8384fu
			if (it->key == 18050028689871886544ull && it->fromToPro == 4903) { vec.erase(it); break; } // 2726fu3334fu7776fu 5354fu
			if (it->key == 8222405343924515057ull && it->fromToPro == 8386)  { vec.erase(it); break; } // 2726fu 8384fu
			if (it->key == 4502226897143472341ull && it->fromToPro == 8386)  { vec.erase(it); break; } // 7776fu 8384fu
		}
		if (it == std::end(vec)) break;
	}
	std::ofstream ofs("book.bin", std::ios::binary);
	ofs.write(reinterpret_cast<char*>(&vec[0]), vec.size()*sizeof(BookEntry));
}
