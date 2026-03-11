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
    try
    {
        test_connection_();
        spdlog::info("Database connection pool initialized with {} connections.", pool_size_);
    }
    catch (const std::exception &e)
    {
        spdlog::critical("Failed to initialize database connection pool: {}", e.what());
        throw;
    }
}

bool DatabasePool::test_connection() const
{
    try
    {
        pqxx::connection test_conn{connection_string_};
        return test_conn.is_open();
    }
    catch (const std::exception &e)
    {
        spdlog::error("Connection test failed: {}", e.what());
        return false;
    }
}

void DatabasePool::test_connection_()
{
    if (!test_connection())
    {
        throw std::runtime_error("Database connection test failed");
    }
}

std::unique_ptr<pqxx::connection> DatabasePool::get_connection() const
{
    return connection_queue_.get_connection();
}

void DatabasePool::return_connection(std::unique_ptr<pqxx::connection> conn) const
{
    connection_queue_.return_connection(std::move(conn));
}

size_t DatabasePool::pool_size() const
{
    return pool_size_;
}

size_t DatabasePool::available_count() const
{
    return connection_queue_.available_count();
}
