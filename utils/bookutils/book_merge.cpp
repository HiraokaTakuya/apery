#include <iostream>
#include <fstream>
#include <cinttypes>
#include <vector>
#include <map>
#include <algorithm>

struct BookEntry {
	uint64_t key;
	uint16_t fromToPro;
	uint16_t count;
	int score;
};

template <typename T>
struct comp {
	explicit comp(const T& a) : a_(a) {}
	bool operator () (const T& b) const {
		return a_.key == b.key && a_.fromToPro == b.fromToPro;
	}
	const T& a_;
};

inline bool countCompare(const BookEntry& b1, const BookEntry& b2) {
	return b1.count < b2.count;
}

int main(int argc, char *argv[]) {
	std::ifstream ifs0("bookbin/0book.bin", std::ios::binary);
	std::ifstream ifs1("bookbin/1book.bin", std::ios::binary);
	std::ifstream ifs2("bookbin/2book.bin", std::ios::binary);
	std::ifstream ifs3("bookbin/3book.bin", std::ios::binary);
	std::ifstream ifs4("bookbin/4book.bin", std::ios::binary);
	std::ifstream ifs5("bookbin/5book.bin", std::ios::binary);
	std::ifstream ifs6("bookbin/6book.bin", std::ios::binary);
	std::ifstream ifs7("bookbin/7book.bin", std::ios::binary);

	const size_t fileSize0 = static_cast<size_t>(ifs0.seekg(0, std::ios::end).tellg());
	const size_t fileSize1 = static_cast<size_t>(ifs1.seekg(0, std::ios::end).tellg());
	const size_t fileSize2 = static_cast<size_t>(ifs2.seekg(0, std::ios::end).tellg());
	const size_t fileSize3 = static_cast<size_t>(ifs3.seekg(0, std::ios::end).tellg());
	const size_t fileSize4 = static_cast<size_t>(ifs4.seekg(0, std::ios::end).tellg());
	const size_t fileSize5 = static_cast<size_t>(ifs5.seekg(0, std::ios::end).tellg());
	const size_t fileSize6 = static_cast<size_t>(ifs6.seekg(0, std::ios::end).tellg());
	const size_t fileSize7 = static_cast<size_t>(ifs7.seekg(0, std::ios::end).tellg());

	ifs0.seekg(0, std::ios::beg);
	ifs1.seekg(0, std::ios::beg);
	ifs2.seekg(0, std::ios::beg);
	ifs3.seekg(0, std::ios::beg);
	ifs4.seekg(0, std::ios::beg);
	ifs5.seekg(0, std::ios::beg);
	ifs6.seekg(0, std::ios::beg);
	ifs7.seekg(0, std::ios::beg);

	std::vector<BookEntry> vec0(fileSize0/sizeof(BookEntry));
	std::vector<BookEntry> vec1(fileSize1/sizeof(BookEntry));
	std::vector<BookEntry> vec2(fileSize2/sizeof(BookEntry));
	std::vector<BookEntry> vec3(fileSize3/sizeof(BookEntry));
	std::vector<BookEntry> vec4(fileSize4/sizeof(BookEntry));
	std::vector<BookEntry> vec5(fileSize5/sizeof(BookEntry));
	std::vector<BookEntry> vec6(fileSize6/sizeof(BookEntry));
	std::vector<BookEntry> vec7(fileSize7/sizeof(BookEntry));

	ifs0.read(reinterpret_cast<char*>(&vec0[0]), fileSize0);
	ifs1.read(reinterpret_cast<char*>(&vec1[0]), fileSize1);
	ifs2.read(reinterpret_cast<char*>(&vec2[0]), fileSize2);
	ifs3.read(reinterpret_cast<char*>(&vec3[0]), fileSize3);
	ifs4.read(reinterpret_cast<char*>(&vec4[0]), fileSize4);
	ifs5.read(reinterpret_cast<char*>(&vec5[0]), fileSize5);
	ifs6.read(reinterpret_cast<char*>(&vec6[0]), fileSize6);
	ifs7.read(reinterpret_cast<char*>(&vec7[0]), fileSize7);

	std::map<uint64_t, std::vector<BookEntry> > map0;
	std::map<uint64_t, std::vector<BookEntry> > map1;
	std::map<uint64_t, std::vector<BookEntry> > map2;
	std::map<uint64_t, std::vector<BookEntry> > map3;
	std::map<uint64_t, std::vector<BookEntry> > map4;
	std::map<uint64_t, std::vector<BookEntry> > map5;
	std::map<uint64_t, std::vector<BookEntry> > map6;
	std::map<uint64_t, std::vector<BookEntry> > map7;

	for (auto elem : vec0) map0[elem.key].push_back(elem);
	for (auto elem : vec1) map1[elem.key].push_back(elem);
	for (auto elem : vec2) map2[elem.key].push_back(elem);
	for (auto elem : vec3) map3[elem.key].push_back(elem);
	for (auto elem : vec4) map4[elem.key].push_back(elem);
	for (auto elem : vec5) map5[elem.key].push_back(elem);
	for (auto elem : vec6) map6[elem.key].push_back(elem);
	for (auto elem : vec7) map7[elem.key].push_back(elem);

#define FOO(mapn) \
	for (auto& elem : mapn) {					\
		for (const auto& elel : elem.second) {							\
			auto it = std::find_if(std::begin(map0[elel.key]), std::end(map0[elel.key]), comp<BookEntry>(elel)); \
			if (it != std::end(map0[elel.key])) {						\
				it->count += elel.count;								\
			}															\
			else														\
				map0[elel.key].push_back(elel);							\
		}																\
	}
	FOO(map1);
	FOO(map2);
	FOO(map3);
	FOO(map4);
	FOO(map5);
	FOO(map6);
	FOO(map7);

	for (auto& elem : map0)
		std::sort(elem.second.rbegin(), elem.second.rend(), countCompare);

	std::ofstream ofs("book.bin", std::ios::binary);
	for (auto& elem : map0) {
		for (auto& elel : elem.second) {
			ofs.write(reinterpret_cast<char*>(&elel), sizeof(BookEntry));
		}
	}
}
