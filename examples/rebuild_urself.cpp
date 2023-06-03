#define BOOTSTRAB_IMPLEMENTATION
#include "../bootstrab.hpp"

using namespace bootstrab;

auto main(int argc, char **argv) -> int {
  REBUILD_URSELF(env::Args::from(argc, argv));

  const auto *msg = "CHANGE THIS MESSAGE AND RERUN!";

  Command::from("echo", msg)
      .run({
          .pipe = Pipe::Inherited(),
      });
}
