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

#include <map>
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
//  Description:  Extract the number of outstanding shares from forms
//
// =====================================================================================

// using terms from above book

using vocabulary = std::vector<std::string>;                    //  words
using document_features = std::map<std::string, int>;           //  words and counts for single doc/query
using document_idf = std::map<std::string, float>;              //  vocab word, inverse document frequency

using features_list = std::map<int, document_features>;         //  collection of doc/query ID, query features

using features_vectors = std::map<int, std::vector<float>>;
using idfs_vector = std::map<int, std::vector<float>>;  //  weights = count * idf

class SharesOutstanding
{
public:

    // ====================  LIFECYCLE     ======================================= 

    SharesOutstanding () = default;                   // constructor 

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    int64_t operator()(EM::sv html) const;

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

}; // -----  end of class SharesOutstanding  ----- 

    // try a new approach

// NOTE: this method can throw length_error if max_length_to_parse_ != 0

void CleanText(GumboNode* node, size_t max_length_to_clean, std::string& cleaned_text);

[[nodiscard]] std::string ParseHTML(EM::sv html, size_t max_length_to_parse = 0, size_t max_length_to_clean = 0);

[[nodiscard]] std::vector<EM::sv> FindCandidates(const std::string& parsed_text);

#endif   // ----- #ifndef _SHARESOUTSTANDING_INC_  ----- 
