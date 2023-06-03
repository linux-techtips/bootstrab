#ifdef BOOTSTRAB_IMPLEMENTATION  // Comment out if experiencing linter issues

    #include <filesystem>
    #include <functional>
    #include <iostream>
    #include <vector>

    #ifndef _WIN32  // LINUX INCLUDE
namespace sys {
        #include <fcntl.h>
        #include <sys/wait.h>
        #include <unistd.h>

constexpr auto* PATH_SEP            = "/";
static const auto dev_null_fd_read  = open("/dev/null", O_RDONLY);
static const auto dev_null_fd_write = open("/dev/null", O_WRONLY);
}  // namespace sys

    #else  // WINDOWS INCLUDE
namespace sys {
        #include <process.h>
        #include <windows.h>

constexpr auto* PATH_SEP = "//";
}  // namespace sys

    #endif

namespace std {  // I await my seat on the C++ committee.
namespace fs = filesystem;
}

namespace bootstrab {

using CStr = char const*;
using Fd   = int;
using Pid  = pid_t;

[[noreturn]] inline auto panic(
    const CStr message,
    std::ostream& ostream = std::cerr
) -> void {
    ostream << "[PANIC]: " << message << '\n';
    std::abort();
}

namespace fs {

    inline auto path_modified_after(const CStr path1, const CStr path2)
        -> bool {
        auto path1_write_time = std::fs::file_time_type {};
        auto path2_write_time = std::fs::file_time_type {};

        try {
            path1_write_time = std::fs::last_write_time(path1);
        } catch (std::fs::filesystem_error) {
            return false;
        }

        try {
            path2_write_time = std::fs::last_write_time(path2);
        } catch (std::fs::filesystem_error) {
            return true;
        }

        return path1_write_time > path2_write_time;
    }

    template<typename T>
    concept DirectoryIterator = std::is_same_v<
                                    typename T::value_type,
                                    std::fs::directory_entry>
        && !
    std::is_const_v<T>;

    template<typename T>
    concept DirectoryPredicate = std::is_invocable_r_v<
                                     bool,
                                     std::decay_t<T>,
                                     std::decay_t<std::fs::directory_entry>>
        && !
    std::is_const_v<T>;

    template<DirectoryIterator Iter, DirectoryPredicate Pred>
    struct DirectoryFilterIterator {  // I hate C++ iterators
        using iterator_category = typename std::input_iterator_tag;
        using value_type        = typename Iter::value_type;
        using difference_type   = typename Iter::difference_type;
        using pointer           = typename Iter::pointer;
        using reference         = typename Iter::reference;
        using const_reference   = const typename Iter::reference;
        using const_pointer     = const typename Iter::pointer;

        Iter curr;
        Iter last {};
        Pred pred;

        explicit DirectoryFilterIterator(Iter& it, Pred&& predicate) :
            curr {it},
            pred {predicate} {
            this->next();
        }

        DirectoryFilterIterator() = default;

        auto operator++() -> DirectoryFilterIterator& {
            ++curr;
            this->next();
            return *this;
        }

        auto operator++(int) -> DirectoryFilterIterator& {
            auto res = *this;
            ++curr;
            this->next();
            return *this;
        }

        auto operator*() const -> const_reference {
            return *curr;
        }

        auto operator->() const -> const_pointer {
            return &(*curr);
        }

        friend auto operator==(
            const DirectoryFilterIterator& lhs,
            const DirectoryFilterIterator& rhs
        ) -> bool {
            return lhs.curr == rhs.curr;
        }

        friend auto operator!=(
            const DirectoryFilterIterator& lhs,
            const DirectoryFilterIterator& rhs
        ) -> bool {
            return !(lhs == rhs);
        }

        auto next() -> void {
            for (; curr != last && !std::invoke(pred, *curr); ++curr) {
            }
        }
    };

    template<DirectoryIterator Iter, DirectoryPredicate Pred>
    struct DirectoryFilter {
        using value_type = typename Iter::
            value_type;  // Not standard, but for compatibility.

        using iterator       = DirectoryFilterIterator<Iter, Pred>;
        using const_iterator = DirectoryFilterIterator<Iter, Pred>;

        Iter curr;
        Pred pred;

        auto begin() -> iterator {
            return iterator(curr, std::forward<Pred>(pred));
        }

        auto end() -> iterator {
            return iterator();
        }

        auto cbegin() const -> const_iterator {
            return const_iterator(curr, std::forward<Pred>(pred));
        }

        auto cend() const -> const_iterator {
            return const_iterator();
        }
    };

    template<DirectoryIterator Iter, DirectoryPredicate Pred>
    inline auto filter(const Iter& it, Pred&& predicate)
        -> DirectoryFilter<Iter, Pred> {
        return {it, std::forward<Pred>(predicate)};
    }

    template<DirectoryPredicate Pred>
    inline auto filter(const std::fs::path& path, Pred&& predicate)
        -> DirectoryFilter<std::filesystem::directory_iterator, Pred> {
        return {
            std::filesystem::directory_iterator(path),
            std::forward<Pred>(predicate)};
    }

    template<DirectoryPredicate Pred>
    inline auto filter(const CStr path, Pred&& predicate)
        -> DirectoryFilter<std::filesystem::directory_iterator, Pred> {
        return {
            std::filesystem::directory_iterator(std::fs::path(path)),
            std::forward<Pred>(predicate)};
    }

}  // namespace fs

namespace env {

    #if defined(__clang__)
    static constexpr auto* PARENT_COMPILER = "clang++";
    #elif defined(__GNUC__) || defined(__GNUG__)
    static constexpr auto* PARENT_COMPILER = "g++";
    #elif defined(_MSC_VER)
    // TODO (Makoto) Make sure this is correct.
    static constexpr auto* PARENT_COMPILER = "cl.exe";
    #elif defined(__MINGW32__) || defined(__MINGW64__)
    // TODO (Makoto) Make sure this is correct.
    static constexpr auto* PARENT_COMPILER = "g++";
    #endif

    struct Args: public std::vector<CStr> {
        static auto from(const int argc, char** argv) -> Args {
            return {std::vector<CStr>(argv, argv + argc)};
        }
    };

}  // namespace env

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
    }
    #endif

    #ifndef _WIN32
    static auto Owned(Fd read, Fd write) -> Pipe {
        Fd pipefd[2];  // NOLINT
        pipefd[0] = read;
        pipefd[1] = write;

        if (sys::pipe(pipefd) != 0) {
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
        // if you can't open /dev/null, you have bigger issues than an error on your build script
        return {sys::dev_null_fd_read, sys::dev_null_fd_write};
    }

    #else  // TODO: (Makoto) Make work on windows
    static auto Null() -> Pipe {
        panic("noooo windows nooooooo");
    }
    #endif

    auto deinit() -> void {
        sys::close(read);
        sys::close(write);
    }
};

template<typename T>  // Why did my linter do this?
concept IterableToString = requires(T it) {
                               {
                                   std::begin(it)
                                   } -> std::input_or_output_iterator;
                               {
                                   std::end(it)
                                   } -> std::input_or_output_iterator;
                               std::is_convertible_v<decltype(*std::begin(it)), T>;
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
        Pipe pipe {Pipe::Null()};
        bool verbose {false};

        ~Config() {
            pipe.deinit();
        }
    };

    template<typename... Args>
    static auto from(Args&&... args) -> Command {
        auto res = Argv {};
        res.reserve(sizeof...(Args) + 1);

        (Command::from_base_case(res, std::forward<Args>(args)), ...);

        return {std::move(res)};
    }

    // Don't ask me why
    static auto from_base_case(Argv& args, std::string&& arg) -> void {
        args.push_back(std::forward<std::string>(arg));
    }

    static auto from_base_case(Argv& args, std::string& arg) -> void {
        args.push_back(arg);
    }

    static auto from_base_case(Argv& args, const std::string& arg) -> void {
        args.push_back(arg);
    }

    template<IterableToString T>
    static auto from_base_case(Argv& args, T& it) -> void {
        for (const auto& entry : it) {
            args.push_back(entry);
        }
    }

    template<fs::DirectoryIterator Iter, fs::DirectoryPredicate Pred>
    static auto from_base_case(Argv& args, fs::DirectoryFilter<Iter, Pred>& it)
        -> void {
        for (const auto& entry : it) {
            args.push_back(entry.path().string());
        }
    }

    #ifndef _WIN32
    static auto process_wait(Pid pid) -> Status {
        auto status = 0;
        sys::waitpid(pid, &status, 0);

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
    auto exec(const Config& config) -> Pid {
        if (config.verbose) {
            std::cerr << "[INFO]: " << *this << '\n';
        }

        auto child_pid = sys::fork();
        if (child_pid < 0) {
            panic("Failed to fork process.");
        }

        if (child_pid == 0) {
            const auto& fd_read  = config.pipe.read;
            const auto& fd_write = config.pipe.write;

            if (fd_read != STDIN_FILENO) {
                sys::dup2(fd_read, STDIN_FILENO);
            }

            // git will not like you if you try to pipe to /dev/null
            if (fd_write != STDOUT_FILENO) {
                sys::dup2(fd_write, STDOUT_FILENO);
            }

            const auto& argv = this->c_str_args();
            if (sys::execvp(argv.front(), const_cast<char* const*>(argv.data()))
                != 0) {
                panic("Task exited abnormally.");
            }
        }

        return child_pid;
    }

    #else  // TODO: (Makoto) Make work on windows
    auto exec(const Config& config) -> Pid {
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
        const auto child_pid = Command::exec(config);
        return Command::process_wait(child_pid);
    }

    auto run_async(const Config& config) -> Future {
        const auto child_pid = Command::exec(config);
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

    #define REBUILD_URSELF(args) rebuild_urself(__FILE__, (args))  // NOLINT

constexpr inline auto rebuild_urself(const CStr source, const env::Args& args)
    -> void {
    const auto* target = args[0];
    if (!fs::path_modified_after(source, target)) {
        return;
    }

    std::cout << "Change detected, rebuilding...\n";

    #if defined(_MSC_VER)  // TODO: (Makoto) Make sure this is correct
    Command::from(
        env::PARENT_COMPILER,
        "/EHsc",
        "/std:c++20",
        source,
        std::string {"/Fe:"} + target
    )
        .run({});
    #else
    Command::from(env::PARENT_COMPILER, "-std=c++20", source, "-o", target)
        .run({});
    #endif
    Command::from(target, args).run({.pipe = Pipe::Inherited()});

    std::exit(0);  // NOLINT
}

}  // namespace bootstrab

#endif
