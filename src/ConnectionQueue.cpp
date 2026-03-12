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
#include <spdlog/spdlog.h>

PooledConnection::PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionQueue &pool)
    : conn_(std::move(conn)), pool_(&pool)
{
}

PooledConnection::~PooledConnection()
{
    // Always return the slot back to the pool, even if conn_ is null.
    // This preserves the pool capacity if a connection is dropped.
    if (pool_)
    {
        pool_->return_connection(std::move(conn_));
    }
}

ConnectionQueue::ConnectionQueue(const std::string &connection_string, size_t max_size)
    : connection_string_(connection_string), max_size_(max_size)
{
    for (size_t i = 0; i < max_size_; ++i)
    {
        available_.push({std::make_unique<pqxx::connection>(connection_string_), std::chrono::steady_clock::now()});
    }
    spdlog::info("Connection queue initialized with {} connections.", max_size_);
}

PooledConnection ConnectionQueue::get_connection()
{
    ConnectionEntry entry;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !available_.empty(); });

        entry = std::move(available_.front());
        available_.pop();
    }

    auto now = std::chrono::steady_clock::now();
    bool needs_health_check = (now - entry.last_used) > std::chrono::seconds(30);

    if (!entry.conn || !entry.conn->is_open() || needs_health_check)
    {
        try
        {
            // Explicitly throw if connection is dead so the catch block handles the renewal
            if (!entry.conn || !entry.conn->is_open())
            {
                throw std::runtime_error("Connection is null or closed natively.");
            }

            if (needs_health_check)
            {
                pqxx::nontransaction test_trxn{*entry.conn};
                test_trxn.exec("SELECT 1");
                spdlog::trace("Connection health check passed");
            }
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Connection stale or broken: {}. Renewing.", e.what());
            try
            {
                entry.conn = std::make_unique<pqxx::connection>(connection_string_);
            }
            catch (...)
            {
                return_connection(std::move(entry.conn));
                throw;
            }
        }
    }

    return PooledConnection(std::move(entry.conn), *this);
}

void ConnectionQueue::return_connection(std::unique_ptr<pqxx::connection> conn)
{
    // Removed the `if (!conn) return;` check.
    // We MUST allow null connections back into the queue to maintain the pool's max size.
    std::lock_guard<std::mutex> lock(mutex_);
    available_.push({std::move(conn), std::chrono::steady_clock::now()});
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
