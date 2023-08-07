#define BSTB_IMPL 
#include "../bootstrab.hpp"

using namespace bstb;

auto main() -> int {
  const auto config = Config{
      // The default pipe is /dev/null to avoid unecessary output.
      // Luckily, we can inherit from stdout that the build script has when run.
      .pipe = Pipe::Inherited(),
  };

  cmd("echo", "Hello, World!").run(config);
}
