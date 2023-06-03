#define BOOTSTRAB_IMPLEMENTATION
#include "../bootstrab.hpp"

using namespace bootstrab;

auto main() -> int {
  auto filter = fs::filter("src/", [](const std::fs::path &path) {
    return path.extension() == ".cpp";
  });

  Command::from("echo", filter)
      .run({
          .pipe = Pipe::Inherited(),
      });
}
