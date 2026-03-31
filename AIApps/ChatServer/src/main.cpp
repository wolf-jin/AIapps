#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include"../include/ChatServer.h"

const std::string RABBITMQ_HOST = "localhost";
const std::string QUEUE_NAME = "sql_queue";
const int THREAD_NUM = 2; // 表示 RabbitMQ 消费线程池里有 2 个 worker 线程。

// 表示 RabbitMQ 消费线程池里有 2 个 worker 线程。
// worker 线程取出一条消息后，就会把这条 SQL 交给 executeMysql
void executeMysql(const std::string& sql) {
    // http::MysqlUtil mysqlUtil_;
    // mysqlUtil_.executeUpdate(sql);
    http::MysqlUtil::executeUpdate(sql);
}


int main(int argc, char* argv[]) {
	LOG_INFO << "pid = " << getpid();
    // 设置服务名和默认端口
	std::string serverName = "ChatServer";
	int port = 8080;
    // 
    int opt;
    const char* str = "p:"; // -p: 选项后面跟一个参数，表示端口号
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            port = atoi(optarg); // 将字符串端口号转换为整数
            break;
        }
        default:
            break;
        }
    }
    // 以后只保留 WARN 及以上级别的日志。
    muduo::Logger::setLogLevel(muduo::Logger::WARN);
    // 构造一个聊天服务对象。
    /*
    初始化 MySQL
    初始化 Session
    初始化 Middleware
    初始化 Router
    
    */
    ChatServer server(port, serverName);
    // 设置 ChatServer 自己的线程数为 4。
    server.setThreadNum(4);
    /*
    整个程序现在有两套线程
    第一套：服务线程
    第二套：RabbitMQ 消费线程池里的 worker 线程

    4 个服务线程处理网络请求
    2 个后台线程处理异步 SQL
    
    
    */

    // 启动前休眠2秒
    std::this_thread::sleep_for(std::chrono::seconds(2));
    /*
    调用 readDataFromMySQL() 从 MySQL 里把历史聊天记录读出来，放在内存里。这样用户一登录，就能看到之前的聊天记录了。
    */
    server.initChatMessage();    

    /*
    帮我创建一个消费 sql_queue 的 RabbitMQ 线程池，开 2 个 worker，收到消息后就执行 executeMysql(sql)。”
    */
    RabbitMQThreadPool pool(RABBITMQ_HOST, QUEUE_NAME, THREAD_NUM, executeMysql);
    // 先启动了 RabbitMQ 的消费者线程
    /*
    建立 RabbitMQ channel
    声明/消费队列
    循环从队列取 SQL
    调 executeMysql(...)
    ack 消息
    */
    pool.start();
    // 最后才启动网络服务,正式开始监听端口，对外提供 HTTP 服务。
    // 最终 httpServer_.start() 
    /*
    到这一步，路由已经注册好了
    Session 已经初始化好了
    聊天历史已经恢复好了
    RabbitMQ 消费线程也已经跑起来了

    所以这一步之后，整个系统才算真正进入“可服务”状态。
    */
    server.start();
}
