#pragma once
#include "../../../../HttpServer/include/router/RouterHandler.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"

#include"../AIUtil/AISessionIdGenerator.h"
#include "../ChatServer.h"


// 说明它工作在 HTTP 接口层，负责处理一个具体 URL 的请求。
class ChatCreateAndSendHandler : public http::router::RouterHandler
{
public:
    explicit ChatCreateAndSendHandler(ChatServer* server) : server_(server) {}

    void handle(const http::HttpRequest& req, http::HttpResponse* resp) override;
private:

private:
    ChatServer* server_;
    // http::MysqlUtil     mysqlUtil_;
};
