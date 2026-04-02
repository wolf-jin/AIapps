#include "../include/handlers/ChatHistoryHandler.h"

void ChatHistoryHandler::handle(const http::HttpRequest& req, http::HttpResponse* resp)
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

        std::string sessionId;
        auto body = req.getBody();
        if (!body.empty()) {
            auto j = json::parse(body);
            if (j.contains("sessionId")) sessionId = j["sessionId"];
        }

        std::vector<std::tuple<int, std::string, long long>> messages;
        // std::vector<std::pair<std::string, long long>> messages;

        {
            std::shared_ptr<AIHelper> AIHelperPtr;
            std::lock_guard<std::mutex> lock(server_->mutexForChatInformation);

            auto& userSessions = server_->chatInformation[userId];

            if (userSessions.find(sessionId) == userSessions.end()) {

                userSessions.emplace( 
                    sessionId,
                    std::make_shared<AIHelper>()
                );
            }
            AIHelperPtr= userSessions[sessionId];
            messages= AIHelperPtr->GetMessages();
        }


        json successResp;
        successResp["success"] = true;
        successResp["history"] = json::array();

        for (size_t i = 0; i < messages.size(); ++i) {
            json msgJson;
            // 🌟 1. 抛弃 % 2，用 std::get<0> 获取绝对真实的 is_user 身份，并转成布尔值
            msgJson["is_user"] = (std::get<0>(messages[i]) == 1);
            // 🌟 2. 抛弃 .first，用 std::get<1> 获取真实的文本内容
            msgJson["content"] = std::get<1>(messages[i]);
            successResp["history"].push_back(msgJson);
        }

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









