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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <queue>
#include <string>

class ConnectionQueue
{
public:
    explicit ConnectionQueue(const std::string &connection_string, size_t max_size = 4);

    // Get a connection from the queue (blocking if all in use)
    std::unique_ptr<pqxx::connection> get_connection();

    // Return a connection to the queue
    void return_connection(std::unique_ptr<pqxx::connection> conn);

    // Get count of available connections
    size_t available_count() const;

    bool test_connection() const;

    void test_connection_();

private:
    std::string connection_string_;
    size_t max_size_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::unique_ptr<pqxx::connection>> available_;
};

#endif /* CONNECTIONQUEUE_H_ */
