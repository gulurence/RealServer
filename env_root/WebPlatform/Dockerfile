# =============================================================================
# WebPlatform — PHP 8.2 Apache + pdo_mysql + redis 扩展
# =============================================================================
FROM php:8.2-apache

# 装 PECL 构建工具 + redis 扩展 + pdo_mysql
RUN apt-get update \
    && apt-get install -y --no-install-recommends $PHPIZE_DEPS \
    && pecl install redis \
    && docker-php-ext-enable redis \
    && docker-php-ext-install pdo_mysql \
    && apt-get purge -y --auto-remove $PHPIZE_DEPS \
    && rm -rf /var/lib/apt/lists/*

# 允许 Apache 读取挂载的 www 目录
RUN chown -R www-data:www-data /var/www/html
