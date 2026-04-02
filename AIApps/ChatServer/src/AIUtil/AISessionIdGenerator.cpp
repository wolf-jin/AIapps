#include"../include/AIUtil/AISessionIdGenerator.h"



std::string AISessionIdGenerator::generate(){
    // auto now = std::chrono::system_clock::now().time_since_epoch().count();
    // long long randVal = std::rand() % 100000; // 0~99999

    // 🌟 架构师魔法：thread_local
    // 为每个 Worker 线程创建一个专属的、互不干扰的随机数引擎！
    // 既保证了绝对的线程安全，又完全不需要加锁，性能榨干到极致！
    thread_local std::mt19937_64 generator(std::random_device{}());
    
    // 生成一个 0 到 10亿 之间的超大随机数
    std::uniform_int_distribution<long long> distribution(0, 1000000000);

    // 获取高精度时间戳（纳秒级/微秒级，取决于编译器）
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    
    // 从专属引擎里取出一个高质量随机数
    long long randVal = distribution(generator);


    // 异或合并，生成最终 ID
    long long rawId = now ^ randVal;
    return std::to_string(rawId);
}
