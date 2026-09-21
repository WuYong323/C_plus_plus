// ============================================================================
//  多态 · demo2：虚函数 + 对象模型 + 手搓 vtable
//  目标：vptr 多出的 8 字节、动态分发、手搓分发表(对照 Cool codegen)、虚析构
//  构建    ：build.bat（或 g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo2.exe）
// ============================================================================

#include <iostream>
#include <memory>
#include <vector>

// ---------------------------------------------------------------------------
// 1. 加一个 virtual，对象多了 8 字节（vptr）
// ---------------------------------------------------------------------------
struct Plain { int x;};                  // 无虚函数
struct Poly { virtual void f() {}};      // 有虚函数（哪怕函数体为空、没数据成员）

// ---------------------------------------------------------------------------
// 2. 真正的 C++ 多态：Shape/Circle/Square
// ---------------------------------------------------------------------------
class Shape {
public:
    virtual ~Shape() = default;                 // 虚析构（delete 基类指针才安全）
    virtual double area() const = 0;            // 纯虚函数：抽象类
    virtual const char* name() const = 0;
};

class Circle : public Shape{
public:
    explicit Circle(double r) : r_(r) {}
    double area() const override { return 3.141592653589793 * r_*r_;}
    const char* name() const override { return "Circle"; }
private:
    double r_;
};

class Square : public Shape{
public:
    explicit Square(double s) : s_(s) {}
    double area() const override { return s_*s_; }
    const char* name() const override { return "Square"; }
private:
    double s_;
};

// ---------------------------------------------------------------------------
// 3. 手搓 vtable：把 C++ 编译器做的事，亲手做一遍
//    Cool 编译器里：对象头存 tag → class_meta → vtable → 函数指针
//    这里简化：对象头存 vtbl 指针 → vtbl[槽] → 函数指针（即 C++ 的 vptr → vtable）
// ---------------------------------------------------------------------------

// 分发表 = 函数指针表（相当于 C++ 的 vtable / Cool 的 @vtable_Cons）
struct ShapeVtbl {
    double      (*area)(const void* self);   // 槽 0
    const char* (*name)(const void* self);   // 槽 1
};

// 对象头：第一个成员就是 vtbl 指针，位于 offset 0（相当于 vptr / tag）
struct ShapeObj {
    const ShapeVtbl* vtbl;
};

// 派生对象：头部仍是 vtbl，后面追加自己的数据（继承的属性在前、新增在后）
struct CircleObj {
    const ShapeVtbl* vtbl;   // 继承的"头部"
    double radius;           // 新增数据
};
struct SquareObj {
    const ShapeVtbl* vtbl;
    double side;
};

// Circle 的"方法实现"：第一个参数 self（相当于 C++ 的 this / Cool 的 self）
static double circle_area(const void* self) {
    const auto* c = static_cast<const CircleObj*>(self);
    return 3.141592653589793 * c->radius * c->radius;
}
static const char* circle_name(const void*) { return "Circle"; }

static double square_area(const void* self) {
    const auto* s = static_cast<const SquareObj*>(self);
    return s->side * s->side;
}
static const char* square_name(const void*) { return "Square"; }

// 每个类一张分发表：覆盖方法=同一槽位换函数指针
static const ShapeVtbl circle_vtbl = { circle_area, circle_name };
static const ShapeVtbl square_vtbl = { square_area, square_name };

// 动态分派：obj->vtbl->槽( obj ) —— 这就是 vptr->vtable->fn
static double dispatch_area(const ShapeObj* s) { return s->vtbl->area(s); }


int main() {
    std::cout << "== 1) vptr 多出的 8 字节 ==\n";
    std::cout << "  sizeof(Plain) = " << sizeof(Plain) << "  (只有 int x)\n";
    std::cout << "  sizeof(Poly)  = " << sizeof(Poly)  << "  (没数据成员却有 8 字节 = vptr)\n";

    std::cout << "\n== 2) C++ 真多态：一套接口，多种实现 ==\n";
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(2.0));
    shapes.push_back(std::make_unique<Square>(3.0));
    for (const auto& s : shapes) {          // s 是 Shape&，指向真实子类
        std::cout << "  " << s->name() << " area = " << s->area() << "\n";
        // 动态分发：运行时按 s 的真实类型跳转（vptr -> vtable -> fn）
    }

    std::cout << "\n== 3) 手搓 vtable：和编译器做的事一模一样 ==\n";
    CircleObj c{ &circle_vtbl, 2.0 };       // 构造对象：头=分发表指针
    SquareObj q{ &square_vtbl, 3.0 };
    ShapeObj* pc = reinterpret_cast<ShapeObj*>(&c);   // 当"基类"用（头在 offset 0，无需调整）
    ShapeObj* pq = reinterpret_cast<ShapeObj*>(&q);

    std::cout << "  C++ 虚调用: " << shapes[0]->name() << " = " << shapes[0]->area() << "\n";
    std::cout << "  手搓分发表: " << dispatch_area(pc) << "  (obj->vtbl->area(obj))\n";
    std::cout << "  手搓分发表: " << dispatch_area(pq) << "\n";
    // 对照：c.vtbl->area(&c) 和 pc->vtbl->area(pc) 是同一件事，只是后者经过了"基类指针"

    std::cout << "\n== 4) 虚析构：delete 基类指针能正确析构子类 ==\n";
    Shape* p = new Circle(5.0);
    delete p;   // 若 ~Shape 非虚，这里只调 Shape 析构，Circle 资源泄漏

    std::cout << "\n[demo2 end]\n";
    return 0;
}












