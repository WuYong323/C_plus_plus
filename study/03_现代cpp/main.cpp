// ============================================================================
// module3_现代Cpp · demo3：RAII + 智能指针 + 移动语义 + 异常安全
//  学习目标：资源跟随对象生命周期、移动"搬"而非"复制"、异常时资源仍不泄漏
//  构建    ：build.bat（或 g++ -std=c++17 -Wall -Wextra -O2 main.cpp -o demo3.exe）
//  说明    ：用"模拟 GPU buffer"演示，不需要真实 CUDA 也能跑通原理
// ============================================================================

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>

// 全局计数器：模拟 GPU 显存总量，跟踪"分配/释放"是否配对
static long g_allocated = 0;            // 已分配的显存总量
static long g_live_blocks = 0;          // 当前活跃的显存块数

// 模拟 cudaMalloc / cudaFree（真实版只需换成 CUDA 调用）
// :: 是全局作用域解析运算符
static void* fake_alloc(std::size_t n) { g_allocated += n; ++g_live_blocks; return ::operator new(n); }         //模拟分配显存
static void  fake_free(void* p, std::size_t n) { g_allocated -= n; --g_live_blocks; ::operator delete(p); }     //模拟释放显存

// 自定义 deleter：让 unique_ptr 管理"非 new 分配"的资源
struct GpuDeleter {
    std::size_t bytes;
    void operator()(float* p) const {
        std::printf("    [deleter] freeing GPU block (%zu bytes), p=%p\n", bytes, (void*)p);
        fake_free(p, bytes);
    }
};

// 一个拥有 GPU 显存的张量（RAII：构造分配、析构释放、移动"搬"所有权）
class GpuTensor {
public:
    GpuTensor(std::size_t n, const std::string& tag)
        : n_(n), tag_(tag),
          data_(static_cast<float*>(fake_alloc(n * sizeof(float))), GpuDeleter{n * sizeof(float)}) {
        std::printf("  [ctor] %s alloc %zu floats, p=%p\n", tag_.c_str(), n_, (void*)data_.get());
    }

    // 移动构造：把源的所有权"搬"过来（改指针，不复制显存）
    GpuTensor(GpuTensor&& o) noexcept
        : n_(o.n_), tag_(std::move(o.tag_) + " (moved)"), data_(std::move(o.data_)) {
        o.n_ = 0;                          // 源被掏空，但保持"有效"状态
        std::printf("  [move ctor] %s 接管了显存，源被掏空\n", tag_.c_str());
    }

    // 禁止拷贝：显存的所有权是唯一的
    GpuTensor(const GpuTensor&) = delete;
    GpuTensor& operator=(const GpuTensor&) = delete;

    std::size_t size() const { return n_; }
    bool owns() const { return data_ != nullptr; }

private:
    std::size_t n_;
    std::string tag_;
    std::unique_ptr<float, GpuDeleter> data_;   // unique_ptr + 自定义 deleter 管显存
};


static void risky(std::size_t n) {
    GpuTensor a(n, "a");              // 分配
    GpuTensor b(n, "b");              // 分配
    throw std::runtime_error("boom"); // 抛出！a、b 的析构必须被栈展开调用
    // 下面这行永远不执行
    (void)0;
}

int main() {
    std::cout << "== 1) RAII：构造分配、析构释放（配对）==\n";
    {
        GpuTensor t(1024, "t");
        std::printf("  块内：live_blocks=%ld\n", g_live_blocks);
    }   // 出作用域，析构自动释放
    std::printf("  块外：live_blocks=%ld  (回到 0，无泄漏)\n\n", g_live_blocks);

    std::cout << "== 2) 移动语义：buffer 所有权转移，不复制显存 ==\n";
    {
        GpuTensor src(2048, "src");
        GpuTensor dst = std::move(src);      // 移动：dst 接管，src 被掏空
        std::printf("  src.owns() = %d, dst.owns() = %d\n", src.owns(), dst.owns());
        std::printf("  注意：整个过程 fake_alloc 只被调用 1 次（没有复制显存）\n");
    }   // 块结束：dst 析构，显存释放
    std::printf("  块外 live_blocks=%ld  (回到 0)\n\n", g_live_blocks);

    std::cout << "== 3) vector 存 GpuTensor：移动构造 noexcept 才敢搬 ==\n";
    {
        std::vector<GpuTensor> tensors;
        tensors.reserve(4);                       // 预留，避免演示中多次扩容
        tensors.push_back(GpuTensor(512, "x"));   // 临时对象 → 移动进容器
        tensors.push_back(GpuTensor(512, "y"));
        std::printf("  容器内 live_blocks=%ld（x、y 各持一块）\n", g_live_blocks);
    }
    std::printf("  容器析构后 live_blocks=%ld\n\n", g_live_blocks);

    std::cout << "== 4) 异常安全：throw 时 RAII 照样释放 ==\n";
    try {
        risky(256);                     // 抛异常
    } catch (const std::exception& e) {
        std::printf("  caught: %s\n", e.what());
    }
    std::printf("  异常后 live_blocks=%ld  (a、b 都被栈展开释放，回到 0)\n\n", g_live_blocks);

    std::cout << "[demo3 end] 最终 live_blocks=" << g_live_blocks << " (应为 0)\n";
    return 0;
}
































