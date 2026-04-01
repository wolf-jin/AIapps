#include"../include/AIUtil/MQManager.h"

// ------------------- MQManager -------------------
MQManager::MQManager(size_t poolSize)
    : poolSize_(poolSize), counter_(0) {
    for (size_t i = 0; i < poolSize_; ++i) {
        auto conn = std::make_shared<MQConn>();
        //  Create
        conn->channel = AmqpClient::Channel::Create("localhost", 5672, "guest", "guest", "/");

        pool_.push_back(conn);
    }
}

void MQManager::publish(const std::string& queue, const std::string& msg) {
    size_t index = counter_.fetch_add(1) % poolSize_;
    auto& conn = pool_[index];

    std::lock_guard<std::mutex> lock(conn->mtx);
    // 把普通字符串包装成 RabbitMQ 能识别的“消息对象”。
    // 
    auto message = AmqpClient::BasicMessage::Create(msg);
    // 通过这个 channel，把 message 发出去。
    // "" 代表默认交换机，用默认交换机时，可以直接按队列名投递，第二个把传队列名
    // 在默认交换机模式下，这基本就等价于：

    // “我要把消息送到这个队列”。
    conn->channel->BasicPublish("", queue, message);
}

// ------------------- RabbitMQThreadPool -------------------

void RabbitMQThreadPool::start() {
    for (int i = 0; i < thread_num_; ++i) {
        workers_.emplace_back(&RabbitMQThreadPool::worker, this, i);
    }
}

void RabbitMQThreadPool::shutdown() {
    stop_ = true;
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void RabbitMQThreadPool::worker(int id) {
    try {
        // Each thread has its own independent channel
        auto channel = AmqpClient::Channel::Create(rabbitmq_host_, 5672, "guest", "guest", "/");
        // set exclusive
        // “我要开始消费这个队列了，先确认它已经建好了
        channel->DeclareQueue(queue_name_, false, true, false, false);
        // Prevent channel error: 403: AMQP_BASIC_CONSUME_METHOD caused: ACCESS_REFUSED - queue 
        // 'sql_queue' in vhost '/' in exclusive use
        // std::string consumer_tag = channel->BasicConsume(queue_name_, "");
        // 告诉 RabbitMQ：我要开始消费 queue_name_ 这个队列了，RabbitMQ 会给我返回一个 consumer_tag，代表我的这个消费这实例的编号/句柄
        std::string consumer_tag = channel->BasicConsume(queue_name_, "", true, false, false);
        // 控制这个消费者一次不要囤太多未处理消息。
        channel->BasicQos(consumer_tag, 1); 

        while (!stop_) {
            AmqpClient::Envelope::ptr_t env;
            // 真正从队列里尝试取一条消息。
            // 
            bool ok = channel->BasicConsumeMessage(consumer_tag, env, 500); // 500ms 
            if (ok && env) {
                // 这一步就是把 RabbitMQ 消息包裹里的正文取出来。
                // 一条 SQL 字符串
                std::string msg = env->Message()->Body();
                // using HandlerFunc = std::function<void(const std::string&)>;
                // 也就是说，它可以接受任何“处理字符串消息”的函数
                // main.cpp 里我们传给它的就是 executeMysql, 所以这里等价于executeMysql(msg);
                // 拿到一条SQL，然后执行它
                handler_(msg);     
                // 告诉 RabbitMQ，这条消息我处理完了，你可以从队列里把它丢掉了。     
                channel->BasicAck(env); 
            }
        }
        // 取消当前这个消费者对队列的订阅。
        channel->BasicCancel(consumer_tag);
    }
    catch (const std::exception& e) {
        std::cerr << "Thread " << id << " exception: " << e.what() << std::endl;
    }
}
