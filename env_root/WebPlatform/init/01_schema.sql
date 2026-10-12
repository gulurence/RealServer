-- =============================================================================
-- WebPlatform 登录账号表 (mysql:8 首次启动自动执行)
-- =============================================================================
CREATE TABLE IF NOT EXISTS `users` (
    `id`        BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `username`  VARCHAR(64)  NOT NULL,
    `password`  VARCHAR(255) NOT NULL,
    `role`      VARCHAR(16)  NOT NULL DEFAULT 'viewer',
    `created_at` TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 初始管理员: admin / admin (bcrypt 哈希, 与 config.php 兜底一致)
INSERT INTO `users` (`username`, `password`, `role`) VALUES
    ('admin', '$2y$10$ByiVHyic9GTcn1LghUgjQ.Rj2TZ5f/iq9J.JSDoW9ND3MWfVAAEFy', 'admin')
ON DUPLICATE KEY UPDATE `username` = `username`;

-- ---------------------------------------------------------------------------
-- account — 游戏账户 (登录链路核心, 与 EnvRoot/Mysql/init/01_schema.sql 一致)
-- state 账户状态机: 0=Created 1=LoggedIn 2=RoleListed 3=RoleSelected 4=InGame
-- 热数据存 Redis (account:{id}), 此表为持久化兜底 (Redis miss 回源)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `account` (
    `id`         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `username`   VARCHAR(64)  NOT NULL,
    `password`   VARCHAR(255) NOT NULL,          -- password_hash() 输出
    `state`      TINYINT      NOT NULL DEFAULT 0,
    `created_at` TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- player — 角色/玩家档案 (选角后生成 player actor, 热数据存 Redis player:{id})
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `player` (
    `id`         BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `account_id` BIGINT UNSIGNED NOT NULL,
    `role_name`  VARCHAR(64)  NOT NULL,
    `level`      INT UNSIGNED NOT NULL DEFAULT 1,
    `job`        INT UNSIGNED NOT NULL DEFAULT 0,
    `state`      TINYINT      NOT NULL DEFAULT 0,
    `created_at` TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
