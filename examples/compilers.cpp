#define BSTB_IMPL
#include "../bootstrab.hpp"

using namespace bstb;

auto main() -> int {

  // Currently, bootstrab supports abstractions for a few types of C and C++ compilers.
  // NOTE: MinGW is implemented but windows support is underway

  // We can get the native compiler that bootstrab was compiled with like this.
  // If you compiled with gcc this will return a gcc compiler, etc.
  [[maybe_unused]]
  auto native_compiler = compiler::native();

  // We can also explicitly request a specific compiler for a specific language.
  // In this case, I'll request clang++
  [[maybe_unused]]
  auto clang_compiler = compiler::cpp::clang();

  // We can also specify a buffer for the compiler as well
  [[maybe_unused]]
  auto my_compiler = compiler::native<buffer::StackBuffer<100>>();

  // Let's check out all of the cool stuff we can do with our compiler abstraction.
  auto hello_world_compiler = compiler::native()
    .version("c++20")
    .warn("all")
    .warn("error")
    .opt("z")
    .feature("no-exceptions")
    .input("hello_world.cpp")
    .output("hello_world");
    
    // If there isn't support for a compiler arg, you can use
    // the arg() method to pass in a flag in plaintext

  hello_world_compiler.compile({ .verbose = true });

  cmd("./hello_world").run({ .pipe = Pipe::Inherited() });
};
