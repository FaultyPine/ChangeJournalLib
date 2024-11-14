#pragma once

#include <array>

typedef signed long long int64;

constexpr size_t cWin32MaxPathSizeUTF16 = 32768;
constexpr size_t cWin32MaxPathSizeUTF8  = 32768 * 3ull;	// UTF8 can use up to 6 bytes per character, but let's suppose 3 is good enough on average.

using PathBufferUTF16 = std::array<wchar_t, cWin32MaxPathSizeUTF16>;
using PathBufferUTF8  = std::array<char, cWin32MaxPathSizeUTF8>;
