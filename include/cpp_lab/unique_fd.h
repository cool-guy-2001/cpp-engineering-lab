#pragma once

namespace cpp_lab {
class UniqueFd {
public:
  UniqueFd() noexcept = default;
  explicit UniqueFd(int fd) noexcept;

  ~UniqueFd() noexcept;

  UniqueFd(const UniqueFd &) = delete;
  UniqueFd &operator=(const UniqueFd &) = delete;

  UniqueFd(UniqueFd &&other) noexcept;
  UniqueFd &operator=(UniqueFd &&other) noexcept;

  int get() const noexcept;
  bool valid() const noexcept;

  int release() noexcept;//释放所有权
  void reset(int new_fd = -1) noexcept;//重新设置资源

private:
  int fd_ = {-1};
};
} // namespace cpp_lab
