// =====================================================================================
//
//       Filename:  ExtractorMutexAndLock.h
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
//        Class:  ExtractMutex
//  Description: Manage access to CIK-PeriodEnding updates.
//              We single thread on <CIK>_<period_end_date>.
//              Mainly needed for amended form processing.
//              If we can add our entry to the list, we can proceed.
//              If not, wait and try again.
// =====================================================================================

#ifndef ExtractMutex_INC
#define ExtractMutex_INC

#include <mutex>
#include <set>
#include <string>

class ExtractMutex
{
public:
    // ====================  LIFECYCLE     =======================================
    ExtractMutex() = default; // constructor
    ExtractMutex(const ExtractMutex &rhs) = delete;
    ExtractMutex(ExtractMutex &&rhs) = delete;

    // ====================  ACCESSORS     =======================================

    // ====================  MUTATORS      =======================================

    bool AddEntry(const std::string &entry);
    void RemoveEntry(const std::string &entry);

    // ====================  OPERATORS     =======================================

    ExtractMutex &operator=(const ExtractMutex &rhs) = delete;
    ExtractMutex &operator=(ExtractMutex &&rhs) = delete;

protected:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

private:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

    std::mutex m_;
    std::set<std::string> active_forms_;

}; // -----  end of class ExtractMutex  -----

// =====================================================================================
//        Class:  ExtractLock
//  Description:  grant access to an activity. uses ExtractMutex.
//                use RAII
// =====================================================================================
class ExtractLock
{
public:
    // ====================  LIFECYCLE     =======================================
    ExtractLock(ExtractMutex *active_forms, const std::string &locking_id_); // constructor
    ~ExtractLock(void);

    // ====================  ACCESSORS     =======================================

    // ====================  MUTATORS      =======================================

    //    bool SeekExtractLock(const std::string& locking_id);
    //    void ReleaseExtractLock();

    // ====================  OPERATORS     =======================================

protected:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

private:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

    ExtractMutex *extract_list_;
    std::string locking_id_;
    bool lock_is_active_;

}; // -----  end of class ExtractLock  -----

#endif // ----- #ifndef ExtractMutex_INC  -----
