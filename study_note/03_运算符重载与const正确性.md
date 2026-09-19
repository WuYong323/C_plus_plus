# 03 · 运算符重载与 const 正确性（Operator Overloading & Const Correctness）

- 主题：cpp
- 日期：2026-09-19
- 状态：[精]
- 标签：#C++ #OOP #运算符重载 #const正确性 #static #friend
- 一句话结论：运算符重载让你的类型像内置类型一样运算；const 是"我承诺不改"的接口契约，贯穿参数、成员、返回值。
- 相关笔记：[[01_类与对象_封装构造析构]]、[[02_对象生命周期_拷贝移动与三五法则]]、[[10_模板基础与泛型编程]]
- 前置要求：01 的成员函数/const 成员函数，竞赛中 `sort` 自定义比较器

---

## 精炼段

运算符重载不是发明新语法，而是给已有的运算符（`+ - * == [] <<` 等）**补上"你的自定义类型"这一种操作数**，让 `a + b`、`a == b`、`cout << a` 像内置类型一样自然。它分两类：成员函数形式（`= [] () ->` 必须成员）和非成员/友元形式（对称的 `+` `==` 建议非成员，让左操作数也能隐式转换）。`const` 正确性是接口的"契约"：`const` 成员函数承诺不修改对象，`const` 参数承诺不修改入参，`const` 返回值阻止"改临时值"——它的传播规则是工业 C++ 里最硬的一道纪律，也是你能写出可被他人安全调用的库的前提。

---

## 0. 一句话直觉

> 竞赛里你写 `sort(v.begin(), v.end(), cmp)`，要传一个"比较函数"告诉 sort 怎么比大小。运算符重载做的事本质一样——只不过把"比较函数"变成了 `operator<`，于是你可以直接写 `sort(v.begin(), v.end())` 甚至 `a < b`，让自定义类型**长得像内置类型**。`const` 则是另一个维度：在函数签名上白纸黑字写"我读、不改"，编译器替你监督，谁违规谁编译失败。

---

## 1. 运算符重载：本质是一个"名字特殊的函数"

`a + b` 就是 `operator+(a, b)` 的语法糖。可重载的运算符有几十个（`+ - * / % == != < > <= >= && || ! & | ^ ~ << >> += -= ... [] () -> ++ -- new delete` 等），但**不能重载**的有：`.`、`::`、`?:`、`sizeof`、`typeid`、`.*`，且**不能发明新运算符**（如 `**`）。

```cpp
class Vec2 {
public:
    Vec2(double x, double y) : x_(x), y_(y) {}
    double x() const { return x_; }
    double y() const { return y_; }
private:
    double x_, y_;
};

// 非成员函数形式（推荐用于对称运算）：
Vec2 operator+(const Vec2& a, const Vec2& b) {   // 按 const 引用传参：不拷贝、不修改
    return Vec2(a.x() + b.x(), a.y() + b.y());
}
bool operator==(const Vec2& a, const Vec2& b) { return a.x()==b.x() && a.y()==b.y(); }
std::ostream& operator<<(std::ostream& os, const Vec2& v) {   // 让 cout << v 可用
    return os << "(" << v.x() << ", " << v.y() << ")";
}

Vec2 p(1,2), q(3,4);
Vec2 r = p + q;        // 等价 operator+(p, q)
bool same = (p == q);
std::cout << r;        // (4, 6)
```

### 1.1 成员 vs 非成员，怎么选

| 形式 | 写法 | 何时用 |
|---|---|---|
| 成员函数 | `Vec2 operator+(const Vec2& b) const` | 必须访问私有成员、或语义上"属于左操作数" |
| 非成员（可 friend） | `Vec2 operator+(const Vec2& a, const Vec2& b)` | **对称运算**（`+` `==` `*` 等），让两边都能隐式转换 |

**必须声明为成员函数**的四个：`=`（赋值）、`[]`（下标）、`()`（调用）、`->`（成员访问）——因为它们绑定在左操作数上，语义上只能成员。

**为什么 `+` 建议非成员？** 看这个隐式转换陷阱：
```cpp
// 若 + 是成员：p + 3 能隐式把 3 转成 Vec2 吗？要看 Vec2 有没有非 explicit 的 Vec2(double) 构造
// 更关键的是 3 + p：若是成员函数，左操作数必须是 Vec2，3 + p 编译失败；
// 若是非成员，3 + p 和 p + 3 都能隐式转换，对称且直观。
```

### 1.2 `operator<` 让 STL 直接用（接回你的竞赛经验）

```cpp
struct Student { std::string name; int score; };
bool operator<(const Student& a, const Student& b) { return a.score > b.score; }  // 分数高排前

std::vector<Student> v{...};
std::sort(v.begin(), v.end());          // 不再需要写 cmp 函数
// 或 C++20 之前手动给 sort 传，C++20 的 std::ranges::sort 还能用 projections（16 细讲）
```
> 你竞赛里写的 `sort(v.begin(), v.end(), [](auto&a,auto&b){return a.x<b.x;})`，本质上就是给 `operator<` 提供一个"临时的替代品"。理解了 operator< 你就理解了 sort 的第三个参数在干嘛。

---

## 2. const 正确性：接口的契约

`const` 在 C++ 里有四个位置的语义，务必分清：

| 位置 | 写法 | 含义 |
|---|---|---|
| 成员函数 | `int rows() const` | **承诺：调用我不修改对象**（编译器监督） |
| 参数（引用/指针） | `void f(const Vec2& v)` | 承诺：不修改入参，且**不拷贝**（引用） |
| 返回值 | `const float& at(...) const` | 返回的引用是只读的，阻止外部"改临时值" |
| 变量本身 | `const Vec2 v(0,0)` | v 一旦初始化不可再改 |

**const 传播铁律（最重要）**：一旦你在某处用了 `const`，它会"传染"——`const` 对象只能调 `const` 成员函数；`const` 成员函数里不能改成员，也不能调非 `const` 成员函数。你要一路把 `const` 补对，否则层层报错。

```cpp
class Matrix {
public:
    float& at(int i, int j)             { return data_[i*cols_+j]; }       // 可写
    const float& at(int i, int j) const { return data_[i*cols_+j]; }       // 只读
    // 两个重载：非 const 对象调第一个，const 对象调第二个
};
const Matrix cm(2,2);
cm.at(0,0);          // OK：调 const 版本，返回 const float&
// cm.at(0,0) = 1;   // 编译错误：const 引用不可写

void print(const Matrix& m) {   // 只读参数
    m.at(0,0);                  // 只能调 const 成员
    // m.at(0,0) = 1;           // 错误：print 承诺不改 m
}
```

**为什么要这么较真？** 因为 `const` 是**契约**：一个接受 `const Matrix&` 的函数，向所有调用者保证"我不会动你的对象"。调用者才敢放心传。没有 `const` 正确性，库就无法被安全复用——这是"能写"和"能写给别人用"的分水岭。

---

## 3. static 成员与 friend（补充两块拼图）

### 3.1 static 成员：属于类，不属于某个对象

```cpp
class Counter {
public:
    Counter() { ++count_; }                 // 每个对象构造，类级计数 +1
    static int count() { return count_; }   // 静态成员函数：没有 this！
private:
    static int count_;                      // 静态数据成员：全类共享一份
};
int Counter::count_ = 0;                    // 静态数据成员必须在类外定义（一次）

Counter a, b;
Counter::count();      // 通过类名调用，无需对象；返回 2
```
- **静态成员函数没有 `this`**：不能访问非静态成员（因为没有"当前对象"）。
- **静态数据成员**全类共享一份，常用于"类级常量/计数/单例资源"。

### 3.2 friend：授权一个外部函数/类访问私有成员

```cpp
class Vec2 {
    friend Vec2 operator+(const Vec2&, const Vec2&);  // 授权这个非成员函数碰私有
    friend std::ostream& operator<<(std::ostream&, const Vec2&);
public:
    Vec2(double x, double y) : x_(x), y_(y) {}
private:
    double x_, y_;
};
// 现在 operator+ 能直接读 a.x_（不必走 x() getter）
```
> `friend` 是"**最小授权**"——只给需要访问私有的那个函数开一扇后门，而不是把数据改成 public。谨慎使用：friend 会打破封装，只在运算符重载、紧密耦合的类之间用。

---

## 4. 图示：一次 `p + q` 到底发生了什么

```
p + q
  │  语法糖展开
  ▼
operator+(const Vec2& a, const Vec2& b)   ← p、q 以 const 引用传入（零拷贝、不可改）
  │
  ▼
return Vec2(a.x()+b.x(), a.y()+b.y());    ← 构造临时结果（可能触发 RVO，见 02/08）
```

---

## 5. 易错点

1. **对称运算符写成成员** → `3 + p` 编译失败、`p + 3` 也受限于隐式转换。对称的 `+ == <` 用非成员（friend）。
2. **`const` 对象调了非 const 成员** → 编译错误。这是设计信号：说明要么函数该加 `const`，要么你不该用 `const` 对象。
3. **`const` 成员函数里改了成员** → 编译错误。若"逻辑上不改但物理上要改"（如缓存），用 `mutable` 关键字标记那个成员。
4. **返回非 const 引用暴露私有数据**：`float*& data()` 或 `float& at(i,j)` 不加 const 版本，等于把私有数据"借"出去任人改，破坏封装。
5. **重载 `&& ||` 丢了短路语义**：重载后的 `&&`/`||` 不再短路（两边都会求值），且求值顺序未定——**几乎永远别重载它们**。
6. **static 数据成员忘了类外定义** → 链接错误 undefined reference。
7. **运算符重载滥用**：`operator+` 里干"乘法"的事、给不相关类型硬塞 `operator<`——语义必须和内置类型一致（principle of least surprise）。

---

## 6. 自测题

1. **概念题**：`= `、`[]`、`()`、`->` 为什么必须声明为成员函数，而不能是自由函数？对称运算符 `+`、`==` 为什么建议声明为非成员函数？
2. **应用题**：为 `Vec2` 补上 `operator-`、`operator*`（标量乘）、`operator!=`，并让 `std::sort` 能对一个 `vector<Vec2>` 按 x 坐标排序（写出 `operator<` 或 C++20 的写法）。确保每个都 `const` 正确。
3. **思考题**：`operator[]` 通常要同时提供"可写"和"只读"两个重载（就像本笔记的 `at`）。为什么不能只提供一个返回 `float&` 的版本给 `const` 对象用？（提示：`const` 对象里 `data_` 是 const 的，返回 `float&` 会怎样？）这和你 01 里写的 `const Matrix` 有什么关系？

---

## 来源与延伸

- *C++ Primer* 第 5 版：第 14 章（运算符重载）、第 2.4 节（const 限定符）、第 7.6 节（static）
- cppreference：[Operator overloading](https://en.cppreference.com/w/cpp/language/operators)、[cv (const/volatile) qualifiers](https://en.cppreference.com/w/cpp/language/cv)
- C++ Core Guidelines：[C.160 定义运算符要成对](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c160-define-operators-primarily-to-mimic-conventional-usage)、[Con.2 const 成员函数](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#con2-by-default-make-member-functions-const)
- 待深挖：`operator<=>`（C++20 三路比较，`16`）、移动赋值里运算符重载与资源管理的结合（`08`）、lambda 与函数对象的底层关系（`12`）
