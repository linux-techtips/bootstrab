#define BOOTSTRAB_IMPLEMENTATION
#include "bootstrab.hpp"

auto main() -> int {
    using namespace bootstrab;

    auto output_config = Command::Config {
        .pipe = Pipe::Inherited(),
    };

    auto hello_cmd = Command::from("echo", "Hello World!");

    hello_cmd.run(output_config);
}
