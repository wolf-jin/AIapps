#include "../include/handlers/ChatCreateAndSendHandler.h"


void ChatCreateAndSendHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {

        // 从 SessionManager 里取当前请求的 session
        auto session = server_->getSessionManager()->getSession(req, resp);
        // 读取 session 里的 isLoggedIn
        LOG_INFO << "session->getValue(\"isLoggedIn\") = " << session->getValue("isLoggedIn");
        // 如果不是 "true"，直接返回 401 Unauthorized
        /*
        说明这个接口是必须登录后才能调用的。
        所以前端如果没带 cookie/session，哪怕请求体正确，也会直接失败。
        */
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

        // 第二部分：从 session 里取用户身份
        /*
        为什么从 session 里拿，而不是从前端请求体里拿
        因为前端传来的 userId 可以伪造，而服务端 session 里的 userId 才更可信。
        身份信息以后端登录态为准，不信任前端自报。
        */
        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");

        std::string userQuestion;
        std::string modelType;

        // 这里表示这个接口期望前端传的是 JSON。
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("question")) userQuestion = j["question"];

            // 前端希望走哪条模型策略，如果没传 modelType，默认就是：1
            modelType = j.contains("modelType") ? j["modelType"].get<std::string>() : "1";
        }
        
        /*
        这里没读取 sessionId

        这是它和 ChatSendHandler 最本质的区别之一。
        因为这个接口要自己创建新 session，所以前端不需要传 sessionId。
        */
        AISessionIdGenerator generator;
        // 生成一个新的 sessionId，代表一个新的会话。
        std::string sessionId = generator.generate();
        std::cout<<"sessionId: "<<sessionId<<std::endl;

        // 为新会话创建 AIHelper
        // 新建会话，必须同时维护两张表：chatInformation 和 sessionsIdsMap
        std::shared_ptr<AIHelper> AIHelperPtr;
        {
            // chatInformation 是共享状态
            // userId -> sessionId -> AIHelper
            // 多个线程可能同时访问它，所以要加锁。
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);
            // 如果这个 userId 之前没有记录，operator[] 会自动创建一个空 map。
            auto& userSessions = server_->chatInformation[userId];
            // 如果这个 sessionId 之前没有记录，说明这是个新会话，就创建一个新的 AIHelper。
            if (userSessions.find(sessionId) == userSessions.end()) {

                userSessions.emplace(sessionId, std::make_shared<AIHelper>());
                // 如果只创建 AIHelper 而不更新 sessionsIdsMap，那前端左侧会话列表就看不到这个新会话。
                server_->sessionsIdsMap[userId].push_back(sessionId);
            }
            // 取出共享指针，离开代码段，锁就会自动释放了。
            AIHelperPtr= userSessions[sessionId];

        }
        // 因为后面 AIHelper::chat(...) 可能很慢：
        /*
        要构造上下文
        要发网络请求
        要等模型返回

        如果一直拿着 mutexForChatInformation，别的请求就会被堵住。
        锁只保护map的访问，不保护耗时操作。
        */
        /*
        把以下信息交给 AIHelper：

        谁在聊天：userId
        用户名：username
        这是哪个会话：sessionId
        用户问题是什么：userQuestion
        用哪种模型策略：modelType
        
        然后 AIHelper 内部会去做：

        选择策略
        构造请求
        调 curl
        解析回复
        记录消息
        异步落库
        
        */
        std::string aiInformation=AIHelperPtr->chat(userId, username,sessionId, userQuestion, modelType);
        json successResp;
        successResp["success"] = true; // 调用成功
        successResp["Information"] = aiInformation; // AI 第一条的回复
        successResp["sessionId"] = sessionId;   // 关键，因为这是新会话，前端根本不知道它的sessionId

        /*
        所以这个接口返回后，前端就应该：

        把这条回复显示出来
        把这个 sessionId 保存起来
        以后继续聊天时改走 /chat/send

        也就是说，这个返回值设计是完全符合“新建会话”业务语义的。
        */
        
        std::string successBody = successResp.dump(4);

        resp->setStatusLine(req.getVersion(), http::HttpResponse::k200Ok, "OK");
        resp->setCloseConnection(false);
        resp->setContentType("application/json");
        resp->setContentLength(successBody.size());
        resp->setBody(successBody);
        return;
    }
    //  异常捕获
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









