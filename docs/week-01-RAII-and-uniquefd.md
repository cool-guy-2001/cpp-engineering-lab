# RAII 与 `UniqueFd` 学习笔记

> 目标：理解 RAII、资源所有权、移动语义和异常安全，并能够独立实现、测试和解释一个管理 POSIX 文件描述符的 `UniqueFd`。

## 1. RAII 解决什么问题？

RAII 是 **Resource Acquisition Is Initialization**，即“资源获取即初始化”。

它把资源的生命周期与对象的生命周期绑定：

- 构造对象时获取或接管资源；
- 对象存活期间拥有资源；
- 对象析构时自动释放资源。

这里的“资源”不只是内存，还包括：

- 文件描述符；
- Socket；
- 文件；
- 锁；
- 线程；
- 数据库连接；
- 映射内存；
- GPU资源。

RAII主要解决以下问题：

1. 忘记释放资源；
2. 函数存在多个返回路径时，清理逻辑重复；
3. 异常导致正常清理代码没有执行；
4. 资源所有权不清晰；
5. 获取了多个资源后，中途失败造成部分资源泄漏。

### 没有RAII的写法

```cpp
void process_file(const char* path) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("open failed");
    }

    do_something(fd);  // 如果这里抛出异常

    ::close(fd);       // 这一行不会执行，fd发生泄漏
}
```

### 使用RAII

```cpp
void process_file(const char* path) {
    UniqueFd fd(::open(path, O_RDONLY));
    if (!fd.valid()) {
        throw std::runtime_error("open failed");
    }

    do_something(fd.get());
}  // 无论正常返回还是发生异常，fd都会在这里析构
```

RAII并不是“完全不需要考虑错误”，而是保证**资源释放动作一定会被触发**。

---

## 2. `UniqueFd`管理什么资源？

`UniqueFd`管理一个 POSIX 文件描述符，即一个非负整数，例如：

```cpp
int fd = ::open("data.log", O_RDONLY);
```

文件描述符可以表示：

- 普通文件；
- Socket；
- Pipe；
- eventfd；
- epoll实例；
- timerfd；
- 设备文件。

`UniqueFd`管理的不是整数本身，而是该整数所代表的、需要通过 `close()` 释放的操作系统资源。

只复制文件描述符的整数值，并不会创建一份独立资源：

```cpp
int fd2 = fd1;  // 只是复制数字，不是复制或转移资源
```

如果确实需要创建另一个可独立关闭的文件描述符，应使用 `dup()`、`dup2()` 或类似系统调用。

---

## 3. 谁拥有资源？

持有有效文件描述符的 `UniqueFd` 对象是资源的唯一所有者。

“拥有”表示该对象负责：

1. 保存文件描述符；
2. 控制资源生命周期；
3. 在析构或 `reset()` 时调用 `close()`；
4. 保证同一文件描述符不会被两个 `UniqueFd` 同时释放。

```cpp
UniqueFd fd(::open("data.log", O_RDONLY));
```

执行后：

- `fd`拥有文件描述符；
- 调用者不应再手动调用 `close(fd.get())`；
- 如果需要把资源交给其他代码，应通过移动、`release()`或明确约定的接口完成。

`get()`只提供临时访问权，不转移所有权：

```cpp
read_data(fd.get());  // read_data可以使用fd，但不应关闭它
```

---

## 4. `UniqueFd`有哪些状态？

逻辑上可以分为以下状态：

### 4.1 空状态

```cpp
UniqueFd fd;
```

内部保存 `-1`，不拥有任何资源。析构时不执行 `close()`。

### 4.2 有效拥有状态

```cpp
UniqueFd fd(::open("data.log", O_RDONLY));
```

如果 `open()`成功，内部值大于或等于0，`fd`拥有该文件描述符。

### 4.3 移动后状态

```cpp
UniqueFd first(::open("data.log", O_RDONLY));
UniqueFd second(std::move(first));
```

移动后：

- `second`拥有资源；
- `first`必须变为空状态；
- `first`仍然是合法对象，可以析构、重新赋值或调用 `reset()`；
- 不能继续假设 `first`拥有原资源。

移动后状态通常和空状态使用相同表示：`fd_ == -1`。

---

## 5. 为什么禁止拷贝？

`UniqueFd`表达的是唯一所有权，因此同一时刻只能有一个对象负责关闭文件描述符。

如果允许默认拷贝：

```cpp
UniqueFd first(::open("data.log", O_RDONLY));
UniqueFd second = first;
```

那么两个对象会保存相同的整数值。离开作用域时：

1. `second`调用一次`close()`；
2. `first`再次调用`close()`；
3. 发生重复关闭。

重复关闭尤其危险：第一次关闭后，操作系统可能把相同的文件描述符编号分配给另一个新资源，第二次关闭就可能错误关闭无关资源。

因此需要删除拷贝操作：

```cpp
UniqueFd(const UniqueFd&) = delete;
UniqueFd& operator=(const UniqueFd&) = delete;
```

如果业务确实需要另一个文件描述符，应该显式调用 `dup()`。这使资源复制成本和语义对调用者保持可见。

---

## 6. 为什么支持移动？

禁止拷贝不代表对象不能转移。

移动用于把资源所有权从一个对象转交给另一个对象：

```cpp
UniqueFd open_file(const char* path) {
    UniqueFd fd(::open(path, O_RDONLY));
    return fd;
}

UniqueFd fd = open_file("data.log");
```

移动必须满足：

1. 目标对象获得原文件描述符；
2. 源对象失去所有权并进入空状态；
3. 整个过程不复制底层操作系统资源；
4. 最终仍然只有一个对象负责关闭资源。

支持移动后，`UniqueFd`可以：

- 从工厂函数返回；
- 作为函数参数传递所有权；
- 存放到支持移动类型的标准容器中；
- 作为其他资源管理类的成员。

---

## 7. 为什么移动操作需要`noexcept`？

`UniqueFd`的移动只需要转移一个整数并把源对象设为 `-1`，正常情况下不会失败，因此应该声明为 `noexcept`：

```cpp
UniqueFd(UniqueFd&& other) noexcept;
UniqueFd& operator=(UniqueFd&& other) noexcept;
```

主要原因有两个。

### 7.1 表达真实语义

`noexcept`明确告诉调用者和编译器：该操作不会抛出异常。

### 7.2 帮助标准容器保证异常安全

`std::vector`扩容时需要把旧元素转移到新内存。如果类型的移动构造可能抛异常，容器可能更倾向于使用拷贝来维持强异常安全保证。

但`UniqueFd`禁止拷贝，因此将移动操作声明为`noexcept`能够让它更可靠地用于标准容器和泛型代码。

可以通过编译期断言验证：

```cpp
static_assert(std::is_nothrow_move_constructible_v<UniqueFd>);
static_assert(std::is_nothrow_move_assignable_v<UniqueFd>);
```

---

## 8. 析构函数什么时候执行？

典型执行时机包括：

1. 局部对象离开作用域；
2. 函数正常返回；
3. 异常发生后进行栈展开；
4. 所属对象被析构；
5. 容器销毁或删除元素；
6. `delete`一个动态创建的对象；
7. 静态存储期对象在程序退出阶段销毁。

示例：

```cpp
void example() {
    UniqueFd fd(::open("data.log", O_RDONLY));

    if (some_condition()) {
        return;
    }

    do_something();
}  // 两条路径都会执行fd的析构函数
```

如果对象是通过裸 `new` 创建但没有执行 `delete`，析构函数不会自动运行：

```cpp
auto* fd = new UniqueFd(::open("data.log", O_RDONLY));
// 忘记delete fd，UniqueFd对象和文件描述符都会泄漏
```

所以RAII对象本身也应放在栈上，或者由另一个RAII对象（如`std::unique_ptr`）管理。

---

## 9. 为什么析构函数不能抛异常？

析构函数通常会在栈展开期间执行。如果此时已经存在一个正在传播的异常，而析构函数又抛出第二个异常，程序会调用 `std::terminate()`。

```cpp
class BadResource {
public:
    ~BadResource() {
        throw std::runtime_error("cleanup failed");
    }
};
```

因此资源管理类的析构函数应明确或隐式满足 `noexcept`：

```cpp
~UniqueFd() noexcept;
```

对于`close()`失败：

- 析构函数不能通过抛异常报告；
- 可以在调试场景记录错误；
- 如果调用者必须知道写入或持久化是否成功，应提供显式的`flush()`、`sync()`或`close()`接口，在析构前检查返回值；
- 析构函数主要负责尽最大努力回收资源。

在存储系统中尤其要注意：

> `close()`自动执行不等于数据已经按照预期持久化。需要持久性保证时，应显式检查`write()`和`fsync()`等操作的结果。

---

## 10. 异常发生时资源是否泄漏？

如果资源已经被一个正确实现的RAII对象接管，异常通常不会导致资源泄漏：

```cpp
void example() {
    UniqueFd fd(::open("data.log", O_RDONLY));
    if (!fd.valid()) {
        throw std::runtime_error("open failed");
    }

    do_something_that_throws();
}  // 栈展开时调用fd的析构函数
```

但是以下情况仍然可能泄漏。

### 10.1 获取资源后，没有立即交给RAII对象

```cpp
int raw_fd = ::open("data.log", O_RDONLY);
do_something_that_throws();
UniqueFd fd(raw_fd);  // 没有执行到这里
```

应当立即接管：

```cpp
UniqueFd fd(::open("data.log", O_RDONLY));
```

### 10.2 调用`release()`后没有处理返回值

```cpp
fd.release();  // 返回值被丢弃，资源无人负责关闭
```

应当立即转交：

```cpp
UniqueFd another(fd.release());
```

或者明确由底层API接管：

```cpp
legacy_api_take_ownership(fd.release());
```

### 10.3 两个`UniqueFd`错误接管同一个描述符

```cpp
int raw_fd = ::open("data.log", O_RDONLY);
UniqueFd first(raw_fd);
UniqueFd second(raw_fd);  // 错误：形成两个所有者
```

### 10.4 `UniqueFd`的实现本身有错误

例如：

- 析构函数没有关闭资源；
- 移动后没有清空源对象；
- 移动赋值覆盖目标资源前没有关闭旧资源；
- `reset()`直接覆盖内部描述符；
- 类错误地允许拷贝。

---

## 11. `reset()`和`release()`有什么区别？

### `reset()`

`reset()`的作用是：

1. 关闭当前拥有的资源；
2. 可选地接管一个新文件描述符。

```cpp
fd.reset();          // 关闭当前fd，对象变为空
fd.reset(new_fd);    // 关闭当前fd，接管new_fd
```

调用完成后，`UniqueFd`仍然负责管理其内部保存的新资源。

### `release()`

`release()`的作用是：

1. 返回当前文件描述符；
2. 不调用`close()`；
3. 放弃所有权；
4. 对象进入空状态。

```cpp
int raw_fd = fd.release();
```

调用完成后，调用者负责关闭或再次转交`raw_fd`。

### 对比

| 操作 | 是否关闭旧资源 | 是否返回资源 | 操作后谁拥有资源 |
|---|---:|---:|---|
| `reset()` | 是 | 否 | `UniqueFd`或无人拥有 |
| `reset(new_fd)` | 是 | 否 | `UniqueFd`拥有`new_fd` |
| `release()` | 否 | 是 | 调用者拥有返回的fd |

记忆方式：

- `reset()`：处理掉旧资源，并重置所有权；
- `release()`：只释放所有权，不释放底层资源。

---

## 12. 参考实现

```cpp
#pragma once

#include <unistd.h>

#include <utility>

class UniqueFd {
public:
    UniqueFd() noexcept = default;

    explicit UniqueFd(int fd) noexcept
        : fd_(fd) {}

    ~UniqueFd() noexcept {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_(other.release()) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

    [[nodiscard]] bool valid() const noexcept {
        return fd_ >= 0;
    }

    explicit operator bool() const noexcept {
        return valid();
    }

    [[nodiscard]] int release() noexcept {
        return std::exchange(fd_, invalid_fd);
    }

    void reset(int new_fd = invalid_fd) noexcept {
        if (fd_ == new_fd) {
            return;
        }

        const int old_fd = std::exchange(fd_, new_fd);
        if (old_fd >= 0) {
            // 析构和reset不能通过异常报告close失败。
            // 如果业务必须确认数据持久化，应提前显式检查write/fsync结果。
            ::close(old_fd);
        }
    }

private:
    static constexpr int invalid_fd = -1;
    int fd_{invalid_fd};
};
```

### 编译期检查

```cpp
#include <type_traits>

static_assert(!std::is_copy_constructible_v<UniqueFd>);
static_assert(!std::is_copy_assignable_v<UniqueFd>);
static_assert(std::is_nothrow_move_constructible_v<UniqueFd>);
static_assert(std::is_nothrow_move_assignable_v<UniqueFd>);
static_assert(std::is_nothrow_destructible_v<UniqueFd>);
```

---

## 13. 建议测试清单

### 基本状态

- 默认构造后无效；
- 接管有效文件描述符后有效；
- `reset()`后无效；
- `release()`后无效。

### 析构

- 离开作用域后文件描述符被关闭；
- 函数提前返回时文件描述符被关闭；
- 异常栈展开时文件描述符被关闭。

### 移动

- 移动构造后目标有效；
- 移动构造后源对象无效；
- 移动赋值会先关闭目标原有资源；
- 自移动赋值不会破坏对象；
- 移动后只有目标对象负责关闭资源。

### `reset()`和`release()`

- `reset(new_fd)`关闭旧资源并接管新资源；
- `reset()`关闭旧资源并进入空状态；
- `release()`不关闭资源；
- `release()`返回的资源最终由调用者关闭；
- `reset(get())`不会错误关闭后继续保存相同编号。

### 工具检查

- AddressSanitizer无资源生命周期相关错误；
- 编译器无警告；
- `clang-tidy`无所有权和移动相关严重警告。

---

## 14. 面试快速回答

### RAII解决什么问题？

把资源释放与对象析构绑定，避免正常返回、提前返回或异常路径中的资源泄漏，并让所有权更加清晰。

### `UniqueFd`谁拥有资源？

内部保存有效文件描述符的`UniqueFd`对象是唯一所有者，并负责最终调用`close()`。

### 为什么禁止拷贝？

默认拷贝只会复制文件描述符整数，产生两个所有者，最终可能重复关闭同一个描述符。

### 为什么支持移动？

为了在不复制底层资源的情况下转移唯一所有权，例如从函数返回或存入容器。

### 为什么移动需要`noexcept`？

移动只转移整数，本身不应失败；`noexcept`也有助于标准容器在保持异常安全的同时使用移动操作。

### 为什么析构函数不能抛异常？

析构可能发生在异常栈展开期间，如果析构再抛出异常，程序会调用`std::terminate()`。

### 异常发生时资源是否泄漏？

资源被RAII对象接管后，栈展开会调用析构函数，因此通常不会泄漏；但获取资源后未立即接管或调用`release()`后遗失返回值仍可能泄漏。

### `reset()`和`release()`有什么区别？

`reset()`会关闭当前资源；`release()`只放弃所有权并返回文件描述符，不会关闭资源。

---

## 15. 对应的C++ Core Guidelines

建议重点复习：

- R.1：使用资源句柄和RAII自动管理资源；
- R.5：优先使用作用域对象；
- C.21：定义或删除一个拷贝、移动、析构操作时，应检查其余特殊成员函数；
- C.66：移动操作应当声明为`noexcept`；
- E.6：使用RAII避免资源泄漏；
- E.16：析构、释放和交换操作不能失败；
- CP.20：使用RAII管理锁，不直接配对调用`lock()`和`unlock()`。

参考：[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)

