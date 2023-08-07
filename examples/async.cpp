#define BSTB_IMPL
#include "../bootstrab.hpp"

using namespace bstb;

auto main() -> int {
  const auto config = Config{
      .pipe = Pipe::Inherited(),
  };

  auto sleep_cmd = cmd("sleep", "3");

  sleep_cmd.run({
      .verbose = true,
  });
  cmd("echo", "Slept.\nSleeping async!").run(config);

  auto future = sleep_cmd.run_async({
      .verbose = true,
  });
  cmd("echo", "Working while sleeping...").run(config);

  future.get();
  cmd("echo", "Slept.").run(config);
}
