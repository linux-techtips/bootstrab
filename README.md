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
#define BOOTSTRAB_IMPLEMENTATION
#include "bootstrab.hpp"

using namespace bootstrab;

auto main() -> int {
    const auto config = Command::Config {
        .pipe = Pipe::Inherited(),
    };

    Command::from("echo", "Hello World!").run(config);
}

```

For more usage information, check out [examples](examples).

## Standout Features

- Asynchronous Execution: Commands can be queued to run in parallel and awaited at a later time.
- Command Pooling: Multiple commands can be run in parallel and awaited at once.
- Directory Filters: Filters can be applied to files and directories to create behavior based off of file or directorie's attributes
- REBUILD_URSELF: Inspired by Tsoding's [nobuild](https://github.com/tsoding/nobuild) REBUILD_URSELF can detect changes in the build script and rebuild itself as needed so you can compile once and run forever.

## TODO

- [ ] File Embedding
- [ ] Environment Integration
- [ ] Multi-threading rework
- [ ] Compile-Time code generation
- [ ] Hot Code Reloading
- [ ] Conditional Recompilation
- [ ] Git Integration
- [ ] Windows support (LOL)
- [ ] Optimizsationsz
