#/* =====================================================================================
 *
 * Filename:  ConnectionQueue.cpp
 *
 * Description:  Class which provides a Queue for managing database connections.
 *
 * Version:  1.0
 * Created:  2026-03-11 15:57:33
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> and Qwen3.5
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#include "ConnectionQueue.h"

#include "ConnectionQueue.h"
#include <spdlog/spdlog.h>

ConnectionQueue::ConnectionQueue(const std::string &connection_string, size_t max_size)
    : connection_string_(connection_string), max_size_(max_size)
{
    // Pre-populate the queue with connections
    for (size_t i = 0; i < max_size_; ++i)
    {
        available_.push(std::make_unique<pqxx::connection>(connection_string_));
    }
    spdlog::info("Connection queue initialized with {} connections.", max_size_);
}

std::unique_ptr<pqxx::connection> ConnectionQueue::get_connection()
{
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !available_.empty(); });

    auto conn = std::move(available_.front());
    available_.pop();
    lock.unlock();

    // Verify connection is still valid
    if (!conn || !conn->is_open())
    {
        spdlog::warn("Connection was closed, creating new one");
        conn = std::make_unique<pqxx::connection>(connection_string_);
    }
    else
    {
        // Test connection health with a quick query
        try
        {
            pqxx::nontransaction test_trxn{*conn};
            test_trxn.exec("SELECT 1");
            spdlog::trace("Connection health check passed");
        }
        catch (const pqxx::sql_error &e)
        {
            spdlog::warn("Connection health check failed: {}, creating new connection", e.what());
            conn = std::make_unique<pqxx::connection>(connection_string_);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Connection health check failed: {}, creating new connection", e.what());
            conn = std::make_unique<pqxx::connection>(connection_string_);
        }
    }

    return conn;
}

void ConnectionQueue::return_connection(std::unique_ptr<pqxx::connection> conn)
{
    std::unique_lock<std::mutex> lock(mutex_);
    available_.push(std::move(conn));
    lock.unlock();
    condition_.notify_one();
}

size_t ConnectionQueue::available_count() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return available_.size();
}

bool ConnectionQueue::test_connection() const
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

void ConnectionQueue::test_connection_()
{
    if (!test_connection())
    {
        throw std::runtime_error("Database connection test failed");
    }
}
