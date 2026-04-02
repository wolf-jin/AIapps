#include"../include/AIUtil/AIHelper.h"
#include"../include/AIUtil/MQManager.h"
#include <stdexcept>
#include<chrono>

// 构造函数
// 构造时会设置默认策略。
// 也就是说，新建一个 AIHelper 后，它不是空白的，会先带一个默认模型策略。
AIHelper::AIHelper() {
    //默认使用阿里云大模型
    strategy = StrategyFactory::instance().create("1");
}

// 切换当前会话所使用的模型策略。
void AIHelper::setStrategy(std::shared_ptr<AIStrategy> strat) {
    strategy = strat;
}

/*
作用是：

把一条消息放进当前会话内存上下文
再把这条消息异步入库

所以它不是单纯“加到 vector 里”，而是：

内存更新 + 持久化投递

*/

// 添加一条用户消息
void AIHelper::addMessage(int userId, const std::string& userName, bool is_user, const std::string& userInput, std::string sessionId) {
    auto now = std::chrono::system_clock::now(); // 
    auto duration = now.time_since_epoch();
    // 生成当前时间戳， 转成毫秒时间戳
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(); 
    // 写如内存上下文，把当前消息先放进当前会话的 messages 里。

    // 使用 std::make_tuple 强绑定身份 (1代表用户, 0代表AI)
    messages.push_back(std::make_tuple(is_user ? 1 : 0, userInput, ms));

    // messages.push_back({ userInput, ms });


    //消息队列异步入库，也就是说，这条消息不只是进入当前会话上下文，还会被异步写入数据库。
    pushMessageToMysql(userId, userName, is_user, userInput, ms, sessionId);
}

/*
这是和 addMessage(...) 不同的另一个入口。

它只负责：把历史消息恢复回内存上下文

不做数据库写入。
给ChatServer::readDatabaseMessages(...)调用，用于服务启动时从数据库恢复历史消息。
这非常合理，因为它就是为“服务启动时从数据库恢复历史消息”准备的。

*/
// void AIHelper::restoreMessage(const std::string& userInput,long long ms) {
//     messages.push_back({ userInput,ms });
// }

// 🌟 修改 2：恢复消息时，把从数据库读到的 is_user 塞进去
void AIHelper::restoreMessage(int is_user, const std::string& userInput, long long ms) {
    messages.push_back(std::make_tuple(is_user, userInput, ms));
}





/*
这是整个类最重要的函数。

它负责完整聊天流程：

切换模型策略
拼消息上下文
调模型
解析结果
写回上下文
异步入库
返回最终答案

这个函数就是整条聊天主线的核心。

*/

// 🌟 修改 4：chat 函数里的临时 Prompt 也需要适配 tuple (注意看里面 push_back 的改动)
std::string AIHelper::chat(int userId, std::string userName, std::string sessionId, std::string userQuestion, std::string modelType) {

    
    // 前端传什么modelType，当前会话这次调用就切到对应的策略上
    setStrategy(StrategyFactory::instance().create(modelType));

    // 路线 A：普通模型
    if (false == strategy->isMCPModel) {

        // 把用户问题加入上下文，数据库异步入库
        addMessage(userId, userName, true, userQuestion, sessionId);
        // AIHelper 自己不关心不同模型平台的请求 JSON 长什么样。
        // 它把“如何根据上下文拼请求体”交给策略层去做。
        // 这就是策略模式的核心价值之一。
        json payload = strategy->buildRequest(this->messages);

        //执行请求
        // 这一步通过 curl 把请求发到外部模型平台。
        json response = executeCurl(payload);
        // 同样地，AIHelper 也不关心“外部模型平台的响应 JSON 长什么样”，它把“如何从响应里解析出答案”交给策略层去做。
        std::string answer = strategy->parseResponse(response);
        // 把AI回复加入上下文，数据库异步入库
        addMessage(userId, userName, false, answer, sessionId);
        return answer.empty() ? "[Error] 无法解析响应" : answer;
    }
    // 路线 B：MCP 模型（需要工具调用的模型）
    AIConfig config;
    config.loadFromFile("../AIApps/ChatServer/resource/config.json");

    // 构造第一次增强提示
    std::string tempUserQuestion =config.buildPrompt(userQuestion);
    std::cout << "tempUserQuestion is " << tempUserQuestion << std::endl;


    // 适配 tuple：默认临时提示词算作用户发的 (1)
    messages.push_back(std::make_tuple(1, tempUserQuestion, 0));

    // messages.push_back({ tempUserQuestion, 0 });

    // 第一次调用AI，判断是否需要工具调用
    // 这轮提示只是为了本轮决策，不想污染正式会话历史。
    json firstReq = strategy->buildRequest(this->messages);
    json firstResp = executeCurl(firstReq);
    std::string aiResult = strategy->parseResponse(firstResp);
    // 用完立即移除提示词
    messages.pop_back();

    /*
    也就是说，第一轮模型返回的内容不是直接给用户看的，而是先被系统解析成：
    是否是工具调用
    调哪个工具
    工具参数是什么
    */
    std::cout << "aiResult is " << aiResult << std::endl;
    // 解析AI响应（是否工具调用）
    AIToolCall call = config.parseAIResponse(aiResult);

    // 情况1：AI 不调用工具
    /*
    如果不需要工具，就退化成普通一问一答流程：
    把用户原问题写入上下文
    把模型回答写入上下文
    返回结果
    */
    if (!call.isToolCall) {
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, aiResult, sessionId);

        std::cout << "No tools required" << std::endl;
        return aiResult;
    }

    // 情况 2：AI 要调用工具
    json toolResult;
    AIToolRegistry registry;

    try {
        //这里是真正执行工具调用
        /*
        MCP 流程本质上是：模型先输出一个“调用工具”的意图，再由系统执行工具。
        */
        toolResult = registry.invoke(call.toolName, call.args);
        std::cout << "Tool call success" << std::endl;
    }
    /*
    这一步说明工具调用异常也不会让整个服务崩掉，而是：

    给用户返回一个错误结果
    同时把这次问答也记进会话上下文
    
    
    */
    catch (const std::exception& e) {
        //大多数情况都不会走这里
        std::string err = "[工具调用失败] " + std::string(e.what());
        addMessage(userId, userName, true, userQuestion, sessionId);
        addMessage(userId, userName, false, err, sessionId);

        std::cout << "Tool call failed\n" << std::endl << std::string(e.what());
        return err;
    }

    // 第二次调用AI
    // 用同样的 prompt_template，但说明工具执行过
    /*
    这一步说明工具调用异常也不会让整个服务崩掉，而是：

    给用户返回一个错误结果
    同时把这次问答也记进会话上下文
    
    */
    std::string secondPrompt = config.buildToolResultPrompt(userQuestion, call.toolName, call.args, toolResult);
    
    std::cout << "secondPrompt is " << secondPrompt << std::endl;

    // 适配 tuple
    messages.push_back(std::make_tuple(1, secondPrompt, 0));
    
    // messages.push_back({ secondPrompt, 0 });

    json secondReq = strategy->buildRequest(messages);
    json secondResp = executeCurl(secondReq);
    std::string finalAnswer = strategy->parseResponse(secondResp);
    //删除包含提示词的信息
    messages.pop_back();

    std::cout << "finalAnswer is " << finalAnswer << std::endl;
    /*
    注意这里仍然只把：

    用户原问题
    最终答案

    写入正式上下文。
    而中间那些临时 prompt 没有进入正式历史。

    这是一个很好的设计，否则上下文里会塞满系统内部提示词。
    */
    addMessage(userId, userName, true, userQuestion, sessionId);
    addMessage(userId, userName, false, finalAnswer, sessionId);
    return finalAnswer;

}

// 发送自定义请求体
// 允许外部直接传一个自定义 payload，让 AIHelper 帮你发请求。
// 它不是主聊天链路核心函数，更像一个辅助接口。
json AIHelper::request(const json& payload) {
    return executeCurl(payload);
}
// 作用是拿当前会话的内存消息列表。

// 🌟 修改 3：GetMessages 返回升级后的 tuple
std::vector<std::tuple<int, std::string, long long>> AIHelper::GetMessages() {
    return this->messages;
}

// std::vector<std::pair<std::string, long long>> AIHelper::GetMessages() {
//     return this->messages;
// }


// 内部方法：执行 curl 请求
json AIHelper::executeCurl(const json& payload) {
    // 初始化 curl， 拿到一个 curl 句柄。
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }
    /*
    这行会把 API Key 直接打印到日志。

    这在调试时有用，但正式环境非常不安全。
    因为日志一旦泄露，密钥就暴露了。

    所以这行在生产环境里应该删掉。
    */
    std::cout<<"test "<< strategy->getApiUrl().c_str()<<' '<< strategy->getApiKey()<<std::endl;

    std::string readBuffer;
    struct curl_slist* headers = nullptr;
    // 构造 Bearer 鉴权头
    std::string authHeader = "Authorization: Bearer " + strategy->getApiKey();
    //设置请求头
    //标准 JSON POST 请求。
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    //把请求体序列化成字符串
    std::string payloadStr = payload.dump();

    /*
    设置 curl 参数

    包括：

    CURLOPT_URL
    CURLOPT_HTTPHEADER
    CURLOPT_POSTFIELDS
    CURLOPT_WRITEFUNCTION
    CURLOPT_WRITEDATA

    这一步就是把请求真正配置好。
    
    */
    curl_easy_setopt(curl, CURLOPT_URL, strategy->getApiUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    // 这是网络调用的真正执行点，发请求
    CURLcode res = curl_easy_perform(curl);
    // 清理资源，
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // 解析返回 JSON
    // 
    if (res != CURLE_OK) {
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    }

    try {
        return json::parse(readBuffer);
    }
    catch (...) {
        throw std::runtime_error("Failed to parse JSON response: " + readBuffer);
    }
}





// curl 回调函数，把返回的数据写到 string buffer
// 把服务器返回的数据不断追加到 readBuffer 里。
size_t AIHelper::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

/*
这个函数是为了拼 SQL 时做最基本的转义。

它能处理一些常见字符，比如：

反斜杠
单引号
双引号
换行、回车、tab

它的方向没错，但你要知道：

这不是最强健的数据库防注入方案。

更稳的做法通常是 prepared statement。
不过在你这个项目里，因为后面只是把 SQL 字符串丢到消息队列里再执行，所以作者用了一个轻量手工转义方案。

*/
std::string AIHelper::escapeString(const std::string& input) {
    std::string output;
    output.reserve(input.size() * 2);
    for (char c : input) {
        switch (c) {
            case '\\': output += "\\\\"; break;
            case '\'': output += "\\\'"; break;
            case '\"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:   output += c; break;
        }
    }
    return output;
}


void AIHelper::pushMessageToMysql(int userId, const std::string& userName, bool is_user, const std::string& userInput,long long ms, std::string sessionId) {
    
    std::string safeUserName = escapeString(userName);
    std::string safeUserInput = escapeString(userInput);

    // std::string sql = "INSERT INTO chat_message (id, username, session_id, is_user, content, ts) VALUES ("
    //     + std::to_string(userId) + ", "
    //     + "'" + safeUserName + "', "
    //     + sessionId + ", "
    //     + std::to_string(is_user ? 1 : 0) + ", "
    //     + "'" + safeUserInput + "', "
    //     + std::to_string(ms) + ")";

    std::string sql = "INSERT INTO chat_message (username, session_id, is_user, content, ts) VALUES ('"
        + safeUserName + "', '"
        + sessionId + "', "
        + std::to_string(is_user ? 1 : 0) + ", '"
        + safeUserInput + "', "
        + std::to_string(ms) + ")";



    MQManager::instance().publish("sql_queue", sql);
}

