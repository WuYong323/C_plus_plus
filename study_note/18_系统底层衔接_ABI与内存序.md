# 18 · 系统底层衔接：ABI、调用约定与内存序（ABI, Calling Convention & Memory Order）★亮点

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #ABI #调用约定 #名字修饰 #内存模型 #atomic #内存序 #系统底层
- 一句话结论：C++ 的二进制世界由 ABI 规定——名字修饰、调用约定、对象布局；并发世界由内存模型规定——atomic 与内存序，data race 即未定义行为。
- 相关笔记：[[06_对象模型_虚表与内存布局]]、[[13_多文件工程与构建_CMake]]、[[14_内存模型与性能工程]]
- 前置要求：06 对象布局、13 链接、你的 `08_运行时组织`（调用约定/栈帧）

---

## 精炼段

C++ 源码要变成"能互相链接、能正确并发"的二进制，靠两层底层约定。**ABI（Application Binary Interface）**规定二进制层面的契约：**名字修饰**（mangling，把 `Foo::bar(int)` 编码成唯一符号）、**调用约定**（参数进哪些寄存器、谁负责保存）、**对象/虚表布局**（`06` 的内容）。ABI 不兼容 = 无法链接/运行崩溃（MinGW 和 MSVC 就是两套不兼容的 ABI）。**内存模型（memory model）**规定多线程的可见性：`std::atomic` + **内存序**（relaxed/acquire/release/seq_cst）建立 happens-before，而**数据竞争（data race）是未定义行为**——不是"结果可能错"，是"程序行为无意义"。这两层是你从"应用层 C++"走向"系统/编译器/HPC 底层"的最后一块拼图。

---

## 0. 一句话直觉

> 源码是"给人看的合同"，ABI 是"给机器看的合同"——同一个函数，MinGW 编译出来的符号叫 `_ZN3Foo3barEi`，MSVC 叫 `?bar@Foo@@...`，两边"说的不是同一种语言"，所以链接不上。内存模型则是"多线程世界的交通规则"：没有规则，两个线程同时读写一个变量就是"车祸"（未定义行为）；`atomic` + 内存序就是"红绿灯"，规定了"谁先谁后、谁看得见谁"。

---

## 1. ABI：二进制层面的契约

**ABI** 是"编译产物之间怎么对话"的完整约定，包括：

| 组成部分 | 说明 | 你哪篇学过 |
|---|---|---|
| 名字修饰（mangling） | 把重载/命名空间/类编码进符号名 | 本节 §2 |
| 调用约定 | 参数/返回值怎么传、谁保存寄存器 | 本节 §3、你的 `08_运行时组织` |
| 对象布局 | 成员偏移、对齐、padding | `06`/`14` |
| 虚表结构 | vtable 布局、RTTI 位置 | `06` |
| 异常处理 | 展开表、异常传播 | `09` |

**改 ABI = 破坏兼容**：改了类布局（加成员）、改虚函数表、换编译器（MinGW↔MSVC）——所有依赖它的已编译产物都要**重编**，否则运行崩溃（`15` §7 已提）。

---

## 2. 名字修饰（Name Mangling）

C++ 支持重载（同名函数不同参数）、命名空间、成员函数，符号名必须把这些都编码进去：

```cpp
namespace ns {
    class Foo {
    public:
        void bar(int);
        void bar(double);   // 重载：和上面那个是不同的符号
    };
}
```
编译后（Itanium ABI，g++/clang 用）：
```
_ZN2ns3Foo3barEi      ← ns::Foo::bar(int)
_ZN2ns3Foo3barEd      ← ns::Foo::bar(double)
```
MSVC 用另一套（`?bar@Foo@ns@@QEAAXH@Z`）。**两套不兼容**——这就是为什么 MinGW 编译的 `.dll` 不能直接给 MSVC 程序用（`13` 说的"链接错误 undefined reference"在 ABI 层面的根因）。

> **对你的意义**：你的 Cool 编译器是"直接生成 LLVM IR 文本"，符号名你**自己控制**（`@List.isNil` 这种），所以不存在 mangling 问题。但当你读 LLVM/Clang 源码、或调试"为什么链接不上"时，mangling 就是第一现场。

---

## 3. 调用约定（Calling Convention）

函数调用双方要约定：**参数放哪些寄存器、返回值放哪、谁负责保存寄存器**。你在 `08_运行时组织` §3 已经学过（caller-saved vs callee-saved）。关键：**不同平台/编译器约定不同**：

| 平台 | 约定 | 前 4 个整数参数寄存器 |
|---|---|---|
| Linux x86-64（g++/clang） | **System V AMD64** | RDI, RSI, RDX, RCX |
| Windows x86-64（MSVC **和 MinGW**） | **Microsoft x64** | RCX, RDX, R8, R9 |

> **重要（你的环境）**：你用 MinGW（g++）在 Windows 上编译，**名字修饰是 Itanium 风格（像 Linux），但调用约定是 Microsoft x64（因为 Windows x64 只有这一种约定）**。所以"MinGW 是 Linux 的 g++ 搬到 Windows"是个常见误解——它混合了两套 ABI 的部件。

**`this` 怎么传**：C++ 成员函数调用时，`this` 作为隐藏第一参数，在 Microsoft x64 里走 `RCX`（在 System V 里走 `RDI`）——这和你 Cool 里"传 self 为第一实参"（`call fn(i8* %l)`）**一模一样**，只是寄存器名不同。

---

## 4. 内存模型与 std::atomic

**C++ 内存模型**规定多线程下"读写何时对其他线程可见"。**核心规则：数据竞争（data race）是未定义行为**——两个线程访问同一内存，至少一个写，且无同步 → 程序行为无意义（不是"可能得到错值"，是"什么都可能发生"）。

**解法：`std::atomic<T>`**——原子类型，读写是原子的、不会被"读一半"，且可以配**内存序**控制可见性：

```cpp
#include <atomic>
#include <thread>

std::atomic<int> counter{0};

void inc() { for (int i = 0; i < 1'000'000; ++i) counter.fetch_add(1); }
// 两个线程各加 100 万次 → 结果一定是 200 万（原子，无数据竞争）
```

### 4.1 内存序（memory_order）——从强到弱

| 内存序 | 保证 | 用在哪 |
|---|---|---|
| `seq_cst`（默认） | **顺序一致性**：全局一致的总序，最易推理 | 默认、不确定时用它 |
| `acquire`/`release` | 成对的"获取/释放"：release 前写的内容，acquire 后都可见 | 生产者-消费者、锁、无锁队列 |
| `relaxed` | 只保证原子性，不保证顺序 | 纯计数器、不依赖顺序的场景 |

```cpp
// 生产者-消费者（acquire/release 的经典场景）
std::atomic<bool> ready{false};
int data = 0;

// 线程 A（生产者）
data = 42;
ready.store(true, std::memory_order_release);   // release：发布 data

// 线程 B（消费者）
while (!ready.load(std::memory_order_acquire)) {}   // acquire：获取
// 到这里，data == 42 一定可见（release 前的写，acquire 后一定可见）
```

**为什么不是"默认全 seq_cst 就完事"**：内存序越强，CPU 能做的重排越少，性能越低。`relaxed`/`acquire-release` 在保证正确的前提下**放开重排**，性能更好——但**难推理、易错**。**工程铁律：先用 `seq_cst`（默认）写对，profile 证明是瓶颈再降级。**

---

## 5. volatile ≠ atomic（经典误解）

```cpp
volatile int x;      // "每次访问都真实读写内存，别优化掉" —— 用于 MMIO、信号处理
std::atomic<int> y;  // "多线程安全" —— 原子性 + 内存序
```
- `volatile` **不提供原子性、不提供可见性/顺序保证**，**不能用于多线程同步**。
- `atomic` 才管并发。二者用途完全不同，别混用（这是 C++ 最经典的误区之一）。
- `volatile` 的正确场景：访问**内存映射 IO 寄存器**（硬件寄存器值会自己变）、信号处理函数里共享的 `sig_atomic_t`。

---

## 6. 与 OS / 硬件 / 你的 CUDA 的衔接

- **缓存一致性**：`atomic` 的可见性最终靠 CPU 的缓存一致性协议（MESI）实现——这和你 `14` 篇的 false sharing 是**同一套硬件机制**（同缓存行互相失效）。
- **对齐与页**：`alignas(64)` 对齐到缓存行（`14`）；大 buffer 用页对齐（`mmap`/`posix_memalign`）减少 TLB 缺失。
- **系统调用**：`read`/`write`/`mmap` 是"用户态进内核"的边界，`std::atomic` 的锁（`lock` 前缀指令）与 syscall 的开销量级完全不同（原子操作 ~几十 ns，syscall ~数百 ns~μs）。
- **你的 CUDA 经验**：GPU 的 `__syncthreads()`、`atomicAdd`、memory fence（`__threadfence()`），和 CPU 的 `std::atomic` + `memory_order` 是**同一个概念的两套语法**——都是"多执行单元访问共享数据的顺序与可见性约定"。你在 CUDA 里学到的"数据竞争 = 不确定行为"，在 C++ 里就是"data race = UB"。

---

## 7. 图示：一次带 this 的函数调用（ABI 视角）

```
调用方 main                       被调方 Foo::bar(this, x)
─────────────                     ─────────────────────
mov rcx, &foo    ; this 进 RCX
mov rdx, 5       ; 参数 x 进 RDX   (Microsoft x64 约定)
call ?bar@Foo@@...                Foo::bar 内部：
                                  ;   this 在 RCX，参数在 RDX
                                  ;   按 this + 偏移 访问成员
                                  ret   ; 返回值进 RAX
```
> 你 Cool 的 `call fn(i8* %l)` 传 self，C++ 的 `mov rcx, &foo` 传 this——**同一件事，只是 Cool 是 LLVM IR 文本、C++ 是真实寄存器**。

---

## 8. 易错点

1. **volatile 当 atomic 用**：多线程用 `volatile` 同步 → 未定义行为。并发用 `std::atomic`。
2. **data race 不自知**：两个线程读写同一个普通变量没加同步 → UB（sanitizer 的 TSan 能抓）。
3. **内存序乱用**：上来就 `relaxed` 追求性能 → 顺序错误难查。**先 seq_cst 写对，再降级。**
4. **跨 ABI 混链接**：MinGW 和 MSVC 的 `.o`/`.lib` 不能混链接（mangling/异常/STL 都不同）。
5. **改类布局不重编**：加了成员字段，依赖的旧 `.o` 还用旧偏移 → 运行崩溃（ABI 破坏）。
6. **`atomic` 也有代价**：原子操作有缓存一致性开销（比普通读写慢），不要"所有变量都 atomic"。

---

## 9. 自测题

1. **概念题**：ABI 包含哪些部分？为什么 MinGW 和 MSVC 编译的库不能互相链接（至少说两个原因）？
2. **应用题**：写一个"生产者写 data、置 ready；消费者等 ready、读 data"的无锁同步（用 `acquire/release`），解释为什么 acquire 之后 data 一定可见。若把内存序换成 `relaxed`，会出什么问题？
3. **思考题**：`volatile` 和 `std::atomic` 的区别是什么？为什么"多线程共享的标志位"必须用 atomic 而不是 volatile？联系你 CUDA 里 `__threadfence` 和普通 store 的区别，它们是不是同源的？

---

## 来源与延伸

- cppreference：[ABI](https://en.cppreference.com/w/cpp/language/)、[std::atomic](https://en.cppreference.com/w/cpp/atomic/atomic)、[memory_order](https://en.cppreference.com/w/cpp/atomic/memory_order)、[volatile](https://en.cppreference.com/w/cpp/language/cv)
- Itanium C++ ABI：[规范](https://itanium-cxx-abi.github.io/cxx-abi/abi.html)、Microsoft x64：[调用约定](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention)
- cppreference：[C++ 内存模型](https://en.cppreference.com/w/cpp/language/memory_model)
- 待深挖：无锁数据结构（acquire/release 实战）、[GCC ABI 兼容性研究](https://ieeexplore.ieee.org/document/11503977)
