// =====================================================================================
//
//       Filename:  ExtractorMutexAndLock.cpp
//
//    Description: Implementation of ExtractMutex
//
//        Version:  1.0
//        Created:  03/16/2020 09:19:59 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

#include "ExtractorMutexAndLock.h"

#include <thread>

bool ExtractMutex::AddEntry(const std::string& new_entry)
{
    std::lock_guard<std::mutex> lk{m_};
    const auto [it, success] = active_forms_.insert(new_entry);

    return success;
}    // -----  end of method ExtractMutex::AddEntry  -----

void ExtractMutex::RemoveEntry(const std::string& entry)
{
    std::lock_guard<std::mutex> lk{m_};

    // TODO: decide whether to throw if entry not found.

    auto pos = active_forms_.find(entry);
    if (pos != active_forms_.end())
    {
        active_forms_.erase(pos);
    }
}    // -----  end of method ExtractMutex::RemoveEntry  -----

//--------------------------------------------------------------------------------------
//       Class:  ExtractLock
//      Method:  ExtractLock
// Description:  constructor
//--------------------------------------------------------------------------------------
ExtractLock::ExtractLock(ExtractMutex* extract_list, const std::string& locking_id)
    : extract_list_{extract_list}, locking_id_{locking_id}, lock_is_active_{false}
{
    // loop forever while trying to acquire our lock
    while (!lock_is_active_)
    {
        if (extract_list_->AddEntry(locking_id))
        {
            lock_is_active_ = true;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }
    }
}    // -----  end of method ExtractLock::ExtractLock  (constructor)  -----

ExtractLock::~ExtractLock()
{
    if (lock_is_active_)
    {
        extract_list_->RemoveEntry(locking_id_);
    }
}    // -----  end of method ExtractLock::~ExtractLock  -----
