#define BSTB_IMPL
#include "../bootstrab.hpp"

using namespace bstb;

auto main(int argc, char **argv) -> int {
  REBUILD_URSELF(argc, argv);

  const auto *msg = "CHANGE THIS MESSAGE AND RERUN!";
  cmd("echo", msg).run({ .pipe = Pipe::Inherited() });
}
