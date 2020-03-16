// =====================================================================================
//
//       Filename:  ActivityList.h
//
//    Description: Class to manage async form updates 
//
//        Version:  1.0
//        Created:  03/16/2020 09:01:15 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

// =====================================================================================
//        Class:  ActivityList
//  Description: Manage access to CIK-PeriodEnding updates.
//              We single thread on <CIK>_<period_end_date>.
//              Mainly needed for amended form processing.
//              If we can add our entry to the list, we can proceed.
//              If not, wait and try again.
// =====================================================================================


#ifndef  activitylist_INC
#define  ActivityList_INC

#include <mutex>
#include <set>
#include <string>

class ActivityList
{
public:
    // ====================  LIFECYCLE     ======================================= 
    ActivityList () = default;                             // constructor 
    ActivityList(const ActivityList& rhs) = delete;
    ActivityList(ActivityList&& rhs) = delete;

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    bool AddEntry(const std::string& entry);
    void RemoveEntry(const std::string& entry);

    // ====================  OPERATORS     ======================================= 

    ActivityList& operator = (const ActivityList& rhs) = delete;
    ActivityList& operator = (ActivityList&& rhs) = delete;

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    std::mutex m_;
    std::set<std::string> active_forms_;

}; // -----  end of class ActivityList  ----- 

#endif   // ----- #ifndef ActivityList_INC  ----- 
