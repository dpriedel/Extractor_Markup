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
#include <functional>

#include "spdlog/spdlog.h"

#include "HTML_FromFile.h"

//--------------------------------------------------------------------------------------
//       Class:  SharesOutstanding
//      Method:  SharesOutstanding
// Description:  constructor
//--------------------------------------------------------------------------------------
SharesOutstanding::SharesOutstanding ()
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
    shares_matchers_.emplace_back("r30", std::make_unique<boost::regex const>(s30, my_flags));
    shares_matchers_.emplace_back("r10", std::make_unique<boost::regex const>(s10, my_flags));
    shares_matchers_.emplace_back("r80", std::make_unique<boost::regex const>(s80, my_flags));
    shares_matchers_.emplace_back("r81", std::make_unique<boost::regex const>(s81, my_flags));
    shares_matchers_.emplace_back("r84", std::make_unique<boost::regex const>(s84, my_flags));
    shares_matchers_.emplace_back("r86", std::make_unique<boost::regex const>(s86, my_flags));
    shares_matchers_.emplace_back("r03", std::make_unique<boost::regex const>(s03, my_flags));
    shares_matchers_.emplace_back("r88", std::make_unique<boost::regex const>(s88, my_flags));
    shares_matchers_.emplace_back("r89", std::make_unique<boost::regex const>(s89, my_flags));
    shares_matchers_.emplace_back("r09", std::make_unique<boost::regex const>(s09, my_flags));
    shares_matchers_.emplace_back("r90", std::make_unique<boost::regex const>(s90, my_flags));
    shares_matchers_.emplace_back("r91", std::make_unique<boost::regex const>(s91, my_flags));
}  // -----  end of method SharesOutstanding::SharesOutstanding  (constructor)  ----- 

int64_t SharesOutstanding::operator() (EM::sv html) const
{
    GumboOptions options = kGumboDefaultOptions;
    std::unique_ptr<GumboOutput, std::function<void(GumboOutput*)>> output(gumbo_parse_with_options(&options, html.data(), html.length()),
            [&options](GumboOutput* output){ gumbo_destroy_output(&options, output); });

    std::string the_text_whole = CleanText(output->root);
    std::string the_text = the_text_whole.substr(0, 100000);

    gumbo_destroy_output(&options, output.release());

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

        const std::string delete_this{""};
        const boost::regex regex_comma_parens{R"***([,)(])***"};

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

std::string SharesOutstanding::CleanText(GumboNode* node) const
{
    //    this code is based on example code in Gumbo Parser project

    const boost::regex regex_nbr{R"***(\b[1-9](?:[0-9]{0,2})(?:,[0-9]{3})+\b)***"};
    const boost::regex regex_hi_ascii{R"***([^\x00-\x7f])***"};
    const boost::regex regex_multiple_spaces{R"***( {2,})***"};
    const boost::regex regex_nl{R"***(\n{1,})***"};
    const std::string one_space = " ";

    if (node->type == GUMBO_NODE_TEXT)
    {
        std::string text(node->v.text.text);
        return boost::regex_replace(text, regex_hi_ascii, one_space);
    }
    if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag != GUMBO_TAG_SCRIPT && node->v.element.tag != GUMBO_TAG_STYLE)
    {
        std::string contents;
        GumboVector* children = &node->v.element.children;

        for (unsigned int i = 0; i < children->length; ++i)
        {
            const std::string text = CleanText((GumboNode*) children->data[i]);
            if (! text.empty())
            {
                if (boost::regex_match(text, regex_nbr))
                {
                    contents += ' ';
                }
                contents.append(text);
            }
        }
        contents += ' ';

        std::string result = boost::regex_replace(contents, regex_hi_ascii, one_space);
        result = boost::regex_replace(result, regex_multiple_spaces, one_space);
        result = boost::regex_replace(result, regex_nl, one_space);
        return result;
    }
    return {};
}		// -----  end of method SharesOutstanding::CleanText  ----- 

