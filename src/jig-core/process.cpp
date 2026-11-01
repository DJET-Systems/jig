// SPDX-License-Identifier: GPL-3.0-only
#include "jig-core/process.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>

extern char** environ;

namespace jig {

namespace {

struct ChildSpec {
    std::vector<char*> argv;
    std::vector<std::string> env_storage;
    std::vector<char*> envp;
};

ChildSpec make_spec(const std::vector<std::string>& argv, const EnvMap& env) {
    ChildSpec s;
    for (const auto& a : argv) s.argv.push_back(const_cast<char*>(a.c_str()));
    s.argv.push_back(nullptr);
    for (const auto& [k, v] : env) s.env_storage.push_back(k + "=" + v);
    for (auto& e : s.env_storage) s.envp.push_back(e.data());
    s.envp.push_back(nullptr);
    return s;
}

[[noreturn]] void child_exec(ChildSpec& spec, const fs::path& cwd) {
    if (!cwd.empty() && ::chdir(cwd.c_str()) != 0) _exit(127);
    ::environ = spec.envp.data();  // execvp then resolves the program through the *child's* PATH
    ::execvp(spec.argv[0], spec.argv.data());
    _exit(127);
}

int decode_status(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

}  // namespace

ProcessResult run_streaming(const std::vector<std::string>& argv, const fs::path& cwd, const EnvMap& env,
                            const std::function<void(const std::string&)>& on_line, bool keep_output) {
    ProcessResult r;
    if (argv.empty()) return r;
    int fds[2];
    if (::pipe(fds) != 0) return r;
    auto spec = make_spec(argv, env);
    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return r;
    }
    if (pid == 0) {
        ::close(fds[0]);
        ::dup2(fds[1], STDOUT_FILENO);
        ::dup2(fds[1], STDERR_FILENO);
        ::close(fds[1]);
        int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            ::close(devnull);
        }
        child_exec(spec, cwd);
    }
    ::close(fds[1]);
    r.spawned = true;
    std::string pending;
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(fds[0], buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        if (keep_output) r.output.append(buf, static_cast<std::size_t>(n));
        pending.append(buf, static_cast<std::size_t>(n));
        std::size_t nl;
        while ((nl = pending.find('\n')) != std::string::npos) {
            if (on_line) on_line(pending.substr(0, nl));
            pending.erase(0, nl + 1);
        }
    }
    if (!pending.empty() && on_line) on_line(pending);
    ::close(fds[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    r.exit_status = decode_status(status);
    if (r.exit_status == 127 && !keep_output) {
        // 127 from our child_exec failure and a program's own 127 look alike; callers that care probe with which().
    }
    return r;
}

ProcessResult run_capture(const std::vector<std::string>& argv, const EnvMap& env, const fs::path& cwd) {
    return run_streaming(argv, cwd, env, nullptr, true);
}

int run_inherit(const std::vector<std::string>& argv, const fs::path& cwd, const EnvMap& env) {
    if (argv.empty()) return -1;
    auto spec = make_spec(argv, env);
    pid_t pid = ::fork();
    if (pid < 0) return -1;
    if (pid == 0) child_exec(spec, cwd);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return decode_status(status);
}

}  // namespace jig
