#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>


#include"AIStrategy.h"

class StrategyFactory {

public:
    // Creator 是一个“创建策略对象的函数类型”。
    // 不是直接存 AliyunStrategy 对象
    //而是存一个函数，这个函数调用后返回 std::shared_ptr<AIStrategy>
    using Creator = std::function<std::shared_ptr<AIStrategy>()>;

    static StrategyFactory& instance();

    // 把一个“策略名字 → 创建函数”的映射注册进工厂
    // "1" 注册为创建 AliyunStrategy
    // "2" 注册为创建 DouBaoStrategy

    // 这个函数本身不创建策略对象，它只是往工厂里“登记”。
    void registerStrategy(const std::string& name, Creator creator);
    // 按名字去查表，然后真正创建一个策略对象
    /*
    registerStrategy(...) 是“登记”
    create(...) 是“生成实例”
    */
    std::shared_ptr<AIStrategy> create(const std::string& name);

private:
    // 这是典型的单例模式写法。
    StrategyFactory() = default;
    // 策略名字 → 创建策略对象的函数
    std::unordered_map<std::string, Creator> creators;
};

/*
所以你可以把整个 StrategyFactory 压缩成一句话：
它就是一张从字符串到构造函数的哈希表，再加一个单例壳子。
*/



// 让某个具体策略类可以在程序启动时自动把自己注册进工厂。
template<typename T>
struct StrategyRegister {
    StrategyRegister(const std::string& name) {
        StrategyFactory::instance().registerStrategy(name, [] {
            std::shared_ptr<AIStrategy> instance = std::make_shared<T>();
            return instance;
            });
    }
};

/*

这表示：

不带参数的 lambda
调用时创建一个 T 类型对象
但返回类型是 shared_ptr<AIStrategy>

这里体现的是多态：

T 是具体子类
返回时统一提升成基类指针

所以工厂对外只暴露 AIStrategy，不暴露具体子类类型。

*/