#define BSTB_IMPL 
#include "../bootstrab.hpp"

using namespace bstb;

auto clone_into(const CStr path) -> decltype(auto) {
  std::filesystem::create_directory(path);
  return cmd("git", "clone", "https://github.com/linux-techtips/bootstrab.git", path);
}

auto main() -> int {

  const auto config = Config{
      .pipe = Pipe::Inherited(),
      .verbose = true,
  };

  auto future = TaskList{ .tasks = {
      clone_into("/tmp/bootstrab1").run_async(config),
      clone_into("/tmp/bootstrab2").run_async(config),
      clone_into("/tmp/bootstrab3").run_async(config),
  }};

  future.wait();
}
