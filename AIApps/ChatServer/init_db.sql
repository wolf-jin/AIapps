-- ========================================================
-- AI Chat Server 数据库初始化脚本
-- 描述：创建基础数据库、数据表，并插入默认测试账号
-- ========================================================

-- 1. 创建数据库（如果不存在的话），并强制使用支持 Emoji 的 utf8mb4 字符集
CREATE DATABASE IF NOT EXISTS `ChatHttpServer` 
    DEFAULT CHARACTER SET utf8mb4 
    COLLATE utf8mb4_unicode_ci;

-- 切换到该数据库
USE `ChatHttpServer`;

-- ========================================================

-- 2. 创建用户表 (账号防线)
-- 注意：使用了 IF NOT EXISTS，这样即使重复执行脚本，也不会清空你现有的用户数据
CREATE TABLE IF NOT EXISTS `users` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '用户唯一ID',
  `username` varchar(255) NOT NULL COMMENT '登录账号',
  `password` varchar(255) NOT NULL COMMENT '登录密码',
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_username` (`username`) COMMENT '保证用户名不重复'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户账号表';

-- 插入默认测试账号 (忽略主键冲突，如果已经有了 jrj 就不会报错)
INSERT IGNORE INTO `users` (`username`, `password`) VALUES ('jrj', '123456');

-- ========================================================

-- 3. 创建聊天记录表 (聊天大动脉)
CREATE TABLE IF NOT EXISTS `chat_message` (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '消息唯一ID',
  `username` varchar(255) DEFAULT NULL COMMENT '所属用户',
  `session_id` varchar(255) DEFAULT NULL COMMENT '对话批次号',
  `is_user` tinyint DEFAULT NULL COMMENT '1为用户发的消息，0为AI回复',
  `content` text COMMENT '消息具体内容（支持长文本和Emoji）',
  `ts` bigint DEFAULT NULL COMMENT '时间戳（用于排序）',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='聊天历史记录表';

-- ========================================================
-- 初始化脚本结束
-- ========================================================