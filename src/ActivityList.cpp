// =====================================================================================
//
//       Filename:  ActivityList.cpp
//
//    Description: Implementation of ActivityList 
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

#include "ActivityList.h"

bool ActivityList::AddEntry (const std::string& new_entry)
{
    const std::lock_guard<std::mutex> lk{m_};
    const auto[it, success] = active_forms_.insert(new_entry);

    return success;
}		// -----  end of method ActivityList::AddEntry  ----- 


void ActivityList::RemoveEntry (const std::string& entry)
{
    const std::lock_guard<std::mutex> lk{m_};

    //TODO: decide whether to throw if entry not found.

    auto pos = active_forms_.find(entry);
    if (pos != active_forms_.end())
    {
        active_forms_.erase(pos);
    }
}		// -----  end of method ActivityList::RemoveEntry  ----- 

