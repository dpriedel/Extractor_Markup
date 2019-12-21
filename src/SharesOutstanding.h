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
    using document_idf = std::map<std::string, float>;              //  vocab word, inverse document frequency

    using features_list = std::map<int, document_features>;         //  collection of doc/query ID, query features
    
    using features_vectors = std::map<int, std::vector<float>>;
    using idfs_vector = std::map<int, std::vector<float>>;  //  weights = count * idf

    // ====================  LIFECYCLE     ======================================= 

    explicit SharesOutstanding (size_t max_length = 0);                   // constructor 

    // ====================  ACCESSORS     ======================================= 

    // NOTE: this method can throw length_error if max_length_to_parse_ != 0

    std::string CleanText(GumboNode* node, size_t max_length_to_clean) const;

    // try a new approach

    [[nodiscard]] std::string ParseHTML(EM::sv html, size_t max_length_to_parse = 0, size_t max_length_to_clean = 0) const;

    [[nodiscard]] std::vector<EM::sv> FindCandidates(const std::string& parsed_text) const;

    [[nodiscard]] features_list CreateFeaturesList(const std::vector<EM::sv>& candidates) const;

    [[nodiscard]] vocabulary CollectVocabulary(const features_list& doc_features, const features_list& query_features) const;

    [[nodiscard]] features_vectors Vectorize(const vocabulary& vocab, const features_list& features) const;

    [[nodiscard]] document_idf CalculateIDFs(const vocabulary& vocab, const features_list& features) const;

    [[nodiscard]] idfs_vector VectorizeIDFs(const vocabulary& vocab, const features_list& features, const document_idf& idfs) const;

    [[nodiscard]] float MatchQueryToContent(const std::vector<float>& query, const std::vector<float>& document) const;

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    int64_t operator()(EM::sv html) const;

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    [[nodiscard]] float VectorLength(const std::vector<float>& vec) const;
    [[nodiscard]] float VectorDotProduct(const std::vector<float>& vec_a, const std::vector<float>& vec_b) const;

    // ====================  DATA MEMBERS  ======================================= 

    std::vector<EM::sv> queries_;

    std::map<int, std::unique_ptr<boost::regex const>> shares_extractors_;

    std::vector<std::string> stop_words_;

    std::string se1_ = R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,30}?common)***";
    std::string se2_ = R"***(authorized.{1,20}?(?:\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,50}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";
    std::string se3_ = R"***((?:\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,20}?authorized.{1,50}?(?:\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,20}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";
    std::string se4_ = R"***((?:\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,100}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";
    std::string se5_ = R"***((?:\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,20}?authorized.{1,50}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";
    std::string se6_ = R"***(common.{1,100}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";

    size_t max_length_to_parse_;
}; // -----  end of class SharesOutstanding  ----- 

#endif   // ----- #ifndef _SHARESOUTSTANDING_INC_  ----- 
