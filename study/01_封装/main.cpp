// ============================================================================
// module1_封装 · demo1：用 class 封装一个动态矩阵
//      封装(私有数据+公开接口)、成员初始化列表、const 成员函数、
//        this 指针、构造/析构、对象内存布局、RAII 雏形、浅拷贝二次释放陷阱
//  构建    ：build.bat（或 g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo1.exe）
//  玩法    ：按注释里的"TRY"提示改一行代码，重新编译观察行为变化
// ============================================================================

#include <iostream>
#include <cstddef>
#include <cstring>   // memset

// ---------------------------------------------------------------------------
// 一个"拥有资源"的矩阵类：这就是 RAII 的雏形。
// 构造 = 分配内存；析构 = 释放内存。竞赛里你要手写 new/delete 且常忘 delete，
// 这里编译器会在对象离开作用域时自动调用析构函数 —— "忘记释放"在逻辑上不可能发生。
// ---------------------------------------------------------------------------
class Matrix{
public:
    // 构造函数：成员初始化列表（冒号后）才是"真正的初始化"
    // 注意顺序必须与成员声明顺序一致（rows_, cols_, data_），否则有静默错误
    Matrix(std::size_t r,std::size_t c) :rows_(r),cols_(c),data_(new float[r*c]()){   // 末尾 () = 零初始化
        std::cout<<"[ctor] Matrix("<<r<<"x"<<c<<") allocated, this = "<<this<<"\n";
    }

    // 析构函数：对象"死亡"时自动调用，释放资源（与构造里的 new[] 严格配对）
    ~Matrix(){
        std::cout<<"[dtor] Matrix freed,     this = "<<this<<"\n";
        delete[] data_;
    }

    // const 成员函数：承诺"不修改对象"；const 对象也只能调用 const 成员函数
    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }

    // 访问元素（非常量版）：显式写 this-> 帮你体会"成员其实是挂在 this 上的"
    float& at(std::size_t i,std::size_t j){
        return this->data_[i*cols_+j];
    }

    // 访问元素（常量版）：const 对象走这里
    const float& at(std::size_t i,std::size_t j) const {
        return data_[i*cols_+j];
    }

    // 打印对象内存布局：观察成员按声明顺序连续存放（深入虚表/对齐）
    void dump_layout() const {
        using Byte = const char*;
        std::cout<<"  this    = "<<this<<"\n";
        std::cout<<"  &rows_  = "<<&rows_<<"  (offset "<<(Byte(&rows_)-Byte(this))<<")\n";
        std::cout<<"  &cols_  = "<<&cols_<<"  (offset "<<(Byte(&cols_)-Byte(this))<<")\n";
        std::cout<<"  &data_  = "<<&data_<<"  (offset "<<(Byte(&data_)-Byte(this))<<")\n";
        std::cout<<"  sizeof(Matrix) = "<<sizeof(Matrix)<<" bytes\n";
    }

private:
    std::size_t rows_;
    std::size_t cols_;
    float* data_;
};




int main(){
    std::cout<<"==1) 构造与析构的生命周期==\n";
    {
        Matrix m(2,3);
        m.at(0,0)=1.5f;
        m.at(1,2)=4.0f;
        std::cout<<"m(0,0)="<<m.at(0,0)<<", m(1,2)="<<m.at(1,2)<<"\n";
        
        // TRY-1：解开下面这行，再编译 —— 编译器会报 "data_ is private"。
        //       这就是封装：外部代码不允许碰一下私有数据。
        // m.data_ = nullptr;
    }   // <- 离开作用域，析构函数自动调用，内存不泄漏

    std::cout<<"\n== 2) const 对象只能调用 const 成员函数 ==\n";
    const Matrix cm(1,2);
    std::cout<<"cm.rows()="<<cm.rows()<<", cm.cols()="<<cm.cols()<<"\n";   //OK: rows()、cols() 是 const 成员函数
    // TRY-2：解开下面这行，再编译 —— const 对象不能调用非常量成员函数。
    // cm.at(0,0)=2.0f;

    std::cout<<"\n== 3) 对象内存布局 （成员按声明连续顺序）==\n";
    Matrix m2(4,4);
    m2.dump_layout();
    // 预期（x64）：rows_ offset 0、cols_ offset 8、data_ offset 16、sizeof=24
    //              （size_t 8 字节 × 2 + 指针 8 字节）

    std::cout << "\n== 4) 浅拷贝陷阱（02 的「三法则」预告）==\n";
    // TRY-3：解开下面这两行再编译运行 —— 会看到两个对象的 data_ 指向同一块内存，
    //        两个析构都 delete[] 它 → 崩溃（double free / heap corruption）。
    //        这正是 02 要讲的"浅拷贝 vs 深拷贝"。
    // Matrix copy = m2;           // 编译器生成的默认拷贝构造：只拷贝指针，不拷贝数据
    // std::cout << "  copy.data_ 与 m2.data_ 指向同一地址（浅拷贝）\n";

    std::cout << "\n[main] end of scope -> destructors run automatically\n";
    return 0;
}