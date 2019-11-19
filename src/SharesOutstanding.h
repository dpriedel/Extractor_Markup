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
//  Description:  Approach used is based upon code from book "Essential Natural Language
//  Processing" by Ekaterina Kockmar from Manning Publications.
//
// =====================================================================================
class SharesOutstanding
{
public:

    // using terms from above book

    using vocabulary = std::vector<std::string>;                    //  words
    using document_features = std::map<std::string, int>;           //  words and counts for single doc/query

    using features_list = std::map<int, document_features>;         //  collection of doc/query ID, query features
    
    using features_vector = std::map<int, std::vector<int>>;

    // ====================  LIFECYCLE     ======================================= 

    explicit SharesOutstanding (size_t max_length = 0);                   // constructor 

    // ====================  ACCESSORS     ======================================= 

    // NOTE: this method can throw length_error if max_length_to_parse_ != 0

    std::string CleanText(GumboNode* node, size_t max_length_to_clean) const;

    // try a new approach

    [[nodiscard]] std::string ParseHTML(EM::sv html, size_t max_length_to_parse = 0, size_t max_length_to_clean = 0) const;

    std::vector<EM::sv> FindCandidates(const std::string& parsed_text) const;

    features_list CreateFeaturesList(const std::vector<EM::sv>& candidates) const;

    vocabulary CollectVocabulary(const features_list& features) const;

    // return void for now because I don't yet know what the return type is
    features_vector Vectorize(const vocabulary& vocab, const features_list& features) const;

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    int64_t operator()(EM::sv html) const;

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    using re_info = std::pair<std::string, std::unique_ptr<boost::regex const>>;

    std::vector<re_info> shares_matchers_;
    std::vector<std::string> stop_words_;

    size_t max_length_to_parse_;
}; // -----  end of class SharesOutstanding  ----- 

#endif   // ----- #ifndef _SHARESOUTSTANDING_INC_  ----- 
