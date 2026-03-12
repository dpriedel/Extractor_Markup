#/* =====================================================================================
 *
 * Filename:  DatabasePool.cpp
 *
 * Description:  Class which provides a pool of database connections.
 *
 * Version:  1.0
 * Created:  2026-03-11 16:01:57
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> and Qwen3.5
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#include "DatabasePool.h"
#include <spdlog/spdlog.h>

DatabasePool::DatabasePool(const std::string &connection_string, size_t pool_size)
    : connection_string_{connection_string}, pool_size_{pool_size}, connection_queue_{connection_string, pool_size}
{

    if (!test_connection())
    {
        spdlog::critical("Failed to initialize database connection pool.");
        throw std::runtime_error("Database connection test failed");
    }
    spdlog::info("Database connection pool initialized with {} connections.", pool_size_);
}

PooledConnection DatabasePool::get_connection()
{
    return connection_queue_.get_connection();
}

bool DatabasePool::test_connection() const
{
    return connection_queue_.test_connection();
}

size_t DatabasePool::available_count() const
{
    return connection_queue_.available_count();
}
