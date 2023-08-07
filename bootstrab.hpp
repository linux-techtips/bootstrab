#ifdef BSTB_IMPL

#include <filesystem>
#include <iostream>
#include <ranges>
#include <thread>
#include <vector>
#include <span>

extern "C" {
  #include <sys/wait.h>
  #include <unistd.h>
  #include <fcntl.h>
}

namespace {
  
  using CStr = char const*;
  using Fd = int;
  using Pid = pid_t;

  struct PanicStream {
    static auto instance() -> PanicStream {
      static const auto panic_stream = PanicStream{};
      return panic_stream;
    }

    [[noreturn]]
    friend auto operator<<(std::ostream& os, [[maybe_unused]] const PanicStream panic_stream) -> std::ostream& {
      os.flush();
      perror("Panic");
      std::abort();
    }
  };

  inline static auto panic = PanicStream::instance();

} // namespace private

namespace bstb::buffer {

  template <typename T>
  concept Buffer = requires(T buf, const T buf_const, T& buf_ref, std::string_view sv, std::ostream& os) {
    { std::is_trivially_constructible_v<T> };
    { std::is_trivially_destructible_v<T> };
    { std::is_trivially_copyable_v<T> };

    { buf.exec_args() } -> std::same_as<char* const*>;
    { buf.size() } -> std::same_as<size_t>;
    { buf.push(sv, sv) } -> std::same_as<void>;
    { buf[std::size_t{}] } -> std::same_as<CStr>;
    { buf_const[std::size_t{}] } -> std::same_as<CStr>;
    { os << buf_ref } -> std::same_as<std::ostream&>;
  };

  struct HeapBuffer {
    std::vector<std::string> buffer;
    std::vector<CStr> exec_buffer;

    [[nodiscard]]
    auto exec_args() -> char* const* {
      exec_buffer.clear();
      exec_buffer.reserve(buffer.size() + 1);

      for (const auto& str : buffer) {
        exec_buffer.push_back(&str[0]);
      }
      exec_buffer.push_back(nullptr);

      return const_cast<char* const*>(exec_buffer.data());
    }

    [[nodiscard]]
    constexpr auto size() -> size_t {
      return buffer.size();
    }

    template <typename... Args>
    auto push(Args&&... args) -> void {
      if constexpr (sizeof...(Args) == 1) {
        buffer.emplace_back(std::forward<Args>(args)...);
      } else {
        const auto str = (std::string{} += ... += args);
        buffer.push_back(std::move(str));
      }
    }

    constexpr auto operator[](size_t idx) -> CStr {
      return buffer[idx].c_str();
    }

    constexpr auto operator[](size_t idx) const -> CStr {
      return buffer[idx].c_str();
    }
    
    friend auto operator<<(std::ostream& os, HeapBuffer& buffer) -> std::ostream& {
      for (const auto& str : buffer.buffer) {
        os << str << ' ';
      }
      return os;
    }
  };

  template <size_t Cap>
  struct StackBuffer {
    size_t buffer_idx = 0;
    size_t exec_buffer_idx = 0;
    std::array<char, Cap> buffer;
    std::array<CStr, Cap/8> exec_buffer;

    [[nodiscard]]
    constexpr auto exec_args() -> char* const* {
      exec_buffer[exec_buffer_idx] = nullptr;
      return const_cast<char* const*>(exec_buffer.data());
    }

    [[nodiscard]]
    constexpr auto size() const -> size_t {
      return buffer_idx;
    }

    template <typename... Args>
    constexpr auto push(Args&&... args) -> void {
      exec_buffer[exec_buffer_idx++] = &buffer[buffer_idx];

      auto push_every = [this] (std::string_view str) {
        std::copy(str.begin(), str.end(), &buffer[buffer_idx]);
        buffer_idx += str.size(); 
      };

      (push_every(std::forward<Args>(args)), ...);    
      buffer[buffer_idx++] = '\0';
    }

    constexpr auto operator[](size_t idx) -> CStr {
      return buffer[idx];
    }

    constexpr auto operator[](size_t idx) const -> CStr {
      return buffer[idx];
    }

    friend auto operator<<(std::ostream& os, StackBuffer& buffer) -> std::ostream& {
      const auto* exec_args = buffer.exec_args();
      for (size_t i = 0; i < buffer.exec_buffer_idx; ++i) {
        os << exec_args[i] << ' ';
      }
      return os;
    }
  };

  #ifndef BSTB_DEFAULT_BUFFER
  #define BSTB_DEFAULT_BUFFER HeapBuffer
  #endif
  using Default = BSTB_DEFAULT_BUFFER;

} // namespace bstb::buffer

namespace bstb::fs {

  using Path = std::filesystem::path;
  using Entry = std::filesystem::directory_entry;

  [[nodiscard]]
  inline auto modified_after(const Path& path1, const Path& path2) -> bool {
    return std::filesystem::last_write_time(path1) > std::filesystem::last_write_time(path2);
  }

  namespace {
    
    template <typename T>
    auto iter_impl(const Path& path) -> decltype(auto) {
      return T{ path };
    }

    template <typename T, typename Fn>
    auto filter_impl(const Path& path, Fn&& filter) -> decltype(auto) {
      return T{ path }
        | std::views::filter(std::forward<Fn>(filter))
        | std::views::transform([](auto&& entry) { return entry; });
    }

  } // namespace private 

  inline auto iter(const Path& path) -> decltype(auto) {
    return iter_impl<std::filesystem::directory_iterator>(path);
  }

  inline auto recursive_iter(const Path& path) -> decltype(auto) {
    return iter_impl<std::filesystem::recursive_directory_iterator>(path);
  }

  template <typename Fn>
  inline auto filter(const Path& path, Fn&& filter) -> decltype(auto) {
    return filter_impl<std::filesystem::directory_iterator, Fn>(path, std::forward<Fn>(filter));
  }
 
  template <typename Fn>
  inline auto recursive_filter(const Path& path, Fn&& filter) -> decltype(auto) {
    return filter_impl<std::filesystem::recursive_directory_iterator, Fn>(path, std::forward<Fn>(filter));
  }


} // namespace bstb::fs

namespace bstb {

  template <typename T, typename U>
  concept IterableOver = requires(T container) {
    { *std::begin(container) } -> std::convertible_to<U>;
    { *std::end(container) } -> std::convertible_to<U>;
  };

  struct Pipe {
    Fd read;
    Fd write;

    [[nodiscard]]
    constexpr static auto Inherited() -> Pipe {
      return { STDIN_FILENO, STDOUT_FILENO };
    }

    [[nodiscard]]
    static auto Owned(Fd read, Fd write) -> Pipe {
      return { read, write };
   }

    [[nodiscard]]
    static auto Null() -> Pipe { // TODO: (Carter) static vars in function are not free.
      static auto read = open("/dev/null", O_RDONLY);
      static auto write = open("/dev/null", O_WRONLY);

      return { read, write };
    }
  };

  struct Future {
    using Status = int;

    Pid pid;

    static auto wait(Pid pid) -> Status {
      auto status = Status{};
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }

      std::cerr << "Process did not execute properly. Status: " << status << '\n' << panic;
    }

    auto get() const -> Status {
      return Future::wait(pid);
    }

    [[nodiscard]]
    auto completed() const -> bool {
      Status status;
      return waitpid(pid, &status, WNOHANG) != 0;
    }
  };

  struct TaskList {
    std::vector<Future> tasks;

    auto push(Future future) -> void {
      tasks.push_back(future);
    }

    auto wait() -> void {
      using namespace std::chrono_literals;

      while (!tasks.empty()) {
        auto it = tasks.begin();
        while (it != tasks.end()) {
          if (it->completed()) {
            it = tasks.erase(it);
          } else {
            ++it;
          }
        }
        std::this_thread::sleep_for(10ms);
      }
    }

  };

  struct Config {
    Pipe pipe = Pipe::Null();
    bool verbose {};
  };

  template <buffer::Buffer Buffer>
  struct Command {
    Buffer buffer;

    template <typename... Args>
    auto arg(Args&&... args) -> void {
      buffer.push(std::forward<Args>(args)...);
    }

    template <typename Path>
    auto arg(Path&& path) -> void requires (
      std::same_as<std::remove_cvref_t<Path>, fs::Path> ||
      std::same_as<std::remove_cvref_t<Path>, fs::Entry>
    ) {
      buffer.push(path.string());
    }

    template <typename Iter>
    requires IterableOver<Iter, std::string_view>
    auto arg(Iter&& iter) -> void {
      for (auto&& arg : iter) {
        buffer.push(std::forward<std::decay_t<decltype(arg)>>(arg));
      }
    }

    template <typename Iter>
    requires IterableOver<Iter, fs::Entry>
    auto arg(Iter&& iter) -> void {
      for (auto&& entry : iter) {
        buffer.push(entry.path().string());
      }
    }

    auto exec(const Config& config) -> Pid {
      if (config.verbose) {
        std::cout << buffer << std::endl;
      }

      const auto child = fork();
      if (child < 0) {
        std::cerr << "Failed to fork process with error: " << child << '\n' << panic;
      
      } else if (child == 0) {

        const auto read = config.pipe.read;
        const auto write = config.pipe.write;

        if (read != STDIN_FILENO) {
          dup2(read, STDIN_FILENO);
        }

        if (write != STDOUT_FILENO) {
          dup2(write, STDOUT_FILENO);
        }

        const auto* exec_args = buffer.exec_args();
        if (const auto err = execvp(exec_args[0], exec_args)) {
          std::cerr << "Task exited abnormally: " << err << '\n' << panic;
        }

      }

      return child;
    }

    auto run(const Config& config) -> Future::Status {
      return Future::wait(Command::exec(config));
    }

    [[nodiscard]]
    auto run_async(const Config& config) -> Future {
      return Future { Command::exec(config) };
    }

    friend auto operator<<(std::ostream& os, Command& cmd) -> std::ostream& {
      return os << cmd.buffer;
    }

  };

  template <buffer::Buffer Buffer = buffer::Default, typename... Args>
  [[nodiscard]]
  inline auto cmd(Args&&... args) -> Command<Buffer> {
    auto command = Command<Buffer> { Buffer{} }; 
    (command.arg(std::forward<Args>(args)), ...);
    return command;
  }

  template <size_t Cap, typename... Args>
  [[nodiscard]]
  inline auto cmd(Args&&... args) -> Command<buffer::StackBuffer<Cap>> {
    return cmd<buffer::StackBuffer<Cap>>(std::forward<Args>(args)...);
  }

} // namespace bstb

namespace bstb::compiler::impl {

  template <buffer::Buffer Buffer>
  struct GNU {
    using Command = Command<Buffer>;

    constexpr static auto arg(Command& cmd, std::string_view str) -> void {
      cmd.arg(str);
    }

    constexpr static auto input(Command& cmd, std::string_view str) -> void {
      cmd.arg(str);
    }

    constexpr static auto output(Command& cmd, std::string_view str) -> void {
      cmd.arg("-o");
      cmd.arg(str);
    }

    constexpr static auto version(Command& cmd, std::string_view str) -> void {
      cmd.arg("-std=", str);
    }

    constexpr static auto warn(Command& cmd, std::string_view str) -> void {
      cmd.arg("-W", str);
    }

    constexpr static auto include(Command& cmd, std::string_view str) -> void {
      cmd.arg("-I", str);
    }

    constexpr static auto opt(Command& cmd, std::string_view str) -> void {
      cmd.arg("-O", str);
    }

    constexpr static auto no_exe(Command& cmd) -> void {
      cmd.arg("-c");
    }

    constexpr static auto feature(Command& cmd, std::string_view str) -> void {
      cmd.arg("-f", str);
    }

    constexpr static auto arch(Command& cmd, std::string_view str) -> void {
      cmd.arg("-m", str);
    }
  };

}

namespace bstb::compiler::style {

  template <template <typename> typename T, buffer::Buffer Buffer>
  struct C {
    using Impl = T<Buffer>; 
    
    Command<Buffer> cmd;

    constexpr C(std::string_view name) {
      cmd.arg(name);
    }

    constexpr auto arg(std::string_view str) -> C& {
      Impl::arg(cmd, str);
      return *this;
    }

    constexpr auto input(std::string_view str) -> C& {
      Impl::input(cmd, str);
      return *this;
    }

    constexpr auto output(std::string_view str) -> C& {
      Impl::output(cmd, str);
      return *this;
    }

    constexpr auto version(std::string_view str) -> C& {
      Impl::version(cmd, str);
      return *this;
    }

    constexpr auto warn(std::string_view str) -> C& {
      Impl::warn(cmd, str);
      return *this;
    }

    constexpr auto include(std::string_view str) -> C& {
      Impl::include(cmd, str);
      return *this;
    }

    constexpr auto opt(std::string_view str) -> C& {
      Impl::opt(cmd, str);
      return *this;
    }

    constexpr auto no_exe(std::string_view str) -> C& {
      Impl::no_exe(cmd, str);
      return *this;
    }

    constexpr auto feature(std::string_view str) -> C& {
      Impl::feature(cmd, str);
      return *this;
    }

    constexpr auto arch(std::string_view str) -> C& {
      Impl::arch(cmd, str);
      return *this;
    }
    
    constexpr auto compile(const Config& config) -> Future::Status {
      return cmd.run(config);
    }

    constexpr auto compile_async(const Config& config) -> Future {
      return cmd.run_async(config);
    }
  };

} // namespace bstb::compiler::impl

namespace bstb::compiler::c {

  template <buffer::Buffer Buffer = buffer::Default> 
  [[nodiscard]]
  constexpr auto gcc() -> style::C<impl::GNU, Buffer> {
    return { "gcc" };
  }

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto clang() -> style::C<impl::GNU, Buffer> {
    return { "clang" };
  }

} // namespace bstb::compiler::c

namespace bstb::compiler::cpp {

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto gcc() -> style::C<impl::GNU, Buffer> {
    return { "g++" };
  } 

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto clang() -> style::C<impl::GNU, Buffer> {
    return { "clang++" };
  }

} // namespace bstb::compiler::cpp

namespace bstb::compiler {
  
  #if defined(__clang__)
    template <buffer::Buffer Buffer = buffer::Default>
    [[nodiscard]]
    constexpr auto native() -> decltype(auto) { return cpp::clang<Buffer>(); }
  #elif defined(__GNUC__) || defined(__GNUG__) || defined(__MINGW32__) || defined(__MINGW64__)
    template <buffer::Buffer Buffer = buffer::Default>
    [[nodiscard]]
    constexpr auto native() -> decltype(auto) { return cpp::gcc<Buffer>(); }
    
  #endif

} // namespace bstb::compiler

namespace bstb {

  inline auto rebuild(std::string_view input, std::span<char*>&& args) -> void {
    const auto* target = args[0]; 

    if (fs::modified_after(target, input)) {
      return;
    }

    std::cout << "Change detected. Rebuilding...\n";

    const auto status = compiler::native<buffer::StackBuffer<100>>()
      .version("c++20")
      .input(input)
      .output(target)
      .compile({});

    if (status) {
      std::cerr << "Failed to rebuild urself :(\n" << panic;
    }

    cmd(args).run({ .pipe = Pipe::Inherited() });

    std::exit(0);
  }

  #define REBUILD_URSELF(argc, argv) bstb::rebuild(__FILE__, std::span<char*>(argv, static_cast<size_t>(argc)))

} // namespace bstb

#endif
