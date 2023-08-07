#define BSTB_IMPL

#include "../bootstrab.hpp"

using namespace bstb;


// By default, the BSTB_DEFAULT_BUFFER is a HeapBuffer, but we can define our own
// stack buffer with 500 bytes of space.
#undef BSTB_DEFAULT_BUFFER
#define BSTB_DEFAULT_BUFFER StackBuffer<500>

auto main() -> int {
  const auto config = Config {
    .pipe = Pipe::Inherited()
  };

  cmd("echo", "Implicitly using stack memory.").run(config);

  // We can also pass our own buffers to the command via template
  cmd<buffer::HeapBuffer>("echo", "Using heap memory.").run(config);

  // We can define our own stack memory by shorthand too.
  cmd<300>("echo", "Using 300 bytes of stack memory.").run(config);
}
