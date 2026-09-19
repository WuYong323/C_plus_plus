# 07 · RAII 与智能指针（RAII & Smart Pointers）

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #RAII #智能指针 #unique_ptr #shared_ptr #weak_ptr #所有权 #资源管理
- 一句话结论：资源跟着对象走、对象死资源自动还；智能指针把裸指针的"谁负责释放"自动化并写进类型。
- 相关笔记：[[01_类与对象_封装构造析构]]、[[02_对象生命周期_拷贝移动与三五法则]]、[[08_移动语义与完美转发]]
- 前置要求：01 的构造/析构、02 的三/五/零法则、CUDA 的 `cudaMalloc/cudaFree`

---

## 精炼段

RAII（Resource Acquisition Is Initialization，资源获取即初始化）是 C++ 最核心的工程纪律：**在构造函数里获取资源、在析构函数里释放资源，让"资源生命周期 == 对象生命周期"**——于是任何 `return`、任何异常，编译器都自动帮你析构，资源永不泄漏。智能指针是 RAII 的现成实现：`unique_ptr` 独占所有权（可移动不可拷贝，可配自定义 deleter 管理 `cudaFree`/`fclose`）、`shared_ptr` 共享所有权（引用计数）、`weak_ptr` 非拥有（打破循环引用）。现代 C++ 铁律：**裸 `new`/`delete` 只在极少数场景出现，优先 `make_unique`/`make_shared`**。

---

## 0. 一句话直觉

> 你写 CUDA：`cudaMalloc(&p, n)` 之后，如果中间有个分支提前 `return` 忘了 `cudaFree(p)`，显存就漏了。RAII 的思路是：**别让"释放"成为一件要你记得的事，让它成为对象死亡时自动发生的事**——就像"人死了遗嘱自动执行"。智能指针就是把"谁该负责释放、什么时候释放"这件事，从注释和纪律，变成**编译器强制保证的类型系统**。

---

## 1. RAII：把 01 的雏形变成铁律

01 里 `Matrix` 的构造/析构就是 RAII。现在把它推广成原则：

> **一个类代表一份资源；构造 = 获取资源，析构 = 释放资源；类对象本身可以是栈上的，但它拥有的资源一定被正确管理。**

```cpp
// RAII 版 CUDA 缓冲区：cudaFree 永远会在对象死亡时执行，无论你怎么 return/throw
#include <memory>
#include <cstdio>

struct CudaDeleter {
    void operator()(float* p) const { /* cudaFree(p); */ std::printf("(cudaFree called)\n"); }
};
// 用 unique_ptr + 自定义 deleter 管 GPU 显存
// std::unique_ptr<float, CudaDeleter> buf(alloc(), CudaDeleter{});
```

RAII 的威力在于**异常安全**（`09`）：栈展开（unwinding）时，所有局部对象的析构函数**一定**被调用。所以哪怕中间抛异常，资源也照样归还——这是裸 `new`/`delete` 做不到的。

---

## 2. unique_ptr：独占所有权

**语义**：同一时刻**只有一个** `unique_ptr` 拥有这个对象。**不可拷贝、只能移动**（所有权转移，`08`）。

```cpp
#include <memory>

std::unique_ptr<int> p = std::make_unique<int>(42);   // 工厂函数（推荐，别直接 new）
// std::unique_ptr<int> q = p;                        // 错误：不可拷贝（所有权唯一）
std::unique_ptr<int> q = std::move(p);                // OK：所有权转移给 q
// 现在 p == nullptr，q 拥有那个 int

*p = 10;                 // 像指针一样解引用
q.reset();               // 立即释放，q 变空
q.reset(new int(7));     // 释放旧对象，接管新对象（少用，优先 make_unique）
auto raw = q.release();  // 放弃所有权、返回裸指针（危险，交给别人管）
```

**为什么用 `make_unique` 而不是 `new`**：① 更简洁；② 异常安全——`f(make_unique<A>(), make_unique<B>())` 若两个 `new` 交错求值时抛异常，裸 `new` 会泄漏，`make_unique` 不会。

**自定义 deleter**（管理非 `new` 的资源，对你做 infra 最关键）：
```cpp
// 管 FILE*：离开作用域自动 fclose
std::unique_ptr<std::FILE, decltype(&std::fclose)> fp(std::fopen("a.txt", "r"), &std::fclose);

// 管 cuda 显存：离开作用域自动 cudaFree
std::unique_ptr<float, CudaDeleter> gpu(cuda_malloc_float(n));
```

> **这是你在 HPC/infra 里最该用的一个**：任何"申请-释放成对"的资源（显存、文件、socket、锁），包一个 `unique_ptr` + 自定义 deleter，泄漏和"忘释放"从逻辑上消失。

---

## 3. shared_ptr：共享所有权（引用计数）

**语义**：多个 `shared_ptr` 可以**共同拥有**同一个对象，最后一个 `shared_ptr` 析构时才真正释放（引用计数归零）。

```cpp
std::shared_ptr<int> a = std::make_shared<int>(10);
std::shared_ptr<int> b = a;     // 拷贝：引用计数 1 → 2，a、b 指向同一对象
std::cout << a.use_count();     // 2
a.reset();                      // a 放弃，计数 2 → 1（对象还在，b 还拥有）
b.reset();                      // 计数 1 → 0 → 对象被释放
```

**底层：控制块（control block）**——对象旁边有一块额外内存，存**引用计数 + 弱计数 + deleter + 分配器**。所有指向同一对象的 `shared_ptr` 共享这个控制块。

```
┌─────────────────────────┐
│  控制块 (control block)    │
│  strong_count = 2         │
│  weak_count   = 0         │
│  deleter / allocator      │
└─────────────────────────┘
         ▲          ▲
         │          │
   shared_ptr a    shared_ptr b
         │          │
         ▼          ▼
      ┌───────────────┐
      │  被管理的对象    │
      └───────────────┘
```

**为什么用 `make_shared`**：它把"对象 + 控制块"合并成**一次分配**（省一次 malloc，缓存更友好），且异常安全。`shared_ptr<T>(new T)` 会分两次分配。

**代价（要清醒）**：引用计数是**原子操作**（线程安全），有开销；控制块额外占内存；循环引用会泄漏（见 §4）。**能 unique 就别 shared**——先问"所有权是否真的需要共享"。

---

## 4. weak_ptr：打破循环引用

`weak_ptr` **不拥有**对象（不影响引用计数），只"观察"。它解决 `shared_ptr` 循环引用导致的泄漏：

```cpp
struct Node {
    std::shared_ptr<Node> next;   // 若这里也用 shared_ptr，成环 → 永不释放
    std::weak_ptr<Node>   prev;   // 用 weak_ptr 打破环
};

auto a = std::make_shared<Node>();
auto b = std::make_shared<Node>();
a->next = b;       // b 的计数 +1
b->prev = a;       // weak_ptr 不加计数 → 不会成环
// a、b 出作用域后，引用计数都能归零，正确释放
```

使用时要先 `lock()` 把 `weak_ptr` 升级成 `shared_ptr`（若对象已被释放则得到空）：
```cpp
if (auto sp = a->prev.lock()) {   // 若 a 还活着，sp 是有效的 shared_ptr
    // 用 sp
}
```

> 典型场景：缓存（weak_ptr 观察缓存项，项被淘汰后 lock 失败）、观察者模式、父子互指。

---

## 5. 智能指针选型速查

| 需求 | 用 | 说明 |
|---|---|---|
| 独占资源，所有权唯一 | `unique_ptr` | 默认首选，零额外开销（就是一个指针） |
| 多个持有者共享，生命周期不定 | `shared_ptr` | 有原子计数开销 |
| 观察但不拥有、打破循环 | `weak_ptr` | 配合 shared_ptr |
| 非拥有、只访问 | **裸指针 / 引用** | 函数参数里"借"一下，不涉及所有权 |

**关键区分——所有权 vs 借用**：
```cpp
void use(const Node& n);                    // 借用（不拥有），用引用
void use(const Node* n);                    // 借用，可为空，用裸指针
void take(std::unique_ptr<Node> n);         // 接管所有权（拿走），按值传 unique_ptr
void share(std::shared_ptr<Node> n);        // 共享所有权，按值传 shared_ptr
```
> **裸指针在参数里出现是合法的**——它表示"借用、不拥有、不负责释放"。裸 `new`/`delete` 才是要消灭的（`delete` 交给智能指针做）。

---

## 6. 易错点

1. **循环引用**：两个 `shared_ptr` 互相持有 → 永不释放。用 `weak_ptr` 破环。
2. **滥用 shared_ptr**：不需要共享所有权的场景用了 shared_ptr，白付原子计数开销。**先 unique，再 shared。**
3. **`get()` 出来的裸指针悬空**：`sp.get()` 后 `sp` 被 reset，裸指针变悬空。别长期持有 `get()` 的返回值。
4. **用裸 `new` 直接构造 shared_ptr**：`shared_ptr<T>(new T)` 两次分配；且若在同一表达式里再混裸 new 可能异常泄漏。用 `make_shared`。
5. **unique_ptr 的数组**：`make_unique<int[]>(n)` 支持数组（有 `[]` 版本），但别用它替代 `vector`。
6. **手写 deleter 忘了 `noexcept`**：自定义 deleter 抛异常会导致未定义行为——deleter 不该抛异常。
7. **在容器里存 unique_ptr**：`vector<unique_ptr<T>>` 是合法的（移动语义），常见于多态对象集合（见 module2 demo 的 `vector<unique_ptr<Shape>>`）。

---

## 7. 自测题

1. **概念题**：RAII 的核心思想是什么？为什么它天然异常安全（对比裸 `new`/`delete`）？
2. **应用题**：写一个用 `unique_ptr` + 自定义 deleter 管理 GPU 显存（`cudaMalloc`/`cudaFree`）的 `CudaTensor` 类，并说明为什么这样写之后，任何提前 `return` 或抛异常都不会泄漏显存。
3. **思考题**：`shared_ptr` 的引用计数为什么用**原子操作**？如果程序是单线程，这个原子开销可以省吗？这揭示了 `shared_ptr` 设计上的什么取舍（通用安全 vs 单线程性能）？

---

## 来源与延伸

- *C++ Primer* 第 5 版：第 12 章（动态内存与智能指针）
- cppreference：[std::unique_ptr](https://en.cppreference.com/w/cpp/memory/unique_ptr)、[std::shared_ptr](https://en.cppreference.com/w/cpp/memory/shared_ptr)、[std::weak_ptr](https://en.cppreference.com/w/cpp/memory/weak_ptr)、[RAII](https://en.cppreference.com/w/cpp/language/raii)
- C++ Core Guidelines：[R.11 避免裸 new/delete](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r11-avoid-calling-new-and-delete-explicitly)、[R.20 用 unique_ptr 表达独占所有权](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r20-use-unique_ptr-or-shared_ptr-to-represent-ownership)
- 待深挖：移动语义如何让 unique_ptr 传递零开销（`08`）、异常安全与栈展开（`09`）
