#pragma once

#include <stddef.h>

namespace adapt {

using Char = char;

struct CodeSource {
  size_t line;
  size_t column;
};

}  // namespace adapt