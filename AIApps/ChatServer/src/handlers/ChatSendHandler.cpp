#include "../include/handlers/ChatSendHandler.h"

/*
获取当前 session
检查是否已登录
从 session 里取 userId 和 username
从请求体里解析 question、sessionId、modelType
根据 userId + sessionId 找到对应的 AIHelper
如果没有这个 AIHelper，就创建一个
调 AIHelper::chat(...)
把 AI 回复返回给前端

你可以看出，它和 ChatCreateAndSendHandler 的区别主要集中在：

这里不生成 sessionId
这里不返回 sessionId
这里默认前端已经知道要操作哪个会话




*/
void ChatSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {
        // 第一部分：登录态校验
        auto session = server_->getSessionManager()->getSession(req, resp);
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        if (session->getValue("isLoggedIn") != "true")
        {

            json errorResp;
            errorResp["status"] = "error";
            errorResp["message"] = "Unauthorized";
            std::string errorBody = errorResp.dump(4);

            server_->packageResp(req.getVersion(), http::HttpResponse::k401Unauthorized,
                "Unauthorized", true, "application/json", errorBody.size(),
                errorBody, resp);
            return;
        }

        // 第二部分：从 session 里取身份信息
        // 用户身份以后端 session 为准，不信任前端请求体里的身份信息。
        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion;
        std::string modelType;
        std::string sessionId;

        // 第三部分：从请求体里解析用户问题、继续聊天的会话 ID 和模型类型
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];
            // 依赖前端提供一个已有会话ID
            if (j.contains("sessionId")) sessionId = j["sessionId"];

            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }

        // 第四部分：根据 userId + sessionId 找到对应的 AIHelper，如果没有这个 AIHelper，就创建一个
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);
            // userId -> sessionId -> AIHelper
            auto& userSessions = server_->chatInformation[userId];

            if (userSessions.find(sessionId) == userSessions.end()) {

                userSessions.emplace( 
                    sessionId,
                    std::make_shared<AIHelper>()
                );
            }
            AIHelperPtr= userSessions[sessionId];
        }
        
        /*
        它把以下信息交给 AIHelper：

        用户 ID
        用户名
        当前会话 ID
        用户问题
        模型类型

        然后 AIHelper 内部会做：

        选择策略
        拼请求 JSON
        调 curl 请求模型
        解析响应
        记录上下文
        异步写库

        所以这里再一次体现出清晰的分层：

        ChatSendHandler

        负责：

        接 HTTP 请求
        取参数
        做权限校验
        找到正确的 AIHelper
        AIHelper

        负责：

        真正的 AI 聊天业务流程
        
        
        */
       // 构造成功响应
        std::string aiInformation=AIHelperPtr->chat(userId, username, sessionId, userQuestion, modelType);
        json successResp;
        successResp["success"] = true;
        successResp["Information"] = aiInformation;
        // 这里没有返回 sessionId，因为前端已经知道当前会话的sessionId了，这里只负责“继续聊天”，不负责“生成新会话”
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    catch (const std::exception& e)
    {

        json failureResp;
        failureResp["status"] = "error";
        failureResp["message"] = e.what();
        std::string failureBody = failureResp.dump(4);
        resp->setStatusLine(req.getVersion(), http::HttpResponse::k400BadRequest, "Bad Request");
        resp->setCloseConnection(true);
        resp->setContentType("application/json");
        resp->setContentLength(failureBody.size());
        resp->setBody(failureBody);
    }
}









