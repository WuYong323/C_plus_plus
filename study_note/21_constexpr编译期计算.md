# 21 · constexpr 编译期计算（Compile-time Computation）

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #constexpr #consteval #编译期计算 #元编程 #constinit
- 一句话结论：constexpr 让普通函数也能在编译期求值，把"运行时算"变成"编译期算好"，零运行时开销。
- 相关笔记：[[10_模板基础与泛型编程]]、[[11_模板进阶_特化与concepts]]、[[14_内存模型与性能工程]]
- 前置要求：10/11 的模板、函数、`static const`

---

## 精炼段

`constexpr` 是"**这个值/函数可以在编译期就算出来**"的标记：`constexpr` 变量是编译期常量，`constexpr` 函数在参数全是常量时由编译器**在编译期执行**、把结果直接写进二进制（运行时零开销）。它一路放宽：C++14 允许循环/多语句，C++20 允许 constexpr 里动态分配（`vector`/`string`），并新增 **`consteval`**（**必须**编译期求值）和 **`constinit`**（静态变量**必须**编译期初始化，根治 static 初始化顺序问题）。相比模板元编程（`11`），`constexpr` 用**普通函数语法**做编译期计算，可读性碾压——它是现代 C++ 编译期编程的**首选**。

---

## 0. 一句话直觉

> 运行时算 `fib(20)` 是"程序跑起来才一层层算"；编译期算 `fib(20)` 是"编译器替你算好，直接把 `6765` 这个数字焊死在二进制里"。后者运行时**啥都不算**，直接读常量——就像考试前把答案抄在草稿纸上，考场直接用。`constexpr` 就是让编译器"提前帮你把能算的算好"。

---

## 1. constexpr 变量 vs constexpr 函数

```cpp
constexpr int N = 100;                    // ① 编译期常量（和 const 的区别见下）
int arr[N];                               // 能当数组大小（必须编译期已知）

constexpr int square(int x) {             // ② 编译期可求值的函数
    return x * x;
}
constexpr int s = square(10);             // 编译期算出 100，s 是常量
int arr2[square(5)];                      // 25：编译期求值，能当数组大小
```

**`constexpr` vs `const`**：
- `const` = "运行期不可改"，但值**可能**到运行时才知道（如 `const int x = read_input();`）。
- `constexpr` = "**编译期就能确定**"，是更强的 `const`。**`constexpr` 一定是 `const`，反过来不一定。**

**constexpr 函数既是编译期函数、也是普通函数**：
```cpp
int x = read_input();          // 运行期值
int y = square(x);             // square 此时当普通函数用（运行期调用）
constexpr int z = square(4);   // 此时当编译期函数用（编译期求值）
// 同一份代码，两种用法，取决于实参是否编译期已知
```

---

## 2. constexpr 的演进（能力一路放宽）

| 版本 | constexpr 函数能做什么 |
|---|---|
| C++11 | 只能一条 `return` 语句（`return x*x;`） |
| C++14 | 循环、局部变量、多语句、`if`/`switch` |
| C++17 | `if constexpr`（编译期分支，`10`/demo4 用过） |
| C++20 | **动态分配**（`new`/`vector`/`string` 在编译期可用）、`virtual` |

```cpp
// C++14+：循环也行
constexpr int factorial(int n) {
    int r = 1;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}
constexpr int f = factorial(10);   // 编译期算出 3628800

// C++20：编译期甚至能建 vector 并排序
constexpr int max_of_list() {
    std::vector<int> v{5, 1, 4, 2, 3};
    std::ranges::sort(v);          // C++20：编译期跑 STL 算法！
    return v.back();
}
constexpr int m = max_of_list();   // 编译期算出 5
```

> **C++20 是关键跨越**：编译期能跑 `vector`/`string`/STL 算法，意味着你可以在编译期做"读配置→建表→排序→生成常量"的完整流程，且**写法就是普通运行期代码**。

---

## 3. consteval 与 constinit（C++20）

```cpp
consteval int square2(int x) { return x * x; }   // ① 只能编译期调用，运行期调用报错
// int y = square2(read_input());                 // 错误：consteval 不接受运行期参数

constinit int g = 42;                            // ② 静态变量必须编译期初始化
// 根治 "static initialization order fiasco"：静态变量初始化顺序不确定的经典坑
```
- **`consteval`**：**必须**编译期求值（比 `constexpr` 更强，`constexpr` 是"可以"、`consteval` 是"只能"）。用于"只能编译期算"的元函数。
- **`constinit`**：静态/线程局部变量**必须**在编译期初始化，避免"两个全局变量的初始化顺序不确定"（static init order fiasco）。

---

## 4. 编译期计算 vs 模板元编程（`11`）

| | 模板元编程（`11`） | constexpr（本篇） |
|---|---|---|
| 语法 | 模板特化/递归/SFINAE | **普通函数**语法 |
| 可读性 | 差（类型递归，天书） | **好**（就是正常 C++） |
| 时代 | 老（C++98 就靠它） | 新（C++14+ 主力） |
| 用途 | 类型计算（`11` 的 `is_pointer`） | 值计算（常量、查表、配置） |

```cpp
// 模板元编程算 fib（难读）：
template<int N> struct Fib { static const int v = Fib<N-1>::v + Fib<N-2>::v; };
template<> struct Fib<0> { static const int v = 0; };
template<> struct Fib<1> { static const int v = 1; };
// Fib<20>::v  → 6765

// constexpr 算 fib（好读）：
constexpr int fib(int n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }
// fib(20) → 6765
```
> **结论**：**算"值"用 constexpr（可读），算"类型"用模板（`11`）**。现代 C++ 尽量用 constexpr 替代老式模板元编程。

---

## 5. HPC / infra 里的实战价值

1. **编译期查表（lookup table）**：三角函数、量化表、CRC 表——编译期算好存成 `constexpr` 数组，运行时零开销查表。
2. **编译期维度/配置**：`constexpr int TILE = 128;`、`constexpr int BLOCK_DIM = 256;`——把 HPC 的 tile 大小、维度做成编译期常量，编译器据此做更多优化（循环展开、向量化）。
3. **编译期校验**：`static_assert` + `constexpr` 在编译期就断言"配置合法"（如 `static_assert(TILE % 32 == 0)`），把 bug 拦在编译期。
4. **生成专用代码**：`constexpr` + 模板，让编译器为每个配置生成零开销的专用版本（结合 `14` 的"模板优于虚函数"）。

```cpp
template<int BLOCK>                     // BLOCK 是编译期常量
constexpr int grid_size(int n) { return (n + BLOCK - 1) / BLOCK; }   // 编译期算网格数
// static_assert(grid_size<256>(1000) == 4);   // 编译期校验
```

---

## 6. 易错点

1. **`constexpr` 函数不一定在编译期执行**：实参不是编译期常量时，它就退化成普通运行期函数。要**强制**编译期，用 `consteval` 或把结果赋给 `constexpr` 变量。
2. **`constexpr` 变量必须能编译期求值**：`constexpr int x = read_input();` 编译错误（`read_input` 不是编译期可求值）。
3. **const 和 constexpr 混用**：`const` 不保证编译期已知，`constexpr` 才保证。数组大小、模板非类型参数要 `constexpr`。
4. **编译期太慢**：大量 constexpr 计算会增加编译时间（编译器在"跑"你的代码）。极端元编程会拖慢编译，要权衡。
5. **C++ 版本**：编译期 `vector`/STL 算法要 C++20；老项目 C++17 用不了（`16` 已提醒先确认 `-std`）。

---

## 7. 自测题

1. **概念题**：`const` 和 `constexpr` 的区别是什么？为什么数组大小/模板非类型参数必须用 `constexpr` 而非 `const`？
2. **应用题**：写一个 `constexpr` 函数生成编译期平方表 `constexpr std::array<int, 10> squares`（`i²`），并在 `static_assert` 里校验 `squares[5] == 25`。用 C++20 的话，试试编译期 `vector` + `sort`。
3. **思考题**：模板元编程和 constexpr 都能做编译期计算，为什么现代 C++ 更推荐 constexpr 算"值"？在什么场景下模板（类型计算）仍然不可替代？

---

## 来源与延伸

- cppreference：[constexpr](https://en.cppreference.com/w/cpp/language/constexpr)、[consteval](https://en.cppreference.com/w/cpp/language/consteval)、[constinit](https://en.cppreference.com/w/cpp/language/constinit)
- *C++ Primer* 第 5 版：第 2.4.4 节（constexpr 引入）+ 第 6 章（constexpr 函数）
- C++ Core Guidelines：[F.4 编译期能算就别留到运行期](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#f4-if-a-function-might-have-to-be-evaluated-at-compile-time-declare-it-constexpr)
- 待深挖：`if constexpr` 与模板的结合（`10`）、`std::simd`（`16`）与编译期向量化的关系
