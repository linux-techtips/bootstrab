# bootstrab

Single header library for bootstrapping C++ projects.

## Concept

Writing build scripts for C++ projects is an absolute pain. More often than not,
I find myself resorting to writing bash build scripts because modern build systems
have alot of complexity that ends up being time consuming, hard to maintain, and
massively increase the amount of dependencies that the project requires. I believe
that if you are writing a project in C++, then your only dependency should be a C++
compiler.

## About

bootstrab is a stb-style header library that gives you the tools to create and run
build commands and to enable compile-time metaprogramming. In order to use bootstrab,
all that needs to be done is to copy the header file into your project directory, and
then include it into a c++ file. This means that not only the build script, but the
entire project can have access to bootstrab's features.

## Usage

Here's a quick "Hello World" example in bootstrab:

```cpp
#define BSTB_IMPL 
#include "bootstrab.hpp"

using namespace bstb;

auto main() -> int {
    const auto config = Config {
        .pipe = Pipe::Inherited(),
    };

    cmd("echo", "Hello World!").run(config);
}

```

For more usage information, check out [examples](examples).

## Standout Features

- Asynchronous Execution: Commands can be queued to run in parallel and awaited at a later time.
- Command Pooling: Multiple commands can be run in parallel and awaited at once.
- Buffers: A concept interface is provided that can allow the allocation of command arguments to your own memory pools.
- Directory Filters: Filters can be applied to files and directories to create behavior based off of file or directorie's attributes
- REBUILD_URSELF: Inspired by Tsoding's [nobuild](https://github.com/tsoding/nobuild) REBUILD_URSELF can detect changes in the build script and rebuild itself as needed so you can compile once and run forever.

## TODO

- [X] File Embedding (Kinda)
- [X] Multi-threading rework
- [X] First class compiler support
- [X] BYOB (Bring Your Own Buffers)
- [X] Optimizsationsz
- [ ] Fix Process error propogation
- [ ] Windows support (LOL)
- [ ] Dynamic Libraries
