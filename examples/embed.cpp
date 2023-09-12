#ifndef BSTB_IMPL
#define BSTB_IMPL
#endif
#include "../bootstrab.hpp"

using namespace bstb;

// Uses the embedded file, Uncomment after first run
// namespace test {
//
// #define BSTB_EMBED
// #include "src/test.hpp"
//
// constexpr static auto str = std::string_view { data<char>, size }; 
//
// } // namespace test

auto main(int argc, char** argv) -> int {
  REBUILD_URSELF(argc, argv);

  // Embeds the text file into a header file.
  // Comment out after first run
  embedder::cpp("src/test.txt", "src/test.hpp");

  // Prints out the embedded text
  // Uncomment after first run
  // std::cout << test::str;
}
