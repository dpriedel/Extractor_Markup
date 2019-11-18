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
#include <charconv>
#include <fstream>
#include <functional>
#include <memory>

#include <range/v3/all.hpp>

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
    const std::string s02{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) and (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares issued and outstanding(?:,)? (?:at|as of))***"};
    const std::string s03{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of common stock.{0,30}? (?:at|as of))***"};
    const std::string s04{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of.{1,30}? common stock.{0,30}?outstanding (?:at|as of))***"};
    const std::string s05{R"***(common stock .{0,50}? \b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b and (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares issued and outstanding, respectively)***"};
    const std::string s06{R"***(common stock .{0,70}? \b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b and (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares issued and outstanding at.{1,30}?, respectively)***"};
    const std::string s07{R"***((?:issuer|registrant) had (?:outstanding )?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of (?:its )?common stock(?:,.{0,30}?,)?(?: outstanding)?)***"};
    const std::string s09{R"***(common stock.{1,50}?outstanding (?:as of|at).{1,30}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s10{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) (?:shares issued and outstanding|issued and outstanding shares))***"};
    const std::string s30{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) .?number of shares of common stock(?:, .*?,)? outstanding)***"};
    const std::string s34{R"***(there were (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of .{0,30}?common stock.{0,30}? outstanding)***"};
    const std::string s35{R"***(there were (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) outstanding shares of the issuer.s common stock.{0,30}? on)***"};
    const std::string s37{R"***((?:at|as of).{1,20}? there were (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares outstanding of common stock)***"};
//    const std::string s40{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of the (?:registrant.s|issuer.s) common stock(?:, .*?,)? (?:were )?outstanding)***"};
    const std::string s40{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of the (?:registrant.s|issuer.s) common stock(?:, .*?,)? (?:were )?outstanding)***"};
    const std::string s41{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of common stock of the (?:registrant|issuer) were outstanding as of)***"};
    const std::string s42{R"***((?:as of .{0,30?})?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of the (?:registrant.s|issuer.s) common stock issued and outstanding(?: as of)?)***"};
    const std::string s50{R"***(authorized,.*?\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b issued and (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) outstanding)***"};
    const std::string s60{R"***((?:registrant.s|issuer.s) shares of common stock outstanding was (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) as of)***"};
    const std::string s62{R"***(shares (?:issued and )?outstanding of the registrant.s common stock as of .{1,20}? was.{0,20}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s64{R"***(shares of the (?:registrant.s|issuer.s) common stock outstanding as of .{1,20}? was.{0,20}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s70{R"***(common stock .{0,30}? \b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b shares authorized issued (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares)***"};
    const std::string s72{R"***(common stock .authorized \b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b shares .{1,30}? issued (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s80{R"***((\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) common stock(?:s)? issued and outstanding as of)***"};
    const std::string s81{R"***(common stock.{1,30}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares as of)***"};
    const std::string s84{R"***(number of common shares.{0,50}? issued and outstanding was (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s86{R"***(as of .{1,20}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares of our common stock were outstanding)***"};
    const std::string s88{R"***(\bauthorized\b. \b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b shares. issued. (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares at)***"};
    const std::string s89{R"***(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b shares authorized (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b) shares outstanding)***"};

    // if all of the above fail, look for weighted average.

    const std::string s90{R"***(weighted average shares (?:outstanding )?used to compute.{0,50}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};
    const std::string s91{R"***(weighted.average (?:number of )?(?:common )?shares .{0,50}? (\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b))***"};

    // use each string to create a vector or regexs so we don't have to recreate
    // them for each time we want to use them.

    boost::regex::flag_type my_flags = {boost::regex_constants::normal | boost::regex_constants::icase};

    shares_matchers_.emplace_back("r07", std::make_unique<boost::regex const>(s07, my_flags));
    shares_matchers_.emplace_back("r42", std::make_unique<boost::regex const>(s42, my_flags));
    shares_matchers_.emplace_back("r40", std::make_unique<boost::regex const>(s40, my_flags));
    shares_matchers_.emplace_back("r41", std::make_unique<boost::regex const>(s41, my_flags));
    shares_matchers_.emplace_back("r60", std::make_unique<boost::regex const>(s60, my_flags));
    shares_matchers_.emplace_back("r64", std::make_unique<boost::regex const>(s64, my_flags));
    shares_matchers_.emplace_back("r62", std::make_unique<boost::regex const>(s62, my_flags));
    shares_matchers_.emplace_back("r50", std::make_unique<boost::regex const>(s50, my_flags));
    shares_matchers_.emplace_back("r70", std::make_unique<boost::regex const>(s70, my_flags));
    shares_matchers_.emplace_back("r72", std::make_unique<boost::regex const>(s72, my_flags));
    shares_matchers_.emplace_back("r06", std::make_unique<boost::regex const>(s06, my_flags));
    shares_matchers_.emplace_back("r05", std::make_unique<boost::regex const>(s05, my_flags));
    shares_matchers_.emplace_back("r02", std::make_unique<boost::regex const>(s02, my_flags));
    shares_matchers_.emplace_back("r04", std::make_unique<boost::regex const>(s04, my_flags));
    shares_matchers_.emplace_back("r34", std::make_unique<boost::regex const>(s34, my_flags));
    shares_matchers_.emplace_back("r37", std::make_unique<boost::regex const>(s37, my_flags));
    shares_matchers_.emplace_back("r35", std::make_unique<boost::regex const>(s35, my_flags));
    shares_matchers_.emplace_back("r09", std::make_unique<boost::regex const>(s09, my_flags));
    shares_matchers_.emplace_back("r30", std::make_unique<boost::regex const>(s30, my_flags));
    shares_matchers_.emplace_back("r10", std::make_unique<boost::regex const>(s10, my_flags));
    shares_matchers_.emplace_back("r80", std::make_unique<boost::regex const>(s80, my_flags));
    shares_matchers_.emplace_back("r81", std::make_unique<boost::regex const>(s81, my_flags));
    shares_matchers_.emplace_back("r84", std::make_unique<boost::regex const>(s84, my_flags));
    shares_matchers_.emplace_back("r86", std::make_unique<boost::regex const>(s86, my_flags));
    shares_matchers_.emplace_back("r03", std::make_unique<boost::regex const>(s03, my_flags));
    shares_matchers_.emplace_back("r88", std::make_unique<boost::regex const>(s88, my_flags));
    shares_matchers_.emplace_back("r89", std::make_unique<boost::regex const>(s89, my_flags));
    shares_matchers_.emplace_back("r90", std::make_unique<boost::regex const>(s90, my_flags));
    shares_matchers_.emplace_back("r91", std::make_unique<boost::regex const>(s91, my_flags));


    // now, our stop words

    std::ifstream stop_words_file{"/usr/share/nltk_data/corpora/stopwords/english"};
    std::copy(std::istream_iterator<std::string>(stop_words_file), std::istream_iterator<std::string>(), std::back_inserter(stop_words_));
    stop_words_file.close();
}  // -----  end of method SharesOutstanding::SharesOutstanding  (constructor)  ----- 

int64_t SharesOutstanding::operator() (EM::sv html) const
{
    static const std::string delete_this{""};
    static const boost::regex regex_comma_parens{R"***([,)(])***"};

    GumboOptions options = kGumboDefaultOptions;
    std::unique_ptr<GumboOutput, std::function<void(GumboOutput*)>> output(gumbo_parse_with_options(&options, html.data(), html.length()),
            [&options](GumboOutput* output){ gumbo_destroy_output(&options, output); });

    const std::string the_text = ParseHTML(html);

    boost::smatch the_shares;
    bool found_it = false;
    std::string found_name;

    for (const auto& [name, regex] : shares_matchers_)
    {
        if (boost::regex_search(the_text, the_shares, *regex))
        {
            found_it = true;
            found_name = name;
            break;
        }
    }

    int64_t result = -1;

    if (found_it)
    {
        EM::sv xx(the_text.data() + the_shares.position() - 100, the_shares.length() + 200);
        spdlog::debug(catenate("Found: ", found_name, '\t', xx, " : ", the_shares.str(1)));

        std::string shares_outstanding = the_shares.str(1);
        shares_outstanding = boost::regex_replace(shares_outstanding, regex_comma_parens, delete_this);

        if (auto [p, ec] = std::from_chars(shares_outstanding.data(), shares_outstanding.data() + shares_outstanding.size(), result); ec != std::errc())
        {
            throw ExtractorException(catenate("Problem converting shares outstanding: ",
                        std::make_error_code(ec).message(), '\n'));
        }
    }
    return result;
}		// -----  end of method SharesOutstanding::operator()  ----- 

std::string SharesOutstanding::CleanText(GumboNode* node, size_t max_length_to_clean) const
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

std::vector<EM::sv> SharesOutstanding::FindCandidates(const std::string& the_text) const
{
    static const std::string a1 = R"***((?:(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b).{1,50}?\bshares\b))***";
    static const std::string a2 = R"***((?:\bshares\b.{1,50}?(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)))***";
    static const boost::regex regex_shares_only1{a1, boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_shares_only2{a2, boost::regex_constants::normal | boost::regex_constants::icase};

    std::vector<EM::sv> results;

    boost::sregex_iterator iter1(the_text.begin(), the_text.end(), regex_shares_only1);
    std::for_each(iter1, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        EM::sv xx(the_text.data() + m.position() - 100, m.length() + 200);
        results.push_back(xx);
    });

    boost::sregex_iterator iter2(the_text.begin(), the_text.end(), regex_shares_only2);
    std::for_each(iter2, boost::sregex_iterator{}, [&the_text, &results] (const boost::smatch& m)
    {
        EM::sv xx(the_text.data() + m.position() - 100, m.length() + 200);
        results.push_back(xx);
    });

    return results;
}		// -----  end of method SharesOutstanding::FindCandidates  ----- 

std::string SharesOutstanding::ParseHTML (EM::sv html, size_t max_length_to_parse, size_t max_length_to_clean) const
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

std::vector<std::string> SharesOutstanding::CreateRawWordList (const std::vector<EM::sv>& candidates)
{
    // first, make sure each candidate starts and ends with a space (no partial words)
    
    auto trim_text_and_convert([](const auto& candidate) 
    {
        return ranges::views::trim(candidate, [](char c) { return c != ' '; })
            | ranges::to<std::string>();
    }); 

    std::vector<std::string> results = candidates
        | ranges::views::transform(trim_text_and_convert)
        | ranges::to<std::vector>();

    // next, we would split into words, remove stop words, maybe lematize...
    // But, since our 'documents' are short, we'll just go with splitting into words
    // AND removing stop words.
    
    auto non_stop_words =
        ranges::views::transform([](const auto&word_rng)
            {
                std::string word(&*ranges::begin(word_rng), ranges::distance(word_rng));
                for (char& c : word)
                {
                    c = tolower(c);
                }
                return word;
            })
        | ranges::views::filter([this](const auto& word)
            {
                return ranges::find(stop_words_, word) == ranges::end(stop_words_);
            });

    std::vector<std::string> words;

    for(const auto& result : results)
    {
        auto word_rngs = result | ranges::views::split(' ');
        ranges::copy(word_rngs | non_stop_words, ranges::back_inserter(words));
    }

    std::cout << (ranges::views::all(words)) << '\n';

    return words;
}		// -----  end of method SharesOutstanding::PrepareCandidatesForVectorization  ----- 

