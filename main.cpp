#include "cpp_lab/unique_fd.h"

#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace {

bool is_fd_open(int fd) {
    return ::fcntl(fd, F_GETFD) != -1;
}

bool is_fd_closed(int fd) {
    errno = 0;
    const int result = ::fcntl(fd, F_GETFD);

    return result == -1 && errno == EBADF;
}

void test_default_constructor() {
    cpp_lab::UniqueFd fd;

    assert(!fd.valid());
    assert(fd.get() == -1);

    std::cout << "default constructor: passed\n";
}

void test_constructor_and_destructor() {
    int pipe_fds[2]{};
    assert(::pipe(pipe_fds) == 0);

    const int read_fd = pipe_fds[0];

    {
        cpp_lab::UniqueFd owner{read_fd};

        assert(owner.valid());
        assert(owner.get() == read_fd);
        assert(is_fd_open(read_fd));
    }

    // owner 已离开作用域，析构函数应关闭 read_fd。
    assert(is_fd_closed(read_fd));

    ::close(pipe_fds[1]);

    std::cout << "constructor and destructor: passed\n";
}

void test_release() {
    int pipe_fds[2]{};
    assert(::pipe(pipe_fds) == 0);

    cpp_lab::UniqueFd owner{pipe_fds[0]};

    const int released_fd = owner.release();

    assert(!owner.valid());
    assert(owner.get() == -1);
    assert(released_fd == pipe_fds[0]);

    // release() 只转移所有权，不能关闭 fd。
    assert(is_fd_open(released_fd));

    // 现在所有权由测试代码负责。
    ::close(released_fd);
    ::close(pipe_fds[1]);

    std::cout << "release: passed\n";
}

void test_move_constructor() {
    int pipe_fds[2]{};
    assert(::pipe(pipe_fds) == 0);

    const int raw_fd = pipe_fds[0];

    cpp_lab::UniqueFd source{raw_fd};
    cpp_lab::UniqueFd target{std::move(source)};

    assert(!source.valid());
    assert(source.get() == -1);

    assert(target.valid());
    assert(target.get() == raw_fd);
    assert(is_fd_open(raw_fd));

    ::close(pipe_fds[1]);

    std::cout << "move constructor: passed\n";
}

void test_move_assignment() {
    int source_pipe[2]{};
    int target_pipe[2]{};

    assert(::pipe(source_pipe) == 0);
    assert(::pipe(target_pipe) == 0);

    const int source_fd = source_pipe[0];
    const int old_target_fd = target_pipe[0];

    {
        cpp_lab::UniqueFd source{source_fd};
        cpp_lab::UniqueFd target{old_target_fd};

        target = std::move(source);

        assert(!source.valid());

        assert(target.valid());
        assert(target.get() == source_fd);

        // target 原来拥有的描述符必须被关闭。
        assert(is_fd_closed(old_target_fd));

        // source 原来的描述符现在由 target 管理，仍然有效。
        assert(is_fd_open(source_fd));
    }

    // target 离开作用域后，source_fd 也应该被关闭。
    assert(is_fd_closed(source_fd));

    ::close(source_pipe[1]);
    ::close(target_pipe[1]);

    std::cout << "move assignment: passed\n";
}

}  // namespace

int main() {
    // 编译期验证所有权语义。
    static_assert(!std::is_copy_constructible_v<cpp_lab::UniqueFd>);
    static_assert(!std::is_copy_assignable_v<cpp_lab::UniqueFd>);

    static_assert(std::is_move_constructible_v<cpp_lab::UniqueFd>);
    static_assert(std::is_move_assignable_v<cpp_lab::UniqueFd>);

    test_default_constructor();
    test_constructor_and_destructor();
    test_release();
    test_move_constructor();
    test_move_assignment();

    std::cout << "all UniqueFd tests passed\n";
}
