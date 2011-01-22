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
	std::ifstream ifs("../bin/searched_book.bin");
	const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
	ifs.seekg(0, std::ios::beg);     // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする

	if (fileSize % sizeof(BookEntry) != 0) {
		std::cout << "arere?" << std::endl;
		return 0;
	}
	std::vector<BookEntry> vec(fileSize/sizeof(BookEntry));
	ifs.read(reinterpret_cast<char*>(&vec[0]), fileSize);

	std::ifstream ifs2("../bin/tai_doukaku_book.bin");
	const size_t fileSize2 = static_cast<size_t>(ifs2.seekg(0, std::ios::end).tellg());
	ifs2.seekg(0, std::ios::beg);     // ストリームのポインタを一番前に戻して、これから先で使いやすいようにする
	std::vector<BookEntry> vec2(fileSize2/sizeof(BookEntry));
	ifs2.read(reinterpret_cast<char*>(&vec2[0]), fileSize2);

	for (auto& elem : vec2) {
		for (auto& ref : vec) {
			if (elem.key == ref.key && elem.fromToPro == ref.fromToPro) {
				elem.score = ref.score;
				break;
			}
		}
	}

	std::ofstream ofs("book.bin", std::ios::binary);
	ofs.write(reinterpret_cast<char*>(&vec2[0]), fileSize2);
}
