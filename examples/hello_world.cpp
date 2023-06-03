#define BOOTSTRAB_IMPLEMENTATION
#include "../bootstrab.hpp"

using namespace bootstrab;

auto main() -> int {
  const auto config = Command::Config{
      // The default pipe is /dev/null to avoid unecessary output.
      // Luckily, we can inherit from stdout that the build script has when run.
      .pipe = Pipe::Inherited(),
  };

  Command::from("echo", "Hello, World!").run(config);
}
