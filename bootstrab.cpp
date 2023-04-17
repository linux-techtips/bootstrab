//#define BOOTSTRAB_IMPLEMENTATION
#include "bootstrab.hpp"

auto main() -> int {
    using namespace bootstrab;

    // const auto cwd = std::fs::directory_iterator(std::fs::current_path());
    Command::from("echo", "Hello World").run({.pipe = Pipe::Inherited()});
}
