#/* =====================================================================================
 *
 * Filename:  ConnectionQueue.h
 *
 * Description:  Class which provides a Queue for managing database connections.
 *
 * Version:  1.0
 * Created:  2026-03-11 15:55:13
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> (and Qwen3.5)
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#ifndef CONNECTIONQUEUE_H_
#define CONNECTIONQUEUE_H_

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <queue>
#include <string>

struct ConnectionEntry
{
    std::unique_ptr<pqxx::connection> conn;
    std::chrono::steady_clock::time_point last_used;
};

class ConnectionQueue;

class PooledConnection
{
public:
    PooledConnection(std::unique_ptr<pqxx::connection> conn, ConnectionQueue &pool);
    PooledConnection(const PooledConnection &) = delete;
    PooledConnection &operator=(const PooledConnection &) = delete;
    PooledConnection(PooledConnection &&) noexcept = default;
    PooledConnection &operator=(PooledConnection &&) noexcept = default;
    ~PooledConnection();

    pqxx::connection *operator->()
    {
        return conn_.get();
    }
    pqxx::connection &operator*()
    {
        return *conn_;
    }
    explicit operator bool() const
    {
        return conn_ != nullptr;
    }

private:
    std::unique_ptr<pqxx::connection> conn_;
    ConnectionQueue *pool_;
};

class ConnectionQueue
{
public:
    explicit ConnectionQueue(const std::string &connection_string, size_t max_size = 4);
    PooledConnection get_connection();
    void return_connection(std::unique_ptr<pqxx::connection> conn);
    size_t available_count() const;
    bool test_connection() const;

private:
    std::string connection_string_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<ConnectionEntry> available_;
};

#endif /* CONNECTIONQUEUE_H_ */
