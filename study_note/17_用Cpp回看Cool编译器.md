# 17 · 用 C++ 回看你的 Cool 编译器（Re-reading Your Cool Compiler Through C++）★亮点

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #编译原理 #Cool #codegen #动态分派 #vtable #this #装箱 #值语义
- 一句话结论：你手写的 Cool 编译器，就是在用 C++ 重新实现 C++ 自己的多态机制——动态分派=虚表、self=this，看懂对照你就同时吃透了两边。
- 相关笔记：[[06_对象模型_虚表与内存布局]]、[[05_虚函数与多态]]、[[18_系统底层衔接_ABI与内存序]]
- 前置要求：你 `notes/compilers/08_运行时组织`、`09_代码生成` 里已实现的分发表与对象布局

---

## 精炼段

你已经在 `projects/coolc/src/codegen.cpp` 里亲手实现了 OOP 语言的运行时核心：对象 = `{tag, 属性...}`，动态分派 = `tag → class_meta → vtable → 函数指针`。**这套东西就是 C++ 的虚表机制的"教学简化版"**。逐条对照：Cool 的 `self`（隐藏第一参数）= C++ 的 `this`；Cool 的分发表 = C++ 的 vtable；Cool 的 tag = C++ 的 vptr（但 C++ 少一跳）；Cool 的 `SELF_TYPE` ≈ C++ 的协变返回。**两个根本差异**决定了 C++ 比 Cool 快一个量级：① C++ **不装箱**（`int` 是寄存器里的裸值，不是堆上的 `{tag, i32}` 对象）；② C++ 是**值语义**（对象可住栈上/内嵌，不是"一切皆 `i8*` 堆指针"）。读完本篇，你能用 C++ 的视角**反向优化你的编译器**。

---

## 0. 一句话直觉

> 你写编译器时，是"**站在编译器里面**"实现 `obj.method()` 怎么变成 `call fn(obj)`。现在你学会了 C++ 的多态，再回头看：**你实现的那套分发表，就是 C++ 的 vtable；你传的那个 `self`，就是 C++ 的 `this`。** 你不是在学两件无关的事——你是在**用 C++ 重写 C++**，只是教学版把机制显式摊开、工业版由编译器替你藏起来。

---

## 1. 动态分派：你的三步 vs C++ 的两步（核心对照）

你在 `codegen.cpp` 里生成的分派（`09_代码生成` §2.3）：

```llvm
%tag  = load i8*, i8** %l                  ; ① 对象 offset 0 = tag（指向 class_meta）
%meta = bitcast i8* %tag to %class_meta_t*
%vt   = load i8**, i8*** <meta 的 vtable 字段>   ; ② 从 class_meta 取 vtable 指针
%fnp  = load i8*, i8** <vtable + 下标>         ; ③ 按下标取函数指针
%r    = call i8* %fn(i8* %l)               ; ④ 调用，传对象指针为第一实参（= self）
```

C++ 的等价物（`06` §3）：

```
mov rax, [p]      ; ① 取 vptr（对象 offset 0，直接指向 vtable）
mov rdx, [rax+0]  ; ② vtable[槽] = 函数指针
call rdx          ; ③ 调用（this 作为隐藏第一实参）
```

| | 你的 Cool | C++ |
|---|---|---|
| 对象头部 | `tag` → class_meta | `vptr` → vtable（**直接**） |
| 分派步骤 | **三跳**（tag→meta→vtable→fn） | **两跳**（vptr→vtable→fn） |
| 隐藏第一参数 | `self`（`call fn(i8* %l)`） | `this` |
| 覆盖方法 | 同下标换函数指针 | 同槽位换函数指针 |
| 静态分派 | `e@T.f` → 直接 `call @T.f` | 非虚函数 → 直接 `call` |

**为什么 C++ 少一跳**：你的 `tag` 指向 `class_meta`（`{类名字符串, vtable指针, 对象字节数, 类ID}`），因为 Cool 还要用类名（`type_name`）、大小（`copy`）、类 ID（`case` 分派）。C++ 把这些元信息塞进 vtable 本身（vtable 前面有 typeinfo、offset-to-top），于是 `vptr` 直接指向 vtable，省掉中间一跳。

---

## 2. self = this（隐藏第一参数）

你 codegen 里，方法体里所有对 `self` 属性的访问，最终都编译成"从对象指针 + 固定偏移 load/store"；方法调用 `self.method()` 编译成"把 self 作为第一实参传给函数"。**C++ 的成员函数一模一样**：

```cpp
a.add(1);               // C++ 表面写法
// 底层等价于：Class::add(&a, 1)  —— this 作为隐藏第一参数
```

你在 Cool 里显式写的东西（`self` 变量、分发表、属性偏移），C++ 里都是编译器隐式做的——**但机制完全相同**。这也解释了 `06` 里"成员函数和普通函数的唯一底层区别就是多一个 this"。

---

## 3. 对象布局：你的 {tag, 属性} vs C++ 的对象

```
你的 Cool 对象（全在堆上，cool_alloc）：
┌────────────┐ offset 0
│    tag      │ → class_meta
├────────────┤ offset 8
│  属性1       │
├────────────┤
│  属性2       │
└────────────┘

C++ 对象（可栈上/堆上/内嵌）：
┌────────────┐ offset 0
│   vptr      │ → vtable（仅多态类有）
├────────────┤
│  数据成员1  │
├────────────┤
│  数据成员2  │
└────────────┘
```
**惊人相似**：都是"头部放一个指针（tag/vptr）+ 后面按声明顺序放属性"，且"继承的属性保持原偏移"——所以"把派生类当基类用"时偏移不变。**你实现的对象布局就是 C++ 的（多态对象）布局**，只是 C++ 的"非多态类"没有 vptr（省 8 字节），而 Cool 每个对象都有 tag（因为 Cool 里连 Int 都是对象）。

---

## 4. 两个根本差异：为什么 C++ 快一个量级 ★

### 4.1 装箱 vs 不装箱

你的 Cool（`09_代码生成` §1）：**一切皆 `i8*`，Int/Bool 都装箱**：
```llvm
%Int = type { i8*, i32 }     ; 一个整数 = 堆上的 {tag, 值} 对象
; 算术：unboxInt（从对象取 i32）→ add → boxInt（新分配一个 Int 对象装回去）
```

C++：**`int` 就是寄存器里的 4 字节裸值**，`a + b` 直接一条 `add` 指令，无堆分配、无拆装箱。

```cpp
// C++：int a = 1, b = 2; int c = a + b;
// 汇编：mov eax, 1; add eax, 2  —— 全程寄存器，零内存访问
```

**这是 C++ 高性能的根源**：基本类型不装箱。你 Cool 里 `1 + 2` 要两次 `cool_alloc`（两个 Int 对象）+ 拆箱 + 装箱，C++ 里一条 `add`。你 note 09 自己都写了"真实编译器（如 C++）通常对 Int 用不装箱表示（更快）"——现在你彻底理解了**为什么**。

### 4.2 值语义 vs 引用语义

你的 Cool：**一切皆 `i8*`**，对象全在堆上，变量都是指向堆的指针（引用语义）。

C++：**值语义**——对象可以住在**栈上、寄存器里、或内嵌在别的对象里**，不需要强制堆分配：

```cpp
struct Point { int x, y; };
Point p{1,2};            // 直接在栈上，两个 int 紧挨着，无堆分配
std::vector<Point> v;    // 元素紧挨着连续存放（缓存友好，14 篇）
// 而 Cool 里 Point 必须 cool_alloc 到堆上，vector 里存的是一个个指针（离散）
```

**值语义 → 缓存友好**：C++ 的 `vector<Point>` 是连续内存，Cool 的"对象数组"是"指针数组 + 一堆离散对象"。这正是 `14` 篇 AoS/SoA、合并访存的根——**C++ 快，一半来自不装箱，一半来自值语义带来的数据局部性**。

---

## 5. SELF_TYPE ≈ 协变返回（covalect return）

你的 Cool 有 `SELF_TYPE`：表示"`self` 的动态类型"，典型用在 `Object.copy(): SELF_TYPE`——保证"拷贝出的对象和原对象同类型"（而非退化成 Object）。

C++ 的对应物是**协变返回类型（covariant return type）**：派生类覆盖虚函数时，返回类型可以是"更具体"的指针/引用：

```cpp
class Shape {
public:
    virtual Shape* clone() const = 0;      // 基类返回 Shape*
};
class Circle : public Shape {
public:
    Circle* clone() const override { return new Circle(*this); }  // 派生类返回 Circle*
};
// 调 c.clone() 得到 Circle*（而非 Shape*）—— 这就是 SELF_TYPE 的效果
```
> 差异：C++ 的协变返回只支持**指针/引用**、且要求派生类返回类型是基类返回类型的子类；Cool 的 `SELF_TYPE` 更"直白"（就是"我的类型"）。但**思想同源**：让"返回自己"不丢失类型信息。

---

## 6. 用 C++ 的视角反向优化你的 Cool 编译器（进阶思考）

你学会了 C++ 的实现选择后，能看到你编译器里三处可优化点：

1. **三跳 → 两跳**：把对象的 `tag` 直接指向 vtable（而非 class_meta），分派少一跳。**代价**：`type_name`/`case`/`copy` 需要的类名/类 ID/大小就没地方放了——这正是 C++ 把 typeinfo 塞进 vtable 的做法，你需要把 vtable 扩展成"元信息 + 函数指针"混合结构。
2. **Int/Bool 拆箱**：把 Int 从 `{tag, i32}` 堆对象改成"寄存器里的裸 i32"，算术零分配。**代价**：要处理"基本类型 vs 对象"的统一问题（C++ 靠类型系统在编译期区分，你需要在 codegen 里加类型分支）。
3. **去虚化（devirtualization）**：当编译期能确定动态类型（如 `new Circle` 后立即调 `draw`、或 `final` 类），直接生成 `call @Circle.draw` 而非查表。C++ 编译器常做这个优化。

> 这三个优化，就是"从教学编译器走向工业编译器"的台阶——也是你将来读 LLVM/MLIR/TVM 源码时反复遇到的真实设计。

---

## 7. 案例：用 C++ 视角读一遍你的 factorial.cl 分派

你 `list.cl`/`factorial.cl` 里 `fact(n-1)` 的递归调用，codegen 生成的是"动态分派（走 vtable 下标 11）"。用 C++ 视角问一句：**这个调用真的需要动态分派吗？**

- 若 `fact` 是 `Main` 自己的方法、编译期就能确定类型 → 可以用**静态分派**（直接 `call`），省两次间接寻址。
- C++ 里对应场景：非虚成员函数就是静态分派；虚函数但类型确定时，编译器会**去虚化**。

**这让你明白**：Cool 的"所有方法调用都动态分派"是个**简化**（教学版不区分静态/动态），而 C++ 用 `virtual` 关键字让你**显式选择**哪些方法需要动态分派、哪些直接调用——**这是性能与灵活性的显式权衡**（`05`/`06` 已铺垫）。

---

## 8. 易错点

1. **把"Cool 全动态分派"当 OOP 的必然**：不是。C++ 里非虚函数就是静态分派，虚函数才动态分派——**是否动态分派是设计选择**。
2. **忘了 C++ 不装箱**：别把 Cool 的"Int 是对象"带进 C++ 思维——C++ 的 `int` 就是裸值，这决定了性能。
3. **混淆 tag 和 vptr**：Cool 的 tag 多一跳（指向 class_meta），C++ 的 vptr 直接指 vtable。别以为它们一模一样。
4. **值语义 vs 引用语义混淆**：C++ 对象默认是"值"（拷贝是独立副本），Cool 是"引用"（全是指针）。这是两边写代码时最大的思维差异。

---

## 9. 自测题

1. **概念题**：你的 Cool 编译器分派是"三跳"（tag→class_meta→vtable→fn），C++ 是"两跳"（vptr→vtable→fn）。中间那"一跳"的差别来自哪里？C++ 是怎么省掉它的？
2. **应用题**：在你 `codegen.cpp` 里，如果要把分派从三跳优化成两跳（tag 直接指向 vtable），`type_name`、`case`、`copy` 这三个功能分别会失去什么信息？你该如何改造 vtable 结构来补回它们？
3. **思考题**：C++ 的"不装箱 + 值语义"相比 Cool 的"装箱 + 引用语义"，在"编译一个求和循环 `sum += arr[i]`"时，性能差异的根本来源是什么？（从缓存局部性和堆分配次数两方面谈）

---

## 来源与延伸

- 你的实现：`notes/compilers/08_运行时组织.md`、`notes/compilers/09_代码生成.md`、`projects/coolc/src/codegen.cpp`
- 本套笔记：`06_对象模型`（vtable 机制）、`05_虚函数`（动态/静态分派）、`18_系统底层`（ABI 细节）
- cppreference：[covariant return types](https://en.cppreference.com/w/cpp/language/virtual)、[value category](https://en.cppreference.com/w/cpp/language/value_category)
- 待深挖：ABI 层面 this 到底怎么传（`18`）、去虚化在真实编译器的实现（LLVM devirtualization）
