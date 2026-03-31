#pragma once

#include <atomic>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>


#include "../../../HttpServer/include/http/HttpServer.h"
#include "../../../HttpServer/include/utils/MysqlUtil.h"
#include "../../../HttpServer/include/utils/FileUtil.h"
#include "../../../HttpServer/include/utils/JsonUtil.h"
#include"AIUtil/AISpeechProcessor.h"
#include"AIUtil/AIHelper.h"
#include"AIUtil/ImageRecognizer.h"
#include"AIUtil/base64.h"
#include"AIUtil/MQManager.h"


class ChatLoginHandler;
class ChatRegisterHandler;
class ChatLogoutHandler;
class ChatHandler;
class ChatEntryHandler;
class ChatSendHandler;
class ChatHistoryHandler;

class AIMenuHandler;
class AIUploadHandler;
class AIUploadSendHandler;


class ChatCreateAndSendHandler;
class ChatSessionsHandler;
class ChatSpeechHandler;

class ChatServer {
public:
	// 创建服务并做基础初始化
	ChatServer(int port,
		const std::string& name,
		muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort);
	// 创建服务并做基础初始化
	void setThreadNum(int numThreads);
	// 启动网络服务
	void start();
	// 从数据库恢复历史聊天状态
	void initChatMessage();
private:
	// 这些类可以访问ChatServer的私有成员
	friend class ChatLoginHandler;
	friend class ChatRegisterHandler;
	friend  ChatLogoutHandler;
	friend class ChatHandler;
	friend class ChatEntryHandler;
	friend class ChatSendHandler;
	friend class AIMenuHandler;
	friend class AIUploadHandler;
	friend class AIUploadSendHandler;
	friend class ChatHistoryHandler;

	friend class ChatCreateAndSendHandler;
	friend class ChatSessionsHandler;
	friend class ChatSpeechHandler;

private:
	void initialize();
	void initializeSession();
	void initializeRouter();
	void initializeMiddleware();
	

	void readDataFromMySQL();

	void packageResp(const std::string& version, http::HttpResponse::HttpStatusCode statusCode,
		const std::string& statusMsg, bool close, const std::string& contentType,
		int contentLen, const std::string& body, http::HttpResponse* resp);

	void setSessionManager(std::unique_ptr<http::session::SessionManager> manager)
	{
		httpServer_.setSessionManager(std::move(manager));
	}
	http::session::SessionManager* getSessionManager() const
	{
		return httpServer_.getSessionManager();
	}
	// ChatServer 不是自己实现网络框架，而是组合了一个底层的HttpServer
	http::HttpServer	httpServer_;
	// 数据库工具对象。
	http::MysqlUtil		mysqlUtil_;
	// 表示 userId -> 是否在线
	std::unordered_map<int, bool>	onlineUsers_;
	std::mutex	mutexForOnlineUsers_;

	

	
	// userId -> sessionId -> AIHelper
	/*
	每个用户可以有多个会话
	每个会话对应一个 AIHelper 对象
	AIHelper 里面保存那个会话的的聊天的上下文
	*/
	std::unordered_map<int, std::unordered_map<std::string,std::shared_ptr<AIHelper> > > chatInformation;
	std::mutex	mutexForChatInformation;
	// userId -> ImageRecognizer
	// 负责缓存图像识别器对象，也就是说，图像识别不是每次请求都重新加载模型，而是按用户缓存一个识别器。
	std::unordered_map<int, std::shared_ptr<ImageRecognizer> > ImageRecognizerMap;
	std::mutex	mutexForImageRecognizerMap;
	// userId -> [sessionId1, sessionId2, ...]
	// 它的作用不是保存聊天内容，而是：给前端提供“会话列表”
	std::unordered_map<int,std::vector<std::string> > sessionsIdsMap;
	std::mutex mutexForSessionsId;

};

