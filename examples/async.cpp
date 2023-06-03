#define BOOTSTRAB_IMPLEMENTATION
#include "../bootstrab.hpp"

using namespace bootstrab;

auto main() -> int {
  const auto config = Command::Config{
      .pipe = Pipe::Inherited(),
  };

  auto sleep_cmd = Command::from("sleep", "3");

  sleep_cmd.run({
      .verbose = true,
  });
  Command::from("echo", "Slept.\nSleeping async!").run(config);

  auto future = sleep_cmd.run_async({
      .verbose = true,
  });
  Command::from("echo", "Working while sleeping...").run(config);

  future.wait();
  Command::from("echo", "Slept.").run(config);
}
