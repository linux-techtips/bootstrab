#ifndef BSTB_IMPL
#define BSTB_IMPL
#endif
#include "../bootstrab.hpp"

using namespace bstb;

auto main() -> int {

  // Anything that can fail will return a bstb::Result.
  // In this case the result is the exit code of the process
  // that was run.
  auto [status, err] = cmd("echo", "Hello Seamen").run({ .pipe = Pipe::Inherited() });
  if (err) {
    std::cout << err.why() << std::endl;
    std::exit(status);
  }

  std::cout << "Command exited with status: " << status << '\n';

}
