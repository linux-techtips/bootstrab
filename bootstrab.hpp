// #ifdef BOOTSTRAB_IMPLEMENTATION  // Comment out if experiencing linter issues

#ifndef _WIN32  // LINUX INCLUDE

    #include <fcntl.h>
    #include <sys/wait.h>
    #include <unistd.h>

static constexpr auto* PATH_SEP = "/";

#else  // WINDOWS INCLUDE

    #include <process.h>
    #include <windows.h>

static constexpr auto* PATH_SEP = "\\";

#endif  // STD INCLUDE

#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <vector>

namespace std {
namespace fs = filesystem;
}

namespace bootstrab {

using CStr = char const*;

[[noreturn]] inline auto panic(CStr message, std::ostream& ostream = std::cerr)
    -> void {
    ostream << "[PANIC]: " << message << '\n';
    std::abort();
}

using Fd  = int;
using Pid = pid_t;

struct alignas(8) Pipe {
    Fd read;
    Fd write;

#ifndef _WIN32
    static auto Inherited() -> Pipe {
        return {STDIN_FILENO, STDOUT_FILENO};
    }

#else  // TODO: (Makoto) Make work on windows
    static auto Inherited() -> Pipe {
        panic("haha imagine using windows");
    }
#endif

#ifndef _WIN32
    static auto Owned(Fd read, Fd write) -> Pipe {
        Fd pipefd[2];  // NOLINT modernize-avoid-c-arrays
        pipefd[0] = read;
        pipefd[1] = write;

        if (pipe(pipefd) != 0) {
            panic("Failed to create pipe.");
        }

        return {pipefd[0], pipefd[1]};
    }

#else  // TODO: (Makoto) Make work on windows
    static auto Owned(Fd read, Fd write) -> Pipe {
        panic("lol michaelsoft binbows");
    }
#endif

#ifndef _WIN32
    static auto Null() -> Pipe {
        Fd pipefd[2];  // NOLINT modernize-avoid-c-arrays
        pipefd[0] = open("/dev/null", O_RDONLY);
        pipefd[1] = open("/dev/null", O_WRONLY);

        if (pipefd[0] == -1 || pipefd[1] == -1) {
            panic("Failed to open /dev/null.");
        }

        return {pipefd[0], pipefd[1]};
    }

#else  // TODO: (Makoto) Make work on windows
    static auto Null() -> Pipe {
        panic("noooo windows nooooooo");
    }
#endif

#ifndef _WIN32
    auto deinit() const -> void {
        close(read);
        close(write);
    }

#else  // TODO: (Makoto) Make work on windows
    auto deinit() const -> void {
        panic("This probably works fine on windows but I'm too lazy to check");
    }
#endif
};

struct Command {
    // TODO: (Carter) This is inefficient, but a necessary evil for iterating over directories. Optimize later
    using Argv   = std::vector<std::string>;
    using Status = int;

    Argv args;

    struct Future {
        Pid pid;

        constexpr static auto from(Pid pid) -> Future {
            return {pid};
        }

        auto wait() const -> void {
            Command::process_wait(pid);
        }

        [[nodiscard]] auto get() const -> Status {
            return Command::process_wait(pid);
        }
    };

    struct Config {
        bool verbose {false};

        bool _padding0 {false};
        short _padding1 {0};
        int _padding2 {0};

        Pipe pipe {Pipe::Null()};

        ~Config() {
            pipe.deinit();
        }
    };

    template<typename... Args>
    static auto from(Args&&... args) -> Command {
        auto res = Argv {};
        res.reserve(sizeof...(Args) + 1);

        (Command::from_base_case(res, std::forward<Args>(args)), ...);

        return Command {std::move(res)};
    }

    static auto from_base_case(Argv& args, std::string& arg) -> void {
        args.emplace_back(arg);
    }

    static auto from_base_case(Argv& args, const std::fs::path& path) -> void {
        args.emplace_back(path.string());
    }

    static auto from_base_case(
        Argv& args,
        const std::fs::directory_iterator& it
    ) -> void {
        for (const auto& entry : it) {
            auto str = entry.path().string();
            args.push_back(entry.path().string());
        }
    }

#ifndef _WIN32
    static auto process_wait(Pid pid) -> Status {
        auto status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }

        panic("Process did not execute properly.");
    }

#else  // TODO: (Makoto) Make work on windows
    static auto process_wait(Pid pid) -> Status {
        panic("I bet windows does processes really weird");
    }
#endif

#ifndef _WIN32
    static auto exec(
        const std::vector<CStr>& args,
        const Pipe pipe,
        const bool verbose
    ) -> Pid {
        // TODO: (Carter) Reduce the amount of write syscalls
        if (verbose) {
            // Accounting for nullptr termination
            const auto end = args.cend() - 2;
            for (auto it = args.cbegin(); it != args.cend() - 1; ++it) {
                write(pipe.write, *it, std::strlen(*it) + 1);
                if (it != end) {
                    write(pipe.write, " ", 1);
                }
            }
            write(pipe.write, "\n", 1);
        }

        auto child_pid = fork();
        if (child_pid < 0) {
            panic("Failed to fork process.");
        }

        if (child_pid == 0) {
            if (dup2(pipe.read, STDIN_FILENO) < 0) {
                panic("Failed to setup stdin for child process.");
            }

            if (dup2(pipe.write, STDOUT_FILENO) < 0) {
                panic("Failed to setup stdout for child process.");
            }

            if (execvp(args.front(), const_cast<char* const*>(args.data()))
                != 0) {
                panic("Failed to exec child process.");
            }
        }

        return child_pid;
    }

#else  // TODO: (Makoto) Make work on windows
    static auto exec() -> Pid {
        panic("Good luck on this one");
    }
#endif

    [[nodiscard]] auto c_str_args() const -> std::vector<CStr> {
        auto cstr_args = std::vector<CStr> {};
        cstr_args.reserve(args.size() + 1);

        for (const auto& arg : args) {
            cstr_args.emplace_back(arg.data());
        }
        cstr_args.emplace_back(nullptr);

        return cstr_args;
    }

    auto run(const Config& config) -> Status {
        const auto child_pid = Command::exec(
            this->c_str_args(),
            config.pipe,
            config.verbose
        );

        return Command::process_wait(child_pid);
    }

    auto run_async(const Config& config) -> Future {
        const auto child_pid = Command::exec(
            this->c_str_args(),
            config.pipe,
            config.verbose
        );

        return Future::from(child_pid);
    }

    auto arg(CStr arg) -> Command& {
        args.push_back(arg);
        return *this;
    }

    auto concat(const Command& cmd) -> Command& {
        args.insert(args.cend(), cmd.args.cbegin(), cmd.args.cend());
        return *this;
    }

    friend auto operator<<(std::ostream& ostream, const Command& cmd)
        -> std::ostream& {
        const auto& argv = cmd.args;
        const auto end   = argv.cend() - 1;

        for (auto it = argv.cbegin(); it != argv.cend(); ++it) {
            ostream << *it;
            if (it != end) {
                ostream << ' ';
            }
        }

        return ostream;
    }
};

struct NullOpt {
    friend auto operator<<(
        std::ostream& ostream,
        [[maybe_unused]] const NullOpt& nullopt
    ) -> std::ostream& {
        return ostream << "None";
    }
};

template<typename T = NullOpt>
struct Opt {
    [[no_unique_address]] T value;

    [[nodiscard]] constexpr auto is_some() const -> bool {
        return std::is_same_v<T, NullOpt>;
    }

    [[nodiscard]] constexpr auto is_none() const -> bool {
        return !this->is_some();
    }

    [[nodiscard]] constexpr auto unwrap() const -> T {
        if constexpr (std::is_same_v<T, NullOpt>) {
            panic("Tried to unwrap nullopt.");
        } else {
            return value;
        }
    }

    [[nodiscard]] constexpr auto unwrap_or(T&& val) const -> T {
        if constexpr (std::is_same_v<T, NullOpt>) {
            return std::forward<T>(val);
        } else {
            return val;
        }
    }

    template<typename Fn>
        requires(std::is_invocable_v<Fn>
                 && std::is_same_v<T, std::invoke_result_t<Fn>>)
    [[nodiscard]] constexpr auto unwrap_or_else(Fn&& fn) const -> T {
        if constexpr (std::is_same_v<T, NullOpt>) {
            return std::invoke(std::forward<Fn>(fn));
        } else {
            return value;
        }
    }

    friend auto operator<<(std::ostream& ostream, const Opt& opt)
        -> std::ostream& {
        if constexpr (opt.is_some()) {
            return ostream << "Some(" << opt.value << ")";
        } else {
            return ostream << "None";
        }
    }
};

namespace fs {}  // namespace fs

template<typename T>
constexpr inline auto Some(T&& value) -> Opt<T> {
    return Opt<T> {std::forward<T>(value)};
}

static constexpr auto None = Opt<NullOpt> {};

// TODO: (Carter) Make TaskLists lazy and non-inherited.
struct TaskList: public std::vector<Command::Future> {
    using std::vector<Command::Future>::vector;

    auto wait() -> void {
        for (auto& task : *this) {
            task.wait();
        }
    }

    auto get() -> std::vector<Command::Status> {
        auto res = std::vector<Command::Status> {};
        for (auto& task : *this) {
            res.push_back(task.get());
        }
        return res;
    }
};
}  // namespace bootstrab

//#endif