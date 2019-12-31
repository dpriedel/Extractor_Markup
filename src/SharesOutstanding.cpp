// =====================================================================================
//
//       Filename:  SharesOutstanding.cpp
//
//    Description: class to find the number of shares ourstanding from a SEC 10K or 10Q report 
//
//        Version:  1.0
//        Created:  10/11/2019 01:11:43 PM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

#include "SharesOutstanding.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <functional>
#include <memory>
#include <set>

#include <range/v3/action/remove_if.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/transform.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/trim.hpp>


#include "spdlog/spdlog.h"

#include "HTML_FromFile.h"

//--------------------------------------------------------------------------------------
//       Class:  SharesOutstanding
//      Method:  SharesOutstanding
// Description:  constructor
//--------------------------------------------------------------------------------------
SharesOutstanding::SharesOutstanding ()
{

}  // -----  end of method SharesOutstanding::SharesOutstanding  (constructor)  ----- 

int64_t SharesOutstanding::operator() (EM::sv html) const
{
    const std::string se_8 = R"***(\byes\b.{1,10}?no.{1,700}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***";

    boost::regex::flag_type my_flags = {boost::regex_constants::normal | boost::regex_constants::icase};

    std::map<int, std::unique_ptr<boost::regex>> shares_extractors_;

    shares_extractors_.emplace(1, std::make_unique<boost::regex>(se_8, my_flags));

    std::string the_text = ParseHTML(html, 1'000'000, 10'000);
    std::vector<EM::sv> possibilites = FindCandidates(the_text);

    std::cout << "\npossibilities-----------------------------\n";

    ranges::for_each(possibilites, [](const auto& x) { std::cout << "Possible: " << x << "\n\n"; });

    spdlog::debug("\nMatching results-----------------------------\n");

    std::string shares = "-1";

    for (auto possible : possibilites)
    {
        for (const auto& [id, extractor] : shares_extractors_)
        {
            boost::cmatch the_shares;

            if (bool found_it = boost::regex_search(std::begin(possible), std::end(possible), the_shares, *extractor))
            {
               shares = the_shares.str(1); 
               break;
            }
        }
    }

    int64_t shares_outstanding;

    if (shares != "-1")
    {

        // need to replace any commas we might have.

        const std::string delete_this{""};
        const boost::regex regex_comma{R"***(,)***"};
        shares = boost::regex_replace(shares, regex_comma, delete_this);

        if (auto [p, ec] = std::from_chars(shares.data(), shares.data() + shares.size(),
                    shares_outstanding); ec != std::errc())
        {
            throw ExtractorException(catenate("Problem converting shares outstanding: ",
                        std::make_error_code(ec).message(), '\n'));
        }
    }
    else
    {
        shares_outstanding = -1;
    }
    
    spdlog::debug(catenate("Shares outstanding: ", shares_outstanding));
    return shares_outstanding;
}		// -----  end of method SharesOutstanding::operator()  ----- 

std::string CleanText(GumboNode* node, size_t max_length_to_clean)
{
    //    this code is based on example code in Gumbo Parser project
    //    I've added the ability to break out of the recursive
    //    loop if a maximum length to parse is given.
    //    I use a Pythonic approach...throw an exception (think 'stop iteration').

    static const boost::regex regex_nbr{R"***(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***"};

    if (node->type == GUMBO_NODE_TEXT)
    {
        return node->v.text.text;
    }
    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag != GUMBO_TAG_SCRIPT && node->v.element.tag != GUMBO_TAG_STYLE)
    {
        std::string contents;
//        std::string tagname = gumbo_normalized_tagname(node->v.element.tag);
////        std::cout << tagname << "   ";
//        if (tagname == "tr")
//        {
//            contents += "zzz ";
//        }
        GumboVector* children = &node->v.element.children;

        for (unsigned int i = 0; i < children->length; ++i)
        {
            const std::string text = CleanText((GumboNode*) children->data[i], max_length_to_clean);
            if (! text.empty())
            {
                if (boost::regex_match(text, regex_nbr))
                {
                    contents += ' ';
                }
                contents.append(text);
                if (max_length_to_clean > 0 && contents.size() >= max_length_to_clean)
                {
                    throw std::length_error(contents);
                }
            }
        }
        contents += ' ';
        return contents;
    }
    return {};
}		// -----  end of method SharesOutstanding::CleanText  ----- 

std::vector<EM::sv> FindCandidates(const std::string& the_text)
{
    static const std::string a7 = R"***(\byes\b.{1,10}?no.{1,700}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";

    static const boost::regex regex_shares_only7{a7, boost::regex_constants::normal | boost::regex_constants::icase};

    std::vector<EM::sv> results;

    boost::sregex_iterator iter1(the_text.begin(), the_text.end(), regex_shares_only7);
    std::for_each(iter1, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        // we need to prefix our leading number with some characters so later trim logic will not drop it.
        EM::sv xx(the_text.data() + m.position(), m.length());
        results.push_back(xx);
    });

//    boost::sregex_iterator iter2(the_text.begin(), the_text.end(), regex_shares_only5);
//    std::for_each(iter2, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
//    {
//        EM::sv xx(the_text.data() + m.position(), m.length() + 100);
//        results.push_back(xx);
//    });
//
//    boost::sregex_iterator iter3(the_text.begin(), the_text.end(), regex_shares_only6);
//    std::for_each(iter3, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
//    {
//        EM::sv xx(the_text.data() + m.position() -10, m.length() + 110);
//        results.push_back(xx);
//    });

    return results;
}		// -----  end of method SharesOutstanding::FindCandidates  ----- 

std::string ParseHTML (EM::sv html, size_t max_length_to_parse, size_t max_length_to_clean)
{
    static const boost::regex regex_hi_ascii{R"***([^\x00-\x7f])***"};
    static const boost::regex regex_multiple_spaces{R"***( {2,})***"};
    static const boost::regex regex_nl{R"***(\n{1,})***"};
    static const std::string one_space = " ";

    GumboOptions options = kGumboDefaultOptions;

    size_t length_HTML_to_parse = max_length_to_parse == 0 ? html.length() : std::min(html.length(), max_length_to_parse);

    std::unique_ptr<GumboOutput, std::function<void(GumboOutput*)>> output(gumbo_parse_with_options(&options, html.data(), length_HTML_to_parse),
            [&options](GumboOutput* output){ gumbo_destroy_output(&options, output); });

    std::string parsed_text;
    try
    {
        parsed_text = CleanText(output->root, max_length_to_clean);
    }
    catch (std::length_error& e)
    {
        parsed_text = e.what();
    }
    gumbo_destroy_output(&options, output.release());
    //
    // do a little cleanup to make searching easier

    std::string the_text = boost::regex_replace(parsed_text, regex_hi_ascii, one_space);
    the_text = boost::regex_replace(the_text, regex_multiple_spaces, one_space);
    the_text = boost::regex_replace(the_text, regex_nl, one_space);

    return the_text;
}		// -----  end of method SharesOutstanding::ParseHTML  ----- 

features_list CreateFeaturesList (const std::vector<EM::sv>& candidates)
{
    static std::set<std::string> months{{"january"}, {"february"}, {"march"}, {"april"}, {"may"}, {"june"}, {"july"}, {"august"}, {"september"}, {"october"}, {"november"}, {"december"}};
    static const boost::regex regex_shares_number{R"***(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***"};
    static const boost::regex regex_shares_date{R"***(\b[0-9]{1,2}[/-][0-9]{1,2}[/-][0-9]{2,4}\b)***"};
    
    // first, make sure each candidate starts and ends with a space (no partial words)
    
    auto trim_text_and_convert([](const auto& candidate) 
    {
        return ranges::views::trim(candidate, [](char c) { return c != ' '; })
            | ranges::views::transform([](char c) { return tolower(c); })
            | ranges::to<std::string>();
    }); 

    // we would split into words, remove stop words, maybe lematize...
    // But, since our 'documents' are short, we'll just go with splitting into words
    // AND removing stop words and numbers and punction (for now)
    // AND, we replace month names and numbers that look like they could be
    // numbers of shares with some generic placeholders.

    auto tokenize =
        ranges::views::transform([](const auto&word_rng)
            {
                std::string word(&*ranges::begin(word_rng), ranges::distance(word_rng));
                return word;
            });

    auto cleanup_tokens = ranges::views::transform([](const std::string& word)
            {
                std::string token;
                if (months.contains(word))
                {
                    token = std::string{"ddd"};
                }
                else
                {
                    token = boost::regex_replace(word, regex_shares_number, "nnn");
                    token = boost::regex_replace(token, regex_shares_date, "ddd");
                }
                token |= ranges::actions::remove_if([](char c) { return ispunct(c); });
                return token;
            });
    
    auto filter_stop_words =
        ranges::views::filter([](const auto& token)
            {
                // OK, we not removing stop words, just those which contain numbers
                return ! token.empty()
                    && (ranges::find_if(token, [](int c) { return isdigit(c); }) == ranges::end(token));
//                && (ranges::find(stop_words_, token) == ranges::end(stop_words_));
            });

    std::vector<std::string> results = candidates
        | ranges::views::transform(trim_text_and_convert)
        | ranges::to<std::vector>();

    features_list terms_and_counts;

    for(int ID = 0; const auto& result : results)
    {
        document_features features;
        auto word_rngs = result | ranges::views::split(' ');
        ranges::for_each(word_rngs | tokenize | cleanup_tokens | filter_stop_words, [&features](const auto& token)
        {
            features.try_emplace(token, 0);
            features[token] += 1;
        });
        terms_and_counts.emplace(++ID, features);
    }

    return terms_and_counts;
}		// -----  end of method SharesOutstanding::CreateFeaturesList  ----- 

vocabulary CollectVocabulary (const features_list& doc_features, const features_list& query_features)
{
    vocabulary results;

    ranges::for_each(doc_features | ranges::views::values, [&results](const auto& x)
    {
        ranges::copy(x | ranges::views::keys, ranges::back_inserter(results));
    });
    ranges::for_each(query_features | ranges::views::values, [&results](const auto& x)
    {
        ranges::copy(x | ranges::views::keys, ranges::back_inserter(results));
    });

    results |= ranges::actions::sort | ranges::actions::unique;

    return results;
}		// -----  end of method SharesOutstanding::CollectVocabulary  ----- 

features_vectors Vectorize (const vocabulary& vocab, const features_list& features)
{
    features_vectors results;

    for (const auto& [ID, doc_features] : features)
    {
        std::vector<float> counts;

        for (const auto& word : vocab)
        {
            if (bool found_it = doc_features.contains(word); found_it)
            {
                counts.push_back(doc_features.at(word));
            }
            else
            {
                counts.push_back(0.0);
            }
        }
        results.emplace(ID, std::move(counts));
    }
    return results;
}		// -----  end of method SharesOutstanding::Vectorize  ----- 

document_idf CalculateIDFs (const vocabulary& vocab, const features_list& features)
{
    document_idf results;

    for (const auto& term : vocab)
    {
        int doc_count = 0;

        for (const auto& [ID, doc_features] : features)
        {
            if (bool found_it = doc_features.contains(term); found_it)
            {
                doc_count += 1;
            }
        }
//        float idf = log10(float(features.size()) / float(1 + doc_count));
        float idf = log10(doc_count + 1);          // skip inverse for now
//        std::cout << "term: " << term << " how many docs: " << features.size() << "docs using: " << doc_count << " idf: " << idf << '\n';
        results.emplace(term, idf);
    }
    return results;
}		// -----  end of method SharesOutstanding::CalculateIDF  ----- 

idfs_vector VectorizeIDFs (const vocabulary& vocab, const features_list& features, const document_idf& idfs)
{
    idfs_vector results;

    for (const auto& [ID, doc_features] : features)
    {
        std::vector<float> weights;

        for (const auto& term : vocab)
        {
            if (auto found_it = doc_features.find(term); found_it != doc_features.end())
            {
                weights.push_back(idfs.at(term) * found_it->second);
            }
            else
            {
                weights.push_back(0.0);
            }
        }
        results.emplace(ID, std::move(weights));
    }
    return results;
}		// -----  end of method SharesOutstanding::VectorizeIDFs  ----- 

float MatchQueryToContent (const std::vector<float>& query, const std::vector<float>& document)
{
    float cosine = VectorDotProduct(query, document) / (VectorLength(query) * VectorLength(document));
    return cosine;
}		// -----  end of method SharesOutstanding::MatchQueryToContent  ----- 

float VectorLength (const std::vector<float>& vec)
{
    float length_squared = 0.0;
    for (auto e : vec)
    {
        length_squared += pow(e, 2);
    }
    return sqrt(length_squared);
}		// -----  end of method SharesOutstanding::ComputeVectorLength  ----- 

float VectorDotProduct(const std::vector<float>& vec_a, const std::vector<float>& vec_b)
{
    if (vec_a.size() != vec_b.size())
    {
        throw std::runtime_error("vector size mismatch.");
    }
    float dot_prod = 0.0;
    for (int indx = 0; indx < vec_a.size(); ++indx)
    {
        if (vec_a[indx] != 0.0 && vec_b[indx] != 0.0)
        {
            dot_prod += vec_a[indx] * vec_b[indx];
        }
    }
    return dot_prod;
}		// -----  end of method SharesOutstanding::VectorDotProduct  ----- 

