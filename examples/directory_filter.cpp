#define BSTB_IMPL
#include "../bootstrab.hpp"

using namespace bstb;

auto main() -> int {
  auto filter = fs::filter("src/", [](auto&& entry) {
    return entry.path().extension() == ".cpp";
  });

  cmd("echo", filter).run({ .pipe = Pipe::Inherited() });
}
