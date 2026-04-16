#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <chrono>
#include <functional>

class MQManager {
public:
    static MQManager& instance() {
        static MQManager mgr;
        return mgr;
    }

    void publish(const std::string& queue, const std::string& msg);

private:

    /*
    为什么不直接用一个 vector<Channel>，
    而是要专门搞一个 MQConn 结构体，
    把 channel 和 mutex 绑在一起？
    
    真相： 因为 RabbitMQ 的 Channel 极其脆弱（非线程安全）。
    原作者极其聪明地把**“资源”和“锁”**强行捆绑在了一起。
    这就相当于给每一条传送带（Channel）都配了一个专属的私人保安（mtx）。谁想用这条传送带，必须先拿到这个保安的钥匙！这就是咱们上一回看到的 
    lock_guard<mutex> lock(conn->mtx) 的由来。
    */
    struct MQConn {
        AmqpClient::Channel::ptr_t channel;
        std::mutex mtx;
    };

    MQManager(size_t poolSize = 5);

    MQManager(const MQManager&) = delete;
    MQManager& operator=(const MQManager&) = delete;

    std::vector<std::shared_ptr<MQConn>> pool_;
    size_t poolSize_;
    std::atomic<size_t> counter_;
};

class RabbitMQThreadPool {
public:
    using HandlerFunc = std::function<void(const std::string&)>;

    RabbitMQThreadPool(const std::string& host,
        const std::string& queue,
        int thread_num,
        HandlerFunc handler)
        : stop_(false),
        rabbitmq_host_(host),
        queue_name_(queue),
        thread_num_(thread_num),
        handler_(handler) {}

    void start();
    void shutdown();

    ~RabbitMQThreadPool() {
        shutdown();
    }

private:
    void worker(int id);

private:
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_;
    std::string queue_name_;
    int thread_num_;
    std::string rabbitmq_host_;
    HandlerFunc handler_;
};
