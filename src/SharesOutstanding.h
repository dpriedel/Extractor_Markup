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

#ifndef _SHARESOUTSTANDING_INC_
#define _SHARESOUTSTANDING_INC_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "boost/regex.hpp"

// gumbo-parse

#include "Extractor_Utils.h"
#include "gumbo.h"

// =====================================================================================
//        Class:  SharesOutstanding
//  Description:  Extract the number of outstanding shares from forms
//
// =====================================================================================

class SharesOutstanding
{
   public:
    // ====================  LIFECYCLE     =======================================

    SharesOutstanding() = default;    // constructor

    // ====================  ACCESSORS     =======================================

    // ====================  MUTATORS      =======================================

    // ====================  OPERATORS     =======================================

    int64_t operator()(EM::HTMLContent html) const;

   protected:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

   private:
    // ====================  METHODS       =======================================

    // ====================  DATA MEMBERS  =======================================

};    // -----  end of class SharesOutstanding  -----

// try a new approach

// NOTE: this method can throw length_error if max_length_to_parse_ != 0

void CleanText(GumboNode* node, size_t max_length_to_clean, std::string& cleaned_text);

[[nodiscard]] std::string ParseHTML(EM::HTMLContent html, size_t max_length_to_parse = 0, size_t max_length_to_clean = 0);

[[nodiscard]] std::vector<EM::sv> FindCandidates(const std::string& parsed_text);

#endif    // ----- #ifndef _SHARESOUTSTANDING_INC_  -----
