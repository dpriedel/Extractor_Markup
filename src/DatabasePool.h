#/* =====================================================================================
 *
 * Filename:  DatabasePool.h
 *
 * Description:  Class which provides a pool of database connections.
 *
 * Version:  1.0
 * Created:  2026-03-11 16:00:11
 * Revision:  none
 * Compiler:  gcc / g++
 *
 * Author:  David P. Riedel <driedel@cox.net> and Qwen3.5
 * Copyright (c) 2026, David P. Riedel
 *
 * =====================================================================================
 */

#ifndef DATABASEPOOL_H_
#define DATABASEPOOL_H_

#include "ConnectionQueue.h"
#include <memory>
#include <pqxx/pqxx>
#include <string>

class DatabasePool
{
public:
    // Constructor takes connection string and pool size
    DatabasePool(const std::string &connection_string, size_t pool_size = 4);

    // Test if the pool can create connections
    bool test_connection() const;

    void test_connection_();

    // Get a connection from the pool (blocking if all in use)
    std::unique_ptr<pqxx::connection> get_connection() const;

    // Return a connection to the pool
    void return_connection(std::unique_ptr<pqxx::connection> conn) const;

    // Get pool statistics
    size_t pool_size() const;

    size_t available_count() const;

private:
    std::string connection_string_;
    size_t pool_size_;
    mutable ConnectionQueue connection_queue_;
};

#endif /* DATABASEPOOL_H_ */
