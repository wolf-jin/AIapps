#include"../include/AIUtil/AIFactory.h"

/*
这是 C++ 里最经典的局部静态单例写法。

含义是：

第一次调用时创建 factory
之后每次都返回同一个对象

所以工厂是全局唯一的。


*/
StrategyFactory& StrategyFactory::instance() {
    static StrategyFactory factory;
    return factory;
}


/*
它就是把：

名字 name
创建器 creator

放进哈希表。

为什么用了 std::move

因为 creator 是一个 std::function，通常 move 进去更自然，避免额外复制。

*/

void StrategyFactory::registerStrategy(const std::string& name, Creator creator) {
    creators[name] = std::move(creator);
}



std::shared_ptr<AIStrategy> StrategyFactory::create(const std::string& name) {
    // 也就是去那张“名字 → 创建器”的表里找。
    auto it = creators.find(name);
    if (it == creators.end()) {
        // 如果找不到，就抛异常。
        throw std::runtime_error("Unknown strategy: " + name);
    }
    // 注意这里 it->second 不是对象，而是一个函数。
    // 加 () 之后，才是调用这个函数去创建策略实例。
    /*
    */
    return it->second();
}
/*
creators["1"] 里存的是“怎么创建阿里云策略”的函数
create("1") 时，才真正 new 出一个阿里云策略对象
*/