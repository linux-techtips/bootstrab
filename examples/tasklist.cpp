#define BOOTSTRAB_IMPLEMENTATION
#include "../bootstrab.hpp"
using namespace bootstrab;

auto clone_into(const CStr path) -> Command {
  std::fs::create_directory(path);
  return Command::from("git", "clone",
                       "https://github.com/linux-techtips/bootstrab.git", path);
}

auto main() -> int {

  const auto config = Command::Config{
      .pipe = Pipe::Inherited(),
      .verbose = true,
  };

  auto future = TaskList{
      clone_into("/tmp/bootstrab1").run_async(config),
      clone_into("/tmp/bootstrab2").run_async(config),
      clone_into("/tmp/bootstrab3").run_async(config),
  };

  future.wait();
}
