#ifndef APERY_COLOR_HPP
#define APERY_COLOR_HPP

#include "overloadEnumOperators.hpp"

enum Color {
	Black, White, ColorNum
};
OverloadEnumOperators(Color);

inline constexpr Color oppositeColor(const Color c) {
	return static_cast<Color>(static_cast<int>(c) ^ 1);
}

#endif // #ifndef APERY_COLOR_HPP
