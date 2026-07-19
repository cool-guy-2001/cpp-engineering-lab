#include "cpp_lab/unique_fd.h"

#include <unistd.h>

#include <functional>

namespace cpp_lab {

UniqueFd::UniqueFd(int fd) noexcept
    : fd_(fd) {
}

UniqueFd::~UniqueFd() noexcept {
    reset();
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept
    : fd_(other.release()) {
}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

int UniqueFd::get() const noexcept {
    return fd_;
}
bool UniqueFd::valid() const noexcept {
    return fd_ >= 0;
}

int UniqueFd::release() noexcept {  //返回当前描述符，但不关闭它；随后让 UniqueFd 进入空状态。
    int old_fd = fd_;
    fd_ = -1;
    return old_fd;
}

void UniqueFd::reset(int new_fd) noexcept {
    if (fd_ == new_fd) {
        return;
    }
    if (valid()) {
        ::close(fd_);
    }
    fd_ = new_fd;
}
}  // namespace cpp_lab
