// =====================================================================================
//
//       Filename:  SharesOutstanding.h
//
//    Description: class to find number of shares outstanding in a SEC 10K or 10Q report 
//
//        Version:  1.0
//        Created:  10/11/2019 01:10:55 PM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

#ifndef  _SHARESOUTSTANDING_INC_
#define  _SHARESOUTSTANDING_INC_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/regex.hpp"

// gumbo-parse

#include "gumbo.h"

#include "Extractor_Utils.h"


// =====================================================================================
//        Class:  SharesOutstanding
//  Description: 
//
// =====================================================================================
class SharesOutstanding
{
public:
    // ====================  LIFECYCLE     ======================================= 
    SharesOutstanding ();                             // constructor 

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    int64_t operator()(EM::sv html) const;

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    std::string CleanText(GumboNode* node) const;

    // ====================  DATA MEMBERS  ======================================= 

    using re_info = std::pair<std::string, std::unique_ptr<boost::regex const>>;

    std::vector<re_info> shares_matchers_;

}; // -----  end of class SharesOutstanding  ----- 

#endif   // ----- #ifndef _SHARESOUTSTANDING_INC_  ----- 
