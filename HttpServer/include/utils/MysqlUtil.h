 #pragma once
 #include "db/DbConnectionPool.h"
 
#include <string>

namespace http
{

class MysqlUtil
{
public:
    static void init(const std::string& host, const std::string& user,
                    const std::string& password, const std::string& database,
                    size_t poolSize = 10)
    {
        http::db::DbConnectionPool::getInstance().init(
            host, user, password, database, poolSize);
    }

    template<typename... Args>
    sql::ResultSet* executeQuery(const std::string& sql, Args&&... args)
    {
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        return conn->executeQuery(sql, std::forward<Args>(args)...);
    }

    /*
    叫可变参数模板，std::forward 叫完美转发。这意味着你的 executeUpdate 函数就像个万能插座：不管你传 1 个参数、3 个参数，还是传左值、右值，
    它都能原封不动、连类型都不丢失地“透传”给底层的 conn->executeUpdate。
    这不仅避免了无意义的内存拷贝，还让防止 SQL 注入（预编译处理）变得极其方便。
    */
    template<typename... Args>
    static int executeUpdate(const std::string& sql, Args&&... args)
    {
        // 它避免了频繁创建和销毁数据库连接的高成本，通常能显著降低数据库访问开销，更适合高并发场景。！
        auto conn = http::db::DbConnectionPool::getInstance().getConnection();
        
        return conn->executeUpdate(sql, std::forward<Args>(args)...);
    }
};

} // namespace http
