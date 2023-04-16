#ifdef BOOTSTRAB_IMPLEMENTATION  // Comment out if experiencing linter issues

    #ifndef _WIN32  // LINUX INCLUDE

        #include <fcntl.h>
        #include <sys/wait.h>
        #include <unistd.h>

    #else  // WINDOWS INCLUDE

        #include <process.h>
        #include <windows.h>

    #endif  // STD INCLUDE

    #include <cstring>
    #include <iostream>
    #include <vector>

namespace bootstrab {

using CStr = const char*;

inline auto panic(CStr message, std::ostream& ostream = std::cerr) -> void {
    ostream << "[PANIC]: " << message << '\n';
    std::abort();
}

using Fd = int;

struct Pipe {
    Fd read;
    Fd write;

    #ifndef _WIN32
    static auto Inherited() -> Pipe {
        return {STDIN_FILENO, STDOUT_FILENO};
    }

    #else  // TODO: (Makoto) Make work on windows
    static auto Inherited() -> Pipe {
        panic("haha imagine using windows");
        return {-1, -1};
    }
    #endif

    #ifndef _WIN32
    static auto Owned(Fd read, Fd write) -> Pipe {
        Fd pipefd[2];  // NOLINT
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
        return {-1, -1};
    }
    #endif

    #ifndef _WIN32
    static auto Null() -> Pipe {
        Fd pipefd[2];  // NOLINT
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
        return {-1, -1};
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
    using Status = int;

    std::vector<CStr> args {nullptr};

    struct Future {
        pid_t pid;

        constexpr static auto from(pid_t pid) -> Future {
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
        Pipe pipe {Pipe::Null()};

        ~Config() {
            pipe.deinit();
        }
    };

    template<typename... Args>
    static auto from(Args&&... args) -> Command {
        return {{std::forward<Args>(args)...}};  // NOLINT
    }

    #ifndef _WIN32
    static auto process_wait(pid_t pid) -> Status {
        auto status = 0;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }

        panic("Process did not execute properly.");
        return -1;  // Unreachable
    }

    #else  // TODO: (Makoto) Make work on windows
    static auto process_wait(pid_t pid) -> Status {
        panic("I bet windows does processes really weird");
        return -1;
    }
    #endif

    #ifndef _WIN32
    static auto exec(
        const std::vector<CStr>& args,
        const Pipe pipe,
        const bool verbose
    ) -> pid_t {
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

            if (execvp(args.front(), (char* const*)args.data()) != 0) {
                panic("Failed to exec child process.");
            }
        }

        return child_pid;
    }

    #else  // TODO: (Makoto) Make work on windows
    static auto exec() -> pid_t {
        panic("Good luck on this one");
        return -1;
    }
    #endif

    auto run(const Config& config) -> Status {
        args.push_back(nullptr);
        auto child_pid = Command::exec(args, config.pipe, config.verbose);
        args.pop_back();

        return Command::process_wait(child_pid);
    }

    auto run_async(const Config& config) -> Future {
        args.push_back(nullptr);
        auto child_pid = Command::exec(args, config.pipe, config.verbose);
        args.pop_back();

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
        const auto& args = cmd.args;
        const auto end   = args.cend() - 1;

        for (auto it = args.cbegin(); it != args.cend(); ++it) {
            ostream << *it;
            if (it != end) {
                ostream << ' ';
            }
        }

        return ostream;
    }
};

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

#endif
