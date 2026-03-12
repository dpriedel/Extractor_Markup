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
#include <string>

class DatabasePool
{
public:
    DatabasePool(const std::string &connection_string, size_t pool_size = 4);

    // Get a connection wrapper (blocking if all in use)
    PooledConnection get_connection();

    bool test_connection() const;
    size_t pool_size() const
    {
        return pool_size_;
    }
    size_t available_count() const;

private:
    std::string connection_string_;
    size_t pool_size_;
    ConnectionQueue connection_queue_;
};

#endif /* DATABASEPOOL_H_ */
