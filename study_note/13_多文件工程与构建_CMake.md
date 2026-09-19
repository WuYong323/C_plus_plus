# 13 · 多文件工程与构建：头文件、链接与 CMake（Multi-file & CMake）

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #工程 #多文件 #头文件 #链接 #ODR #CMake #构建系统
- 一句话结论：C++ 工程靠「声明放头文件、定义放源文件」分离接口与实现，编译单元独立编译后由链接器合并，CMake 管构建。
- 相关笔记：[[10_模板基础与泛型编程]]、[[15_现代Cpp工程实践]]、[[18_系统底层衔接_ABI与内存序]]
- 前置要求：竞赛的"单文件提交"经验；知道 `g++ main.cpp -o main`

---

## 精炼段

竞赛里你习惯单文件、一键编译。工业工程是**多文件**：接口（函数/类**声明**）放**头文件 `.h`**，实现（**定义**）放**源文件 `.cpp`**，每个 `.cpp` 先**独立编译**成目标文件 `.o`，再由**链接器**把它们合并成可执行文件。这背后是三条铁律：**ODR（一个实体全程序只能有一份定义）**、**头文件只放声明不放定义**（否则多文件 include 后重复定义）、**头文件要有 include guard**。CMake 是事实标准的构建系统：用 `CMakeLists.txt` 描述"目标 + 依赖"，生成实际构建脚本（Ninja/Make/MSBuild），配合 presets 管配置、vcpkg/Conan 管第三方依赖。**理解"编译 vs 链接"是读懂一切报错的钥匙。**

---

## 0. 一句话直觉

> 单文件是"一页纸的便条"，多文件是"一本书的章节"：每个 `.cpp` 是一章，各自能独立成篇（独立编译），靠一份"目录"（头文件）声明"这一章提供了哪些函数/类"，最后装订（链接）成整本书。分开写的好处：**改一章只重编那一章**（不用重编全书）、**团队并行写不同章节**、**接口与实现隔离**。

---

## 1. 声明 vs 定义（第一块基石）

```cpp
// 声明（declaration）：告诉编译器"有这么个东西"，不占内存/不生成代码
int add(int a, int b);          // 函数声明（只有签名，没有函数体）
class Matrix;                   // 类的前置声明
extern int global_counter;      // 变量声明（extern，不分配）

// 定义（definition）：真正"落地"，占内存/生成代码
int add(int a, int b) { return a + b; }   // 函数定义（有函数体）
class Matrix { /* ... */ };               // 类定义
int global_counter = 0;                   // 变量定义（分配内存）
```

**ODR（One Definition Rule，单一定义规则）**：一个函数、变量、类，**全程序只能有一份定义**（可以有多份声明）。违反它 → "multiple definition" 链接错误。例外：`inline` 函数、模板（`10` 说的"定义放头文件"就靠这个例外）。

---

## 2. 头文件 vs 源文件（标准分工）

```
matrix.h（接口：声明）              matrix.cpp（实现：定义）
─────────────────────              ─────────────────────
#pragma once                       #include "matrix.h"
class Matrix {                     Matrix::Matrix(int r, int c)
public:                              : rows_(r), cols_(c),
    Matrix(int r, int c);              data_(new float[r*c]()) {}
    float& at(int i, int j);        float& Matrix::at(int i, int j) {
    ~Matrix();                        return data_[i*cols_+j];
private:                            }
    int rows_, cols_;               Matrix::~Matrix() { delete[] data_; }
    float* data_;
};
```

- **头文件**：类/函数**声明**、`inline` 函数、模板（定义）、`constexpr` 常量、宏。**被多个 `.cpp` include**。
- **源文件**：函数/类的**定义**（实现细节）。**只编译一次**。
- **为什么头文件只放声明**：头文件被多个 `.cpp` include，若里面有定义（如 `int add(...){...}`），每个 `.cpp` 都生成一份 → 链接时"multiple definition"。声明可以重复，定义不行。

### 2.1 include guard（防止同一头文件被重复包含）

```cpp
#ifndef MATRIX_H        // 若没定义过 MATRIX_H
#define MATRIX_H        // 定义它
// ... 头文件内容 ...
#endif                  // 结束
// 或一句：#pragma once（更简洁，几乎所有编译器都支持）
```
没有 guard，若 `a.h` 和 `b.h` 都 include 了 `c.h`，`c.h` 会被包含两次 → 类重复定义报错。

---

## 3. 编译 vs 链接（读懂报错的钥匙）

```
matrix.cpp ──预处理──► ──编译──► matrix.o ┐
main.cpp   ──预处理──► ──编译──► main.o   ├──链接──► app.exe
                                           │
              （每个 .cpp 独立成一个"翻译单元"）
```

```bat
g++ -c matrix.cpp -o matrix.o     :: ① 编译（-c：只编译不链接）
g++ -c main.cpp   -o main.o       :: ② 编译
g++ main.o matrix.o -o app.exe    :: ③ 链接（把 .o 合并成可执行文件）
```

| 报错 | 含义 | 常见原因 |
|---|---|---|
| `undefined reference to 'Matrix::at(...)'` | **链接**错误：声明了但找不到定义 | 忘了编译那个 `.cpp`；`-c` 漏链接；模板定义放 .cpp |
| `multiple definition of 'x'` | **链接**错误：定义了两份 | 头文件里写了非 inline 函数定义 |
| `error: 'Matrix' was not declared` | **编译**错误：没看到声明 | 忘了 `#include "matrix.h"` |

> **核心技能**：看到报错先判断是"编译期"还是"链接期"——前者是语法/声明问题，后者是定义/符号问题。这个判断能让你少走 90% 的弯路。

---

## 4. CMake：事实标准的构建系统

手写 `g++` 命令在几十个文件、跨平台时就崩溃了。CMake 用**声明式**的 `CMakeLists.txt` 描述工程，再生成平台相关的构建脚本：

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(matrix_app CXX)                     # 工程名 + 语言

add_executable(app main.cpp matrix.cpp)     # 目标：可执行文件

# 现代写法：target 制（只给目标加属性，不污染全局）
target_compile_features(app PRIVATE cxx_std_17)      # 指定 C++17
target_compile_options(app PRIVATE -Wall -Wextra)    # 警告
target_include_directories(app PRIVATE include/)     # 头文件搜索路径
```

```bat
:: 三步构建（presets 可以简化，见下）
cmake -S . -B build            :: ① 配置：生成构建脚本（-S 源码目录，-B 构建目录）
cmake --build build            :: ② 构建：实际编译+链接
:: 可执行文件在 build/ 下
```

**关键概念**：
- **target（目标）**：一个可执行文件或库。现代 CMake 强调"**target 制**"——用 `target_*` 给目标加属性（include 路径、编译选项、链接库），而不是老式的全局 `include_directories`（会污染所有目标）。
- **add_library**：把公共代码打成库（静态 `.a` / 动态 `.dll`），再 `target_link_libraries(app PRIVATE mylib)`。
- **PRIVATE / PUBLIC / INTERFACE**：传播关系——`PUBLIC` 会"传染"给依赖者（如库的头文件路径要 PUBLIC 传给使用方）。

### 4.1 CMake Presets（现代工程标配）

`CMakePresets.json` 把"配置选项"（编译器、构建类型、生成器）写成 JSON，团队成员一条命令就能复现同一配置，CI 也用它：

```json
{ "version": 3,
  "configurePresets": [
    { "name": "default",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" } } ] }
```

### 4.2 第三方依赖：vcpkg / Conan

| 工具 | 定位 |
|---|---|
| **vcpkg**（微软） | 源码级包管理，`vcpkg install fmt` + CMake `find_package`，Windows 生态友好 |
| **Conan** | 预编译二进制包，跨平台，适合大型团队/私有制品库 |
| **FetchContent** | CMake 内置，直接拉源码子模块构建（轻量） |

> 参考：[现代 C++ 工具链：CMake + Conan + vcpkg 依赖管理](https://cloud.tencent.cn/developer/article/2561253)

---

## 5. 与竞赛单文件的差别（心态转变）

| 竞赛 | 工业 |
|---|---|
| 单文件、提交即忘 | 多文件、长期演进、多人协作 |
| 复制粘贴板子 | 提取公共代码成**库**（`add_library`） |
| `#include <bits/stdc++.h>`（全包含，慢） | 精确 include 需要的头文件（编译快） |
| 不关心编译时间 | **增量编译**：只重编改动的文件 |
| `#define` 宏、全局变量 | 头文件声明 + 命名空间，控制可见性 |

---

## 6. 易错点

1. **头文件里写函数定义（非 inline/模板）** → multiple definition。定义放 `.cpp`，或标 `inline`。
2. **忘了 include guard** → 重复包含报错。
3. **`undefined reference` 就狂改代码** → 先检查是不是"那个 `.cpp` 没加进编译/链接"。
4. **`#include "x.h"` vs `<x.h>`**：双引号先搜当前目录（你的头文件），尖括号搜系统/库路径（标准库、第三方）。
5. **循环 include**：`a.h` include `b.h`，`b.h` include `a.h`。用前置声明（`class B;`）打破，只在需要完整类型时再 include。
6. **模板定义放 .cpp**（`10` 的坑）→ undefined reference。

---

## 7. 自测题

1. **概念题**：声明和定义的区别是什么？ODR（单一定义规则）为什么决定了"头文件只放声明"？
2. **应用题**：把一个 `Matrix` 类拆成 `matrix.h` + `matrix.cpp` + `main.cpp` 三文件，写一个 `CMakeLists.txt` 把它们构建成可执行文件（用 target 制 + `cxx_std_17`），并跑通。
3. **思考题**：`undefined reference` 和 `multiple definition` 分别是编译期还是链接期错误？各自最常见的原因是什么？为什么模板是个例外（定义可以/必须在头文件）？

---

## 来源与延伸

- *C++ Primer* 第 5 版：第 2.6 节（定义与声明）、第 6.1.1 节（分离编译）
- cppreference：[One Definition Rule](https://en.cppreference.com/w/cpp/language/definition)、[Phases of translation](https://en.cppreference.com/w/cpp/language/translation_phases)
- CMake 官方：[target-based 教程](https://cmake.org/cmake/help/latest/guide/tutorial/)、[CMakePresets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- 待深挖：ABI 与链接的底层（`18`）、测试/CI 接入构建（`15`）
