# C++ 面向对象与现代工业技术 · 自学笔记（主题 Hub）

> 主题：cpp（新增主题）
> 依据：*C++ Primer*（第 5 版）/ *C++ Primer Plus* / cppreference / C++ Core Guidelines / 现代工业与前沿实践（联网查证）
> 读者定位：算法竞赛出身（CCCC 国三 / 码蹄杯省一 / 校 ACM 集训队），用 C++ 手写 CS143 编译器 + HPC/infra/融合算子/深度学习编译器
> 目标：补齐「竞赛过程式 C++ → 工业面向对象 + 现代 C++」这一段，**重点吃透对象模型底层**，同时长工程与开发能力
> 基线标准：**C++17 + C++20/23 亮点**；编译器 g++ 15.2.0（MSYS2 MinGW），构建命令 `-std=c++17`

---

## 0. 一句话总览

C++ 是「带类的 C + 现代抽象」。竞赛你只用到了它的**过程式子集**（`struct` 打包数据、STL 容器、裸函数）；工业里真正的主角是**类、继承、多态、RAII、模板、移动语义、多文件工程**。本套笔记把后半部分按「封装 → 多态 → 现代 C++ → 泛型 → 工程 → 前沿衔接 → 扩展实战」七个模块串起来，每篇都从你已有的经验出发，落到对象在内存里到底是什么样，最后用一个 capstone 项目收口。

---

## 1. 文档地图（22 篇，按顺序读，勿跳）

> 每篇末尾有「自测题」；每个模块末有「模块测验」。代码片段对应 `projects/learn/cpp/` 下的可运行 demo。

| # | 笔记 | 一句话内容 | 配套 demo |
|---|---|---|---|
| 00 | `00_路线图与全景_从竞赛到工业.md` | 竞赛 C++ vs 工业 C++、心智模型、怎么学 | — |
| 01 | `01_类与对象_封装构造析构.md` | class/struct、访问控制、构造/析构、this、RAII 雏形 | `module1_封装/` |
| 02 | `02_对象生命周期_拷贝移动与三五法则.md` | 拷贝/移动、三·五·零法则、深/浅拷贝、二次释放 | `module1_封装/` |
| 03 | `03_运算符重载与const正确性.md` | 运算符重载、const 传播、static、friend | `module1_封装/` |
| 04 | `04_继承与派生.md` | 继承语法、访问控制、构造析构顺序、切片 | `module2_多态/` |
| 05 | `05_虚函数与多态.md` | virtual、override/final、抽象类、接口、动态绑定 | `module2_多态/` |
| 06 | `06_对象模型_虚表与内存布局.md` | **vtable/vptr、内存布局、多重继承、与 Cool 编译器 codegen 呼应** | `module2_多态/` |
| 07 | `07_RAII与智能指针.md` | RAII、unique_ptr/shared_ptr/weak_ptr、所有权 | `module3_现代Cpp/` |
| 08 | `08_移动语义与完美转发.md` | 右值引用、std::move、完美转发、RVO/拷贝省略 | `module3_现代Cpp/` |
| 09 | `09_异常与错误处理.md` | 异常机制、异常安全、optional/expected、错误码 | `module3_现代Cpp/` |
| 10 | `10_模板基础与泛型编程.md` | 函数/类模板、实参推导、非类型参数、编译期 | `module4_泛型/` |
| 11 | `11_模板进阶_特化与concepts.md` | 特化/偏特化、SFINAE、concepts/requires | `module4_泛型/` |
| 12 | `12_STL深入_容器迭代器分配器.md` | 容器/迭代器/算法/分配器，与竞赛 STL 的差别 | `module4_泛型/` |
| 13 | `13_多文件工程与构建_CMake.md` | 头文件/源文件、ODR、链接、CMake | `module5_工程/` |
| 14 | `14_内存模型与性能工程.md` | 对象布局、缓存、数据局部性、move 性能 | `module5_工程/` |
| 15 | `15_现代Cpp工程实践.md` | Core Guidelines、Google Style、测试、调试、ABI | `module5_工程/` |
| 16 | `16_Cpp20_23新特性.md` | concepts/ranges/span/coroutines/modules | `module6_前沿衔接/` |
| 17 | `17_用Cpp回看Cool编译器.md` | **动态分发=虚表、SELF_TYPE=this、对象布局=codegen** | `module6_前沿衔接/` |
| 18 | `18_系统底层衔接_ABI与内存序.md` | ABI、调用约定、内存序、与 OS/硬件衔接 | `module6_前沿衔接/` |
| 19 | `19_并发与多线程.md` | thread/mutex/condition_variable/async、与 CUDA 对照 | — |
| 20 | `20_设计模式.md` | 策略/观察者/工厂/单例/type erasure/PIMPL | `capstone_算子分派器/` |
| 21 | `21_constexpr编译期计算.md` | constexpr/consteval/constinit/编译期容器 | — |

> **两篇亮点（与你现状直接挂钩）**：`06` 和 `17` 把 C++ 的虚表/对象布局与你 Cool 编译器 codegen 生成的东西**互相对照**——纯看 Primer 学不到、对你价值最大。

---

## 2. demo 地图（`projects/learn/cpp/`）

| demo | 位置 | 讲什么 | 构建 |
|---|---|---|---|
| 封装 | `module1_封装/` | Matrix 类：构造/析构/this/const/内存布局/三法则陷阱 | `build.bat` |
| 多态 | `module2_多态/` | 图形继承体系 + 虚函数 + 用函数指针手搓"vtable"体会动态分发 | `build.bat` |
| 现代 C++ | `module3_现代Cpp/` | 智能指针 + 移动语义 + 异常安全（buffer 所有权流转） | `build.bat` |
| 泛型 | `module4_泛型/` | 泛型 reduce/matmul + concepts 约束 + STL 分配器 | `build.bat` |
| 工程 | `module5_工程/` | 多文件 + CMake + 单元测试（断言/简单测试框架） | `build.bat` |
| 前沿衔接 | `module6_前沿衔接/` | C++20 特性 + 打印对象布局印证 06/17 | `build.bat` |
| **capstone 综合** | `capstone_算子分派器/` | **类型擦除的算子分派器**：虚函数+模板+RAII+STL+异常串成实战 | `build.bat` |

---

## 3. 学习路线与检查点

| 阶段 | 覆盖笔记 | 检查点（能做什么才算过） |
|---|---|---|
| 模块 1 封装 | 01–03 | 写出一个正确管理资源（无泄漏、无二次释放）的类，const 用得对 |
| 模块 2 多态 | 04–06 | 能用虚函数做动态分发，能**手画 vtable/内存布局**，能讲清它和 Cool 分发表的关系 |
| 模块 3 现代 C++ | 07–09 | 用智能指针替代裸 new/delete，能写移动语义、异常安全代码 |
| 模块 4 泛型 | 10–12 | 能写模板 + concepts，理解 STL 的迭代器/分配器设计 |
| 模块 5 工程 | 13–15 | 能组织多文件工程、写 CMake、写测试、读懂编译/链接报错 |
| 模块 6 前沿衔接 | 16–18 | 能用 C++20 特性，能回看你的 Cool 编译器并讲清对象模型/ABI |
| 模块 7 扩展实战 | 19–21 + capstone | 写多线程、落地设计模式、编译期计算，并用 capstone 把全部知识串成「算子分派器」 |

---

## 4. 环境准备（一次性）

| 工具 | 用途 | 现状 |
|---|---|---|
| g++ 15.2.0 | 编译本套笔记所有 demo | ✅ 已装（`g++ --version` 验证） |
| CMake | 模块 5 多文件工程 | 学 13 之前装（`winget install Kitware.CMake`） |
| 文本编辑器 | 写代码 | VS Code / CLion 任意 |

构建一个 demo：

```bat
cd projects\learn\cpp\module1_封装
build.bat
```

等价于 `g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo.exe`（**module6_前沿衔接 例外，用 `-std=c++20`**，演示 C++20 特性）。

---

## 5. 与你三条线的衔接（为什么学这个）

| 你的主线 | C++ OOP 如何接入 |
|---|---|
| **编译原理（CS143 / Cool）** | Cool 是 OOP 语言；你在 codegen 里生成的分发表 = C++ 的虚表；`SELF_TYPE`/`self` = C++ 的 `this`。`06`/`17` 直接打通 |
| **系统底层 / HPC / infra / 融合算子** | RAII 管 `cudaMalloc/cudaFree`、移动语义管 buffer 零拷贝转移、模板元编程做编译期计算、对象布局决定缓存命中率。`07/08/10/14/18` 是核心 |
| **学术科研（PL / 深度学习编译器）** | C++ 是 LLVM/MLIR/TVM 的实现语言；理解 ABI、对象模型、模板是读这些源码的门票。`11/13/15/18` 支撑 |

---

## 6. 使用约定

- **顺序**：00 → 21；每篇末尾有「本文你掌握了什么 / 易错点 / 下一步」，最后做 capstone 项目收口。
- **类比都标注"这是简化"**：准确表述以正文和 cppreference 为准。
- **术语**：中文讲解，英文原名首次出现给译名（统一收进 `glossary.md`）。
- **状态**：`[精]` 定稿 / `[草]` 未整理；重复与过时由 kb-maintenance 处理。

> 相关主题 hub：`notes/compilers/`（Cool 编译器）、`notes/systems/`（内存模型/并发）、`notes/infra/`（CUDA/算子，与 14/18 衔接）。
