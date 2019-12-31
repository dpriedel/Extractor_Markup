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
SharesOutstanding::SharesOutstanding (size_t max_length)
    : max_length_to_parse_{max_length}
{
    queries_ =
    {
        EM::sv{"x nnn shares of common stock of, par value $.02 per share, were outstanding as of ddd x"},
        EM::sv{"x Common Stock, $0.001 par value, outstanding at ddd 31, 2016. nnn x"},
        EM::sv{"x nnn shares of the registrant s common stock were outstanding x"},
        EM::sv{"x nnn Number of shares of common stock, $0.01 par value, outstanding at ddd 7, 2013 x"},
        EM::sv{"x Common stock, $0.01 par value; authorized: nnn shares; issued: nnn shares at ddd x"},
        EM::sv{"x Common stock, $0.01 par value; nnn authorized shares issued nnn shares x"},
        EM::sv{"x Common stock, no par value: nnn shares authorized, FY 2014: nnn issued and nnn outstanding x"},
        EM::sv{"x nnn Number of shares of common stock, $0.01 par value, outstanding at ddd 7, 2013 nnn x"},
        EM::sv{"x Common stock, $0.001 par value, nnn shares authorized; nnn shares issued and outstanding x"},
        EM::sv{"x Common stock - $.001 par value: nnn shares authorized, nnn and nnn shares issued and outstanding at ddd 31, 2014 and 2013, respectively x"},
        EM::sv{"x Common stock, $0.001 par value, nnn shares authorized; nnn and nnn shares issued and outstanding respectively x"},
        EM::sv{"x As of ddd 15, 2013, there were nnn shares of Common Stock outstanding x"},
        EM::sv{"x Weighted average number of common shares used in computing basic earnings per share nnn x"},
        EM::sv{"x indicate state the number of shares outstanding of each class of common stock as of latest date ddd nnn x"},
        EM::sv{"x yes no the number of shares outstanding of common stock as of latest date ddd nnn x"},
        EM::sv{"x yes no nnn the number of shares of common stock par value as of ddd x"},
        EM::sv{"x yes no there were nnn shares of common stock issued and outstanding as of ddd x"},
        EM::sv{"x yes no on ddd the registrant had outstanding nnn shares of its common stock x"}
    };

    boost::regex::flag_type my_flags = {boost::regex_constants::normal | boost::regex_constants::icase};

    shares_extractors_.emplace(1, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(2, std::make_unique<boost::regex const>(se6_, my_flags));
    shares_extractors_.emplace(3, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(4, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(5, std::make_unique<boost::regex const>(se2_, my_flags));
    shares_extractors_.emplace(6, std::make_unique<boost::regex const>(se5_, my_flags));
    shares_extractors_.emplace(7, std::make_unique<boost::regex const>(se3_, my_flags));
    shares_extractors_.emplace(8, std::make_unique<boost::regex const>(se4_, my_flags));
    shares_extractors_.emplace(9, std::make_unique<boost::regex const>(se5_, my_flags));
    shares_extractors_.emplace(10, std::make_unique<boost::regex const>(se5_, my_flags));
    shares_extractors_.emplace(11, std::make_unique<boost::regex const>(se3_, my_flags));
    shares_extractors_.emplace(12, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(13, std::make_unique<boost::regex const>(se7_, my_flags));
    shares_extractors_.emplace(14, std::make_unique<boost::regex const>(se6_, my_flags));
    shares_extractors_.emplace(15, std::make_unique<boost::regex const>(se6_, my_flags));
    shares_extractors_.emplace(16, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(17, std::make_unique<boost::regex const>(se1_, my_flags));
    shares_extractors_.emplace(18, std::make_unique<boost::regex const>(se1_, my_flags));

    // now, our stop words

    std::ifstream stop_words_file{"/usr/share/nltk_data/corpora/stopwords/english"};
    std::copy(std::istream_iterator<std::string>(stop_words_file), std::istream_iterator<std::string>(), std::back_inserter(stop_words_));
    stop_words_file.close();

}  // -----  end of method SharesOutstanding::SharesOutstanding  (constructor)  ----- 

int64_t SharesOutstanding::operator() (EM::sv html) const
{
    std::string the_text = ParseHTML(html, 2'000'000, 20'000);
    std::vector<EM::sv> possibilites = FindCandidates(the_text);

    std::cout << "\npossibilities-----------------------------\n";

    ranges::for_each(possibilites, [](const auto& x) { std::cout << "Possible: " << x << "\n\n"; });

    auto features_list_docs = CreateFeaturesList(possibilites);
    auto features_list_qrys = CreateFeaturesList(queries_);

//    std::cout << "\nfeatures-----------------------------\n";
//
//    std::cout << "document features list\n";
//    ranges::for_each(features_list_docs, [](const auto& e) { const auto& [id, list] = e; std::cout << "\ndoc ID: " << id << '\n'; ranges::for_each(list, [](const auto& y) { std::cout << '\t' << y.first << " : " << y.second << '\n'; }); });
//    std::cout << "\nquery features list\n";
//    ranges::for_each(features_list_qrys, [](const auto& e) { const auto& [id, list] = e; std::cout << "\nqry ID: " << id << '\n'; ranges::for_each(list, [](const auto& y) { std::cout << '\t' << y.first << " : " << y.second << '\n'; }); });

    auto vocab = CollectVocabulary(features_list_docs, features_list_qrys);

//    std::cout << "\nvocabulary-----------------------------\n";
//
//    ranges::for_each(vocab, [](const auto& x) { std::cout << x << "\n"; });

//    ranges::for_each(features_list_docs, [](const auto x) { std::cout << "Possible: " << x << "\n\n"; });

//    auto vectors_docs = Vectorize(vocab, features_list_docs);
    auto vectors_qrys = Vectorize(vocab, features_list_qrys);

//    std::cout << "\nvectorize-----------------------------\n";
//
//    ranges::for_each(vectors_docs, [](const auto& e) { const auto& [id, list] = e; std::cout << "key: " << id << " values: " << ranges::views::all(list) << '\n'; });
//    ranges::for_each(vectors_qrys, [](const auto& e) { const auto& [id, list] = e; std::cout << "key: " << id << " values: " << ranges::views::all(list) << '\n'; });

    auto IDFs_docs = CalculateIDFs(vocab, features_list_docs);
//    auto IDFs_qrys = CalculateIDFs(vocab, features_list_qrys);

//    std::cout << "\nIDFs-----------------------------\n";
//
//    ranges::for_each(IDFs_docs, [](const auto& e) { const auto& [word, idf] = e; std::cout << "word: " << word << " idf: " << idf << '\n'; });
//    ranges::for_each(IDFs_qrys, [](const auto& e) { const auto& [word, idf] = e; std::cout << "word: " << word << " idf: " << idf << '\n'; });
//
    auto IDF_vectors_docs = VectorizeIDFs(vocab, features_list_docs, IDFs_docs);
//    auto IDF_vectors_qrys = VectorizeIDFs(vocab, features_list_qrys, IDFs_qrys);

//    std::cout << "\nIDF vectors-----------------------------\n";
//
//    ranges::for_each(IDF_vectors_docs, [](const auto& e) { const auto& [id, list] = e; std::cout << "key: " << id << " values: " << ranges::views::all(list) << '\n'; });
//    ranges::for_each(IDF_vectors_qrys, [](const auto& e) { const auto& [id, list] = e; std::cout << "key: " << id << " values: " << ranges::views::all(list) << '\n'; });

    // now, we apply each query to each document and look for the best match(s)

    std::vector<std::tuple<int, int, float>> match_results;

    for (const auto& [query_id, query_vec] : vectors_qrys)
    {
        for (const auto& [doc_id, doc_vec] : IDF_vectors_docs)
        {
            float cos = MatchQueryToContent(query_vec, doc_vec);
            match_results.emplace_back(std::make_tuple(query_id, doc_id, cos));
        }
    }

    // descending sort
    ranges::sort(match_results, [](const auto& a, const auto& b) { return std::get<2>(b) < std::get<2>(a); });

//    for (const auto& result : match_results)
//    {
//        std::cout << "query_id: " << std::get<0>(result) << " doc id: " << std::get<1>(result) << " match goodness: " << std::get<2>(result) << '\n';
//    }

    if ( match_results.empty())
    {
        spdlog::info("*** something wrong...no match results. ***\n");
        std::cout << the_text << "\n\n";
        return -1;
    }
    
    spdlog::debug("\nMatching results-----------------------------\n");

    std::string shares = "-1";

    for (auto [qry, doc, cos] : match_results)
    {
        spdlog::debug(catenate("query_id: ", qry, " doc id: ", doc, " match goodness: ", cos));
    
        if (cos >= 0.7)   // closer to 1 than to zero
        {
            boost::cmatch the_shares;

            spdlog::debug(catenate("using this query: ", queries_.at(qry - 1), "\n"));
            spdlog::debug(catenate("on this candidate: ", possibilites.at(doc - 1), "\n"));

            if (bool found_it = boost::regex_search(std::begin(possibilites[doc - 1]), std::end(possibilites[doc - 1]), the_shares, *shares_extractors_.at(qry)))
            {
               shares = the_shares.str(1); 
               break;
            }
        }
        else
        {
            std::cout << "\npossibilities-----------------------------\n";
            ranges::for_each(possibilites, [](const auto& x) { std::cout << "Possible: " << x << "\n\n"; });

            for (const auto& result : match_results)
            {
                std::cout << "query_id: " << std::get<0>(result) << " doc id: " << std::get<1>(result) << " match goodness: " << std::get<2>(result) << '\n';
            }
            break;
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
    static const std::string a1 = R"***(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b.{1,75}?\bcommon stock\b)***";
    static const std::string a2 = R"***(\bcommon stock\b.{1,75}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";
    static const std::string a3 = R"***(\bweighted average\b.{1,50}?\bcommon\b.{1,75}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";
    static const std::string a4 = R"***(\b(?:indicate|state) the number of shares outstanding\b.{1,100}?common.{1,150}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";
    static const std::string a5 = R"***(\byes\b.{1,10}?no.{1,20}?the number of.{1,30}?shares.{1,50}?common.{1,75}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";
    static const std::string a6 = R"***(\byes\b.{1,10}?no.{1,70}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b.{1,50}?shares.{1,50}?common?)***";
    static const std::string a7 = R"***(\byes\b.{1,10}?no.{1,300}?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***";

    static const boost::regex regex_shares_only1{a1, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only2{a2, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only3{a3, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only4{a4, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only5{a5, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only6{a6, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only7{a7, boost::regex_constants::normal | boost::regex_constants::icase};

    std::vector<EM::sv> results;

    boost::sregex_iterator iter1(the_text.begin(), the_text.end(), regex_shares_only4);
    std::for_each(iter1, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        // we need to prefix our leading number with some characters so later trim logic will not drop it.
        EM::sv xx(the_text.data() + m.position(), m.length() + 100);
        results.push_back(xx);
    });

    boost::sregex_iterator iter2(the_text.begin(), the_text.end(), regex_shares_only5);
    std::for_each(iter2, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        EM::sv xx(the_text.data() + m.position(), m.length() + 100);
        results.push_back(xx);
    });

    boost::sregex_iterator iter3(the_text.begin(), the_text.end(), regex_shares_only6);
    std::for_each(iter3, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        EM::sv xx(the_text.data() + m.position() -10, m.length() + 110);
        results.push_back(xx);
    });

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

