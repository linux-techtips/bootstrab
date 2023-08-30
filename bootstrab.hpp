#include <iterator>
#if defined(BSTB_IMPL) || defined(BSTB_RT)

#include <utility>

#if defined(_WIN32) || defined(_WIN64)

#error Michaelsoft Binbows is mean, all the functions are different plz help 
#define bstb_system windows

namespace sys::windows {} // namespace sys::windows

#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#define bstb_system linux

extern "C" {
  #include <sys/types.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <sys/wait.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <dlfcn.h>
  #include <spawn.h>
}

using CStr = char const*;

namespace sys::linux { 

  using Env = char**;

  extern "C" Env environ;

  namespace io {

    using Fd = int;

    constexpr static Fd STDIN = STDIN_FILENO;
    constexpr static Fd STDOUT = STDOUT_FILENO;
    constexpr static Fd STDERR = STDERR_FILENO;
    constexpr static CStr NULL_PATH = "/dev/null";
    constexpr static int FAILED = -1;

    inline auto open_fd_read(CStr path) -> Fd {
      return open(path, O_RDONLY, 0644);
    }

    inline auto open_fd_write(CStr path) -> Fd {
      return open(path, O_RDWR | O_CREAT, 0664);
    }

    inline auto get_fd_size(Fd fd) -> size_t {
      struct stat st;
      fstat(fd, &st);
      return st.st_size;
    }

    inline auto fd_truncate(Fd fd, size_t size) -> int {
      return ftruncate(fd, size);
    }

    inline auto map_fd_read(Fd fd, size_t size) -> void* {
      auto* data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0); 
      if (data == MAP_FAILED) {
        return nullptr;
      }
      return data;
    }

    inline auto map_fd_write(Fd fd, size_t size) -> void* {
      auto* data = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
      if (data == MAP_FAILED) {
        return nullptr;
      }
      return data;
    }

  } // namespace io

  namespace process {

    using Pid = pid_t;
    using Status = int;
    
    constexpr static int FAILED = -1;

    inline auto exec(io::Fd read, io::Fd write, CStr arg, char* const* args) -> Pid {
      posix_spawn_file_actions_t file_actions;
      posix_spawnattr_t attr;

      Pid pid;
      Status status;

      if (posix_spawn_file_actions_init(&file_actions) != 0) {
        return FAILED;
      }

      if (read != io::STDIN) {
        posix_spawn_file_actions_adddup2(&file_actions, read, io::STDIN);
      }

      if (write != io::STDIN) {
        posix_spawn_file_actions_adddup2(&file_actions, write, io::STDIN);
      }

      posix_spawnattr_init(&attr);

      status = posix_spawnp(&pid, arg, &file_actions, &attr, args, environ);

      posix_spawn_file_actions_destroy(&file_actions);
      posix_spawnattr_destroy(&attr);

      if (status != 0) {
        return FAILED;
      }

      return pid;
    }

    inline auto wait(Pid pid) -> Status {
      auto status = Status{};
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }

      return FAILED;
    }

    inline auto completed(Pid pid) -> bool {
      auto status = Status{};
      return waitpid(pid, &status, WNOHANG) != 0;
    }

  } // namespace process

  namespace dylib {

    using Handle = void*;

    enum {
      Now = RTLD_NOW,
      Lazy = RTLD_LAZY,
      Global = RTLD_GLOBAL,
      Local = RTLD_LOCAL,
      NoLoad = RTLD_NOLOAD,
      NoDelete = RTLD_NODELETE,
    };

    inline auto open(CStr path, int mode) -> Handle {
      return dlopen(path, mode);
    }

    inline auto sym(Handle handle, CStr symbol) -> Handle {
      return dlsym(handle, symbol);
    }

    inline auto err() -> CStr {
      return dlerror();
    }

    inline auto close(Handle handle) -> int {
      return dlclose(handle);
    }

  } // namespace dylib

} // sys::linux

#else // bstb_system

#error Unsupported System for bootstrab implementation

#endif // bstb_system

namespace sys {

  using namespace sys::bstb_system;

} // namespace sys

namespace bstb {
  
  struct Err {
    CStr msg {};

    inline auto why() const -> CStr {
      return msg;
    }

    inline operator bool() const {
      return msg;
    }
  };

  template <typename T>
  struct Result {
    T ok;
    Err err {};
 
    inline operator bool () const {
      return err;
    }
  };

  struct Pipe {
    sys::io::Fd read;
    sys::io::Fd write;

    static inline auto Inherited() -> Pipe {
      return { sys::io::STDIN, sys::io::STDOUT };
    }

    static inline auto Owned(sys::io::Fd read, sys::io::Fd write) -> Pipe {
      return { read, write };
    }

    static inline auto Null() -> Pipe {
      static auto read = sys::io::open_fd_read(sys::io::NULL_PATH);
      static auto write = sys::io::open_fd_write(sys::io::NULL_PATH);
      return { read, write };
    }
  };

  struct Dylib {
    sys::dylib::Handle handle;
    CStr path;

    static inline auto Open(CStr path, int mode = sys::dylib::Now) -> Result<Dylib> {
      auto* handle = sys::dylib::open(path, mode);
      if (!handle) {
        return { .err = { "Failed to open dylib." } };
      }
      return {{ handle, path }};
    }

    template <typename T = void*>
    inline auto sym(CStr symbol) -> Result<T> {
      auto* ptr = sys::dylib::sym(handle, symbol);
      if (!ptr) {
        return { .err = { "Invalid symbol." } };
      }

      return { reinterpret_cast<T>(ptr) };
    }

    template <typename Ret, typename Fn, typename... Args>
    inline auto invoke(CStr symbol, Args&&... args) -> Result<Ret> {
      auto& [ptr, err] = this->sym<Fn>(symbol);
      if (err) {
        return { .err = err };
      }

      return { ptr(std::forward<Args>(args)...) };
    }

    inline auto reload(int mode = sys::dylib::Now) -> Result<Dylib&> {
      this->close();
      handle = sys::dylib::open(path, mode);

      if (!handle) {
        return { *this, { "Failed to reload dylib." } };
      }

      return { *this };
    }

    inline auto close() -> int {
      return sys::dylib::close(handle);
    }

    ~Dylib() {
      this->close();
    }
  };

  struct MMap {
    sys::io::Fd fd;
    size_t size;
    void* data;

    inline static auto Read(CStr path) -> Result<MMap> {
      auto fd = sys::io::open_fd_read(path);
      if (fd == sys::io::FAILED) {
        return { .err = { "Failed to open fd." } };
      }

      auto size = sys::io::get_fd_size(fd);
      auto* data = sys::io::map_fd_read(fd, size);
      if (!data) {
        return { .err = { "Failed to map file." } };
      }

      return {{ fd, size, data }};
    }

    inline static auto Write(CStr path, size_t size) -> Result<MMap> {
      auto fd = sys::io::open_fd_write(path);
      if (fd == sys::io::FAILED) {
        return { .err = { "Failed to open fd." } };
      }
      
      auto* data = sys::io::map_fd_write(fd, size);
      sys::io::fd_truncate(fd, size);
      if (!data) {
        return { .err = { "Failed to map file." } };
      }

      return {{ fd, size, data }};
    }
  };

} // namespace bstb

#endif // BSTB_IMPL || BSTB_RT

#ifdef BSTB_IMPL

#include <string_view>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <vector>
#include <cmath>
#include <span>

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
    size_t buffer_idx= 0;
    size_t exec_buffer_idx= 0;
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
  using namespace std::filesystem;

  enum class Create {
    None,
    File,
    Dir,
  };

  inline auto make_path(const fs::path& _path, Create create = Create::None) -> fs::path {
    if (!exists(_path)) {
      switch (create) {
        case Create::None: break;
        case Create::File: {
          auto _ = std::ofstream(_path, std::ios::out);
        }
        case Create::Dir: {
          fs::create_directory(_path);
        }
      }
    }
    return _path;
  }

  inline auto modified_after(const path& path1, const path& path2) -> bool {
    if (!fs::exists(path1)) {
      return false;
    }
    if (!fs::exists(path2)) {
      return true;
    }
    return last_write_time(path1) > last_write_time(path2);
  }

  namespace {
    template <typename T, typename Pred>
    struct Iter {
      using iterator_category = typename std::input_iterator_tag;
      using value_type = typename T::value_type;
      using difference_type = typename T::difference_type;
      using pointer = typename T::pointer;
      using reference = typename T::reference;
      
      T curr;
      T last {};
      Pred pred;

      explicit Iter(const T& _curr, Pred&& _pred) :
        curr(_curr),
        pred(std::forward<Pred>(_pred)) {
          this->next();
        }

      auto operator++() -> Iter& {
        ++curr;
        this->next();
        return *this;
      }

      auto operator*() const -> const reference {
        return *curr;
      }

      auto operator->() const -> const pointer {
        return &(*curr);
      }

      auto operator!=(const Iter& other) const -> bool {
        return curr != other.curr;
      }

      auto next() -> void {
        for (; curr != last && !pred(*curr); ++curr);
      }
    };

    template <typename T, typename Pred>
    struct Range {
      using value_type = typename T::value_type;
      using iterator = Iter<T, Pred>;
    
      T curr;
      Pred&& pred;

      auto begin() -> iterator {
        return iterator(curr, std::forward<Pred>(pred));
      }
      
      auto end() -> iterator {
        return iterator({}, std::forward<Pred>(pred));
      }
    };

    template <typename T, typename Fn>
    auto filter_impl(const path& _path, Fn&& filter) -> Range<T, Fn> { 
      return { T(_path), std::forward<Fn>(filter) };
    }

    template <typename T>
    auto iter_impl(const path& _path) -> decltype(auto) {
      auto pred = []([[maybe_unused]] auto&) { return true; };
      return filter_impl<T, decltype(pred)>(_path, std::forward<decltype(pred)>(pred));
    }

  } // namespace private 

  inline auto iter(const path& path) -> decltype(auto) {
    return iter_impl<std::filesystem::directory_iterator>(path);
  }

  inline auto recursive_iter(const path& path) -> decltype(auto) {
    return iter_impl<std::filesystem::recursive_directory_iterator>(path);
  }

 template <typename Fn>
  inline auto filter(const path& path, Fn&& filter) -> decltype(auto) {
    return filter_impl<std::filesystem::directory_iterator, Fn>(path, std::forward<Fn>(filter));
  }

  template <typename Fn>
  inline auto recursive_filter(const path& path, Fn&& filter) -> decltype(auto) {
    return filter_impl<std::filesystem::recursive_directory_iterator, Fn>(path, std::forward<Fn>(filter));
  }

} // namespace bstb::fs

namespace bstb::embedder {

  namespace {
    constexpr inline size_t DefaultRowSize = 20;

    inline auto write_bytes_impl(char*& buf, char const* data, size_t size) -> void {
      std::memcpy(buf, data, size);
      buf += size;
    }

    inline auto write_hex_impl(char*& buf, unsigned char const* data, size_t size, size_t row_size) -> void {
      constexpr static char hex[] = "0123456789ABCDEF";
      for (size_t i = 0; i < size; ++i) {
        const auto byte = data[i];
        *buf++ = '0';
        *buf++ = 'x';
        *buf++ = hex[(byte >> 4) & 0xF];
        *buf++ = hex[byte & 0xF];
        *buf++ = ',';
        *buf++ = ' ';
        if ((i + 1) % row_size == 0 && i != size - 1) {
          *buf++ = '\n';
          *buf++ = '\t';
        }
      }

      if (*(buf - 2) == ',') {
        *(buf - 2) = ' ';
      }
    }

    inline auto write_num_impl(char*& buf, size_t value) -> void {
      char tmp[21];
      char* idx = tmp + sizeof(tmp) - 1;
      *idx = '\0';
  
      do {
        *--idx = '0' + (value % 10);
        value /= 10;
      } while (value != 0);

      while (*idx) {
        *buf++ = *idx++;
      }
    }

  } // namespace private

  struct Config {
    const std::string_view begin {};
    
    const std::string_view size_header {};
    const std::string_view size_footer {};

    const std::string_view data_header {};
    const std::string_view data_footer {};

    const std::string_view end {};
  };

  static auto embed_impl(const fs::path& read, const fs::path& write, size_t row_size, const Config& config) -> bool { 
    const auto [
      begin,
      size_header,
      size_footer,
      data_header,
      data_footer,
      end
    ] = config;

    const auto text_size = (
      begin.size() + size_header.size() + 
      size_footer.size() + data_header.size() + 
      data_footer.size() + end.size()
    );

    const auto [read_map, read_map_err] = MMap::Read(read.c_str());
    if (read_map_err) {
      return false;
    }

    static constexpr int EMPTY_SIZE_CALC = -2; // TODO: (Carter) There's a logic error somewhere but this constant works
    const auto bytes_for_size = (size_header.empty() && size_footer.empty()) ? EMPTY_SIZE_CALC : std::log10(read_map.size) - 1;
    const auto rows = ((read_map.size + row_size - 1) / row_size) * 2;
    const auto write_size = text_size + (read_map.size * 6) + rows + bytes_for_size;

    auto [write_map, write_map_err] = MMap::Write(write.c_str(), write_size);
    if (write_map_err) {
      return false;
    }

    const auto* read_data = static_cast<unsigned char*>(read_map.data);
    auto* write_data = static_cast<char*>(write_map.data);

    if (!begin.empty()) {
      write_bytes_impl(write_data, begin.data(), begin.size());
    }
      
    if (bytes_for_size != EMPTY_SIZE_CALC) {
      write_bytes_impl(write_data, size_header.data(), size_header.size());
      write_num_impl(write_data, read_map.size);
      write_bytes_impl(write_data, size_footer.data(), size_footer.size());
    }

    write_bytes_impl(write_data, data_header.data(), data_header.size());
    write_hex_impl(write_data, read_data, read_map.size, row_size);
    write_bytes_impl(write_data, data_footer.data(), data_footer.size());
  
    write_bytes_impl(write_data, end.data(), end.size());
  
    return true;
  }

  inline auto cpp(const fs::path& read_fname, const fs::path& write_fname, size_t row_size = DefaultRowSize) -> bool {
    return embed_impl(read_fname, write_fname, row_size, {
      .begin = "#ifndef BSTB_EMBED\n\t#error This is a bootstrab embed file, define BSTB_EMBED to use.\n#else\n",
      .size_header = "constexpr static unsigned long size = ",
      .size_footer = ";\n",
      .data_header = "template <typename T>\nconstexpr static T data = {\n\t",
      .data_footer = "\n};\n",
      .end = "#undef BSTB_EMBED\n#endif\n"
    });
  }

  inline auto c(const fs::path& read_fname, const fs::path& write_fname, size_t row_size = DefaultRowSize) -> bool {
    return embed_impl(read_fname, write_fname, row_size, {
      .begin = "#ifndef BSTB_EMBED\n\t#error This is a bootstrab embed file, define BSTB_EMBED to use.\n#else\n",
      .size_header = "static const unsigned long size = ",
      .size_footer = ";\n",
      .data_header = "static const unsigned char data[] = {\n\t",
      .data_footer = "\n};\n",
      .end = "#undef BSTB_EMBED\n#endif\n"
    });
  }
 
  inline auto py(const fs::path& read_fname, const fs::path& write_fname, size_t row_size = DefaultRowSize) -> bool {
    return embed_impl(read_fname, write_fname, row_size, {
      .data_header = "data = [\n\t",
      .data_footer = "\n]\n",
    });
  }

} // namespace bstb::embedder

namespace bstb {

  template <typename T, typename U>
  concept IterableOver = requires(T container) {
    { *std::begin(container) } -> std::convertible_to<U>;
    { *std::end(container) } -> std::convertible_to<U>;
  };

  struct Future {
    sys::process::Pid pid;

    inline auto wait() const -> Result<sys::process::Status> {
      auto res = sys::process::wait(pid);
      if (res == sys::process::FAILED) {
        return { .err = { "Process did not execute properly." }};
      }

      return { res };
    }

    inline auto completed() const -> bool {
      return sys::process::completed(pid);
    }
  };

  struct TaskList {
    std::vector<Future> tasks;

    auto push(Future future) -> void {
      tasks.push_back(future);
    }

    auto wait() -> void {
      while (!tasks.empty()) {
        auto it = tasks.begin();
        while (it != tasks.end()) {
          if (it->completed()) {
            it = tasks.erase(it);
          } else {
            ++it;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

    template <typename Iter>
    auto arg(Iter&& iter) -> void requires (IterableOver<Iter, std::string_view>) {
      for (auto&& arg : iter) {
        buffer.push(std::forward<std::decay_t<decltype(arg)>>(arg));
      }
    }

    template <typename Iter>
    auto arg(Iter&& iter) -> void requires (IterableOver<Iter, fs::directory_entry>) {
      for (auto&& arg : iter) {
        buffer.push(arg.path().c_str());
      }
    } 

    template <typename... Args>
    auto arg(Args&&... args) -> void {
      buffer.push(std::forward<Args>(args)...);
    }

    template <typename T>
    auto arg(T&& path) -> void requires (
      std::same_as<std::remove_cvref_t<T>, fs::path>
    ) {
      buffer.push(path.c_str());
    }

    auto exec(const Config& config) -> Result<sys::process::Pid> {
      if (config.verbose) {
        std::cout << buffer << std::endl;
      }

      const auto* exec_args = buffer.exec_args();

      const auto pid = sys::process::exec(
        config.pipe.read,
        config.pipe.write,
        exec_args[0],
        exec_args
      );

      if (pid == sys::process::FAILED) {
        return { .err = { "Failed to execute Command." } };
      }

      return { pid };
    }

    inline auto run(const Config& config) -> Result<sys::process::Status> {  
      const auto status = sys::process::wait(this->exec(config));
      if (status == sys::process::FAILED) {
        return { .err { "Process did not complete." }};
      }

      return {{ status }};
    }

    inline auto run_async(const Config& config) -> Result<Future> {
      const auto [pid, err] = this->exec(config);
      if (err) {
        return { .err = err };
      }

      return {{ pid }};
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

    template <typename T>
    constexpr static auto arg(Command& cmd, T&& arg) -> void {
      cmd.arg(std::forward<T>(arg));
    }

    template <typename T>
    constexpr static auto input(Command& cmd, T&& arg) -> void {
      cmd.arg(std::forward<T>(arg));
    }

    template <typename T>
    constexpr static auto output(Command& cmd, T&& arg) -> void {
      cmd.arg("-o");
      cmd.arg(std::forward<T>(arg));
    }

    constexpr static auto version(Command& cmd, std::string_view str) -> void {
      cmd.arg("-std=", str);
    }

    constexpr static auto warn(Command& cmd, std::string_view str) -> void {
      cmd.arg("-W", str);
    }

    constexpr static auto define(Command& cmd, std::string_view str) -> void {
      cmd.arg("-D", str);
    }

    template <typename T>
    constexpr static auto include_path(Command& cmd, T&& arg) -> void {
      cmd.arg("-I", std::forward<T>(arg));
    }

    constexpr static auto link_path(Command& cmd, std::string_view str) -> void {
      cmd.arg("-L", str);
    }

    constexpr static auto link(Command& cmd, std::string_view str) -> void {
      cmd.arg("-l", str);
    }

    constexpr static auto opt(Command& cmd, std::string_view str) -> void {
      cmd.arg("-O", str);
    }

    constexpr static auto debug_info(Command& cmd) -> void {
      cmd.arg("-g");
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

    template <typename U>
    constexpr auto arg(U&& arg) -> C& {
      Impl::arg(cmd, std::forward<U>(arg));
      return *this;
    }

    template <typename U>
    constexpr auto input(U&& arg) -> C& {
      Impl::input(cmd, std::forward<U>(arg));
      return *this;
    }

    template <typename U>
    constexpr auto output(U&& arg) -> C& {
      Impl::output(cmd, std::forward<U>(arg));
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

    constexpr auto define(std::string_view str) -> C& {
      Impl::define(cmd, str);
      return *this;
    }

    template <typename U>
    constexpr auto include_path(U&& str) -> C& {
      Impl::include_path(cmd, std::forward<U>(str));
      return *this;
    }

    constexpr auto link_path(std::string_view str) -> C& {
      Impl::link_path(cmd, str);
      return *this;
    }

    constexpr auto link(std::string_view str) -> C& {
      Impl::link(cmd, str);
      return *this;
    }

    constexpr auto opt(std::string_view str) -> C& {
      Impl::opt(cmd, str);
      return *this;
    }

    constexpr auto debug_info() -> C& {
      Impl::debug_info(cmd);
      return *this;
    }

    constexpr auto no_exe() -> C& {
      Impl::no_exe(cmd);
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
    
    constexpr auto compile(const Config& config) -> decltype(auto) {
      return cmd.run(config);
    }

    constexpr auto compile_async(const Config& config) -> decltype(auto) {
      return cmd.run_async(config);
    }
  };

} // namespace bstb::compiler::impl

namespace bstb::compiler::c {

  template <buffer::Buffer Buffer>
  using GCC = style::C<impl::GNU, Buffer>; 

  template <buffer::Buffer Buffer = buffer::Default> 
  [[nodiscard]]
  constexpr auto gcc() -> GCC<Buffer> {
    return { "gcc" };
  }

  template <buffer::Buffer Buffer>
  using Clang = style::C<impl::GNU, Buffer>;

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto clang() -> Clang<Buffer> {
    return { "clang" };
  }

} // namespace bstb::compiler::c

namespace bstb::compiler::cpp {

  template <buffer::Buffer Buffer>
  using GCC = style::C<impl::GNU, Buffer>; 

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto gcc() -> GCC<Buffer> {
    return { "g++" };
  } 

  template <buffer::Buffer Buffer>
  using Clang = style::C<impl::GNU, Buffer>;

  template <buffer::Buffer Buffer = buffer::Default>
  [[nodiscard]]
  constexpr auto clang() -> Clang<Buffer> {
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

    auto [status, err] = compiler::native<buffer::StackBuffer<100>>()
      .version("c++20")
      .arch("arch=native")
      .arch("tune=native")
      .feature("lto")
      .feature("no-exceptions")
      .opt("z")
      .input(input)
      .output(target)
      .compile({});

    if (status || err) {
      std::cerr << "Could not rebuild urself because: \n" << err.why() << std::endl;
      std::exit(status);
    } 

    cmd(args).run({ .pipe = Pipe::Inherited() });

    std::exit(0);
  }

  #define REBUILD_URSELF(argc, argv) bstb::rebuild(__FILE__, std::span<char*>(argv, static_cast<size_t>(argc)))

} // namespace bstb

#endif // BSTB_IMPL
