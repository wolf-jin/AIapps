#include "../include/handlers/ChatSpeechHandler.h"


void ChatSpeechHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
{
    try
    {

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

        int userId = std::stoi(session->getValue("userId"));
        std::string username = session->getValue("username");


        std::string text;

        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("text")) text = j["text"];
        }
        // =========================================================
        // 🌟 架构师的微创手术：C++11 Magic Statics (静态局部初始化)
        // =========================================================
        // 1. 静态读取环境变量：只在服务器第一次接收到语音请求时查一次操作系统，以后直接从内存读！
        static const char* secretEnv = std::getenv("BAIDU_CLIENT_SECRET");
        static const char* idEnv = std::getenv("BAIDU_CLIENT_ID");

        // const char* secretEnv = std::getenv("BAIDU_CLIENT_SECRET");
        // const char* idEnv = std::getenv("BAIDU_CLIENT_ID");

        if (!secretEnv) throw std::runtime_error("BAIDU_CLIENT_SECRET not found!");
        if (!idEnv) throw std::runtime_error("BAIDU_CLIENT_ID not found!");

        

        

        // std::string clientSecret(secretEnv);
        // std::string clientId(idEnv);

        // 2. 核心大招：静态实例化 Processor！
        // C++11 标准强制保证了局部 static 变量的初始化是【绝对线程安全】的。
        // 不管有多少个 Worker 线程同时冲进这个接口，这行代码只会被执行一次！
        // Token 只会去百度请求一次！后续所有请求全服共享这个 Processor！

        static AISpeechProcessor speechProcessor{std::string(idEnv), std::string(secretEnv)};

        // AISpeechProcessor speechProcessor(clientId, clientSecret);
        
        // 直接复用静态实例发起合成请求
        std::string speechUrl = speechProcessor.synthesize(text,
                                                           "mp3-16k", 
                                                           "zh",  
                                                            5, 
                                                            5, 
                                                            5 );  

        json successResp;
        successResp["success"] = true;
        successResp["url"] = speechUrl;
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









