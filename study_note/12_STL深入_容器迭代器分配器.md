# 12 · STL 深入：容器、迭代器、算法与分配器（STL in Depth）

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #STL #容器 #迭代器 #算法 #分配器 #泛型设计
- 一句话结论：STL 用「容器 + 迭代器 + 算法 + 分配器」解耦，靠泛型把 N×M 的组合降到 N+M 份代码。
- 相关笔记：[[10_模板基础与泛型编程]]、[[11_模板进阶_特化与concepts]]、[[14_内存模型与性能工程]]
- 前置要求：竞赛中大量使用 `vector`/`sort`/`map`/`priority_queue` 的**使用经验**

---

## 精炼段

STL（标准模板库）是 C++ 泛型编程的巅峰作品，它的核心设计是**四个正交的部件**：**容器**（存数据：`vector`/`map`/`unordered_map`…）、**迭代器**（统一访问方式：像指针一样的"游标"）、**算法**（对范围操作：`sort`/`find`/`lower_bound`…）、**分配器**（内存从哪来）。它们通过**迭代器**这个"接口"解耦：N 个容器 × M 个算法，只需写 N+M 份代码（而不是 N×M），因为算法只认迭代器、不认具体容器。你竞赛里用了 STL 的"表面"，本篇讲它**背后的设计原理**——这决定了你工业里"选哪个容器、为什么、以及如何用分配器优化内存"。

---

## 0. 一句话直觉

> 竞赛里你已经体会到：`sort` 既能排 `vector` 也能排数组，`lower_bound` 对 `set` 和 `vector` 都能用。为什么同一套算法能用在这么多容器上？因为 STL 让所有容器都"说同一种语言"——**迭代器**（一个能 `++`、`*`、`==` 的"游标"）。算法只跟迭代器对话，不关心背后是数组还是红黑树。这就像 USB 接口：键盘、鼠标、U 盘各是各的"容器"，但都插同一个"迭代器"USB 口，电脑（算法）就能统一驱动。

---

## 1. 四部件全景

```
┌──────────┐    通过迭代器     ┌──────────┐
│   算法    │ ◄──────────────► │   容器    │
│ sort/find │     (iterator)   │ vector/  │
│ lower_bound│                  │ map/...  │
└──────────┘                   └────┬─────┘
                                   │ 用分配器分配内存
                              ┌────▼─────┐
                              │  分配器    │
                              │ allocator │
                              └──────────┘
```

---

## 2. 容器：怎么选（这是你工业里最常用的决策）

### 2.1 三大类

| 类别 | 容器 | 底层结构 | 特点 |
|---|---|---|---|
| **序列** | `vector` | 动态数组 | 随机访问 O(1)、尾部增删快、**缓存最友好**（连续内存） |
| | `deque` | 分段数组 | 头尾增删都快 |
| | `list` | 双向链表 | 任意位置增删 O(1)、**缓存差**、几乎总被 vector 替代 |
| | `array` | 定长数组 | 编译期定长、栈上、零堆分配 |
| **关联（有序）** | `set`/`map` | 红黑树 | 按键有序、O(log n) |
| **无序** | `unordered_set`/`unordered_map` | 哈希表 | 平均 O(1)、无序 |
| **适配器** | `stack`/`queue`/`priority_queue` | 包装上述容器 | 限定接口 |

### 2.2 选型铁律（HPC 尤其重要）

1. **默认 `vector`**：连续内存 = 缓存命中率高。你的 CUDA/infra 经验告诉你，**缓存友好 > 一切**。
2. **`list` 几乎总是错的**：链表节点离散分布，每次访问都是 cache miss；`list` 的"O(1) 插入"输给 `vector` 的"缓存友好 + 预取"。**别用 list，除非你测过确实更快。**
3. **需要"按键查找"**：先问"要顺序吗？"——要顺序用 `map`（红黑树），不要顺序用 `unordered_map`（哈希，通常更快）。
4. **`priority_queue` 就是堆**：你竞赛里手写的堆，工业里用它或 `<algorithm>` 的 `make_heap`。

### 2.3 与竞赛用法的差别

竞赛里你只关心"能不能过题"；工业里还要关心：

- **异常安全**（`09`）：`push_back` 失败时容器状态如何。
- **迭代器失效**：`vector` 扩容后，之前拿的迭代器/指针**全部失效**（指向旧内存）——这是最经典的生产 bug。
- **内存策略**：`reserve` 预分配（避免反复扩容拷贝）、`shrink_to_fit` 释放多余内存。

---

## 3. 迭代器：统一接口（五类）

迭代器是"泛化的指针"，按能力分五类（能力递进）：

| 类别 | 支持操作 | 典型容器 |
|---|---|---|
| 输入 input | `++`、`*`（读）、`==` | 流 |
| 输出 output | `++`、`*`（写） | 流 |
| 前向 forward | 上面 + 可多次遍历 | `forward_list` |
| 双向 bidirectional | 上面 + `--` | `list`、`set`、`map` |
| **随机访问 random-access** | 上面 + `+n`、`-n`、`[]`、`<` | `vector`、`array`、`deque` |

**算法按"最低要求的迭代器类别"来写**：`sort` 要求随机访问，`find` 只要求前向。**越强的迭代器，能用的算法越多、越高效**——这就是为什么 `vector`（随机访问）是万能选手。

```cpp
std::vector<int> v{5,3,8,1};
std::sort(v.begin(), v.end());           // begin()/end() 返回随机访问迭代器
auto it = std::lower_bound(v.begin(), v.end(), 5);   // 二分：要求随机访问
int x = *it;                             // 解引用，像指针
```

> **关键认识**：指针本身就是"随机访问迭代器"，所以 C 数组也能直接喂给 STL 算法：`std::sort(arr, arr + n)`。迭代器把"指针"这个概念**抽象成接口**，让算法与容器解耦。

---

## 4. 算法：对范围操作，不认容器

`<algorithm>` 里上百个算法，全部形如 `algo(first, last, ...)`，操作 **[first, last)** 这个**半开区间**：

```cpp
std::vector<int> v{1,2,3,4,5};
auto it = std::find(v.begin(), v.end(), 3);       // 找 3
bool all_pos = std::all_of(v.begin(), v.end(), [](int x){ return x > 0; });
int sum = std::accumulate(v.begin(), v.end(), 0); // <numeric>
// C++20 ranges 让写法更简洁（16）：
// auto it = std::ranges::find(v, 3);
```

**为什么 `[first, last)` 半开区间**：`last` 是"哨兵"（past-the-end），`first == last` 表示空区间；遍历时 `while (first != last) { ...; ++first; }` 简洁无边界特判。

---

## 5. 分配器：内存从哪来（infra 最相关）

容器通过**分配器（allocator）**申请内存，默认 `std::allocator<T>` 走 `::operator new`。你可以**替换它**来做自定义内存管理：

```cpp
// 自定义分配器：从预分配的内存池取内存（省去频繁 malloc 的开销）
// 这在 HPC/游戏/实时系统里很常见：一次性大块分配 + 池化分配
```
**为什么 infra 关心分配器**：`new`/`malloc` 有锁竞争和碎片；高频小对象分配是性能杀手。用内存池分配器，把"向系统要内存"变成"从池里拿"，快一个数量级。这也是你 CUDA 里 `cudaMalloc` 很贵（要同步、要驱动调用）所以要用"预分配 + 池化"的同一思想。

> 进阶：`pmr`（多态内存资源，C++17）让容器**运行时**换分配器，无需改容器类型（`16` 会提）。

---

## 6. 图示：sort 如何"只认迭代器不认容器"

```
std::sort(v.begin(), v.end())        std::sort(a.begin(), a.end())        std::sort(arr, arr+n)
        │  vector<int>                       │  deque<int>                      │  C 数组
        ▼                                   ▼                                 ▼
    随机访问迭代器 ◄────────── sort 只依赖"随机访问迭代器"这一接口 ──────────► 指针也是随机访问迭代器
```

一份 `sort` 代码，三种容器通吃——这就是"解耦"的价值。

---

## 7. 易错点

1. **迭代器失效**：`vector` 扩容/插入/删除后，旧迭代器、指针、引用**全部失效**。遍历中修改容器是经典 bug。
2. **`list` 迷信**：以为"插入 O(1)"就快，忽略了缓存友好性。**默认 vector，用数据说话。**
3. **`[]` vs `at()`**：`v[i]` 越界是未定义行为（不检查），`v.at(i)` 越界抛 `out_of_range`（检查，稍慢）。**调试用 at，性能确认后用 []。**
4. **`reserve` vs `resize`**：`reserve` 只预分配容量（size 不变），`resize` 改变元素个数（会构造新元素）。别混。
5. **`erase` 返回值**：`v.erase(it)` 返回下一个有效迭代器；循环删除要写 `it = v.erase(it)`，否则迭代器失效。
6. **`priority_queue` 自定义比较**：它的比较器是"**大顶堆**"（`less` 默认最大在顶），和 `sort` 的升序直觉相反，容易写反。

---

## 8. 自测题

1. **概念题**：STL 的"容器/迭代器/算法/分配器"四部件如何解耦？为什么算法只需认识迭代器就能用于所有容器？
2. **应用题**：写一个泛型函数 `find_min(first, last)`（用迭代器），分别对 `vector<int>` 和 C 数组 `int arr[]` 调用，验证"指针也是迭代器"。
3. **思考题**：为什么 HPC 里 `vector` 几乎总是优于 `list`？从"内存连续性与缓存命中"的角度解释，并联系你 CUDA 里"合并访存"的经验——它们是不是同一个道理？

---

## 来源与延伸

- *C++ Primer* 第 5 版：第 9 章（顺序容器）、第 10 章（泛型算法）、第 11 章（关联容器）
- cppreference：[Containers library](https://en.cppreference.com/w/cpp/container)、[Iterator](https://en.cppreference.com/w/cpp/iterator)、[Algorithm](https://en.cppreference.com/w/cpp/algorithm)、[Allocator](https://en.cppreference.com/w/cpp/memory/allocator)
- C++ Core Guidelines：[SL.con.1 优先 STL](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#slcon1-prefer-using-stl-array-or-vector-instead-of-a-c-array)、[SL.con.2 默认用 vector](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#slcon2-prefer-using-stl-vector-by-default-unless-you-have-a-reason-to-use-a-different-container)
- 待深挖：内存池/pmr（`16`）、缓存与数据局部性（`14`）、ranges（`16`）
