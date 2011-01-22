#ifndef USI_HPP
#define USI_HPP

#include "common.hpp"
#include "move.hpp"

const std::string DefaultStartPositionSFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

struct OptionsMap;

class USIOption {
	typedef void (Fn)(const USIOption&);
public:
	USIOption(Fn* = nullptr);
	USIOption(const char* v, Fn* = nullptr);
	USIOption(const bool v, Fn* = nullptr);
	USIOption(const int v, const int min, const int max, Fn* = nullptr);

	USIOption& operator = (const std::string& v);

	operator int() const {
		assert(type_ == "check" || type_ == "spin");
		return (type_ == "spin" ? atoi(currentValue_.c_str()) : currentValue_ == "true");
	}

	operator std::string() const {
		assert(type_ == "string");
		return currentValue_;
	}

private:
	friend std::ostream& operator << (std::ostream&, const OptionsMap&);

	std::string defaultValue_;
	std::string currentValue_;
	std::string type_;
	int min_;
	int max_;
	size_t idx_;
	Fn* onChange_;
};

struct CaseInsensitiveLess {
	bool operator() (const std::string&, const std::string&) const;
};

struct OptionsMap : public std::map<std::string, USIOption, CaseInsensitiveLess> {
public:
	OptionsMap();
	bool isLegalOption(const std::string name) {
		// count(key) は key が登場する回数を返す。map は重複しないので、count は常に 0 か 1 を返す。
		return this->count(name);
	}
};

extern OptionsMap g_options;

void doUSICommandLoop(int argc, char* argv[]);
Move csaToMove(const Position& pos, const std::string& moveStr);
Move usiToMove(const Position& pos, const std::string& moveStr);

#endif // #ifndef USI_HPP
