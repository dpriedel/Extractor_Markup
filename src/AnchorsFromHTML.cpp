/*
 * =====================================================================================
 *
 *       Filename:  AnchorsFromHTML.cpp
 *
 *    Description:  Range compatible class to extract anchors
 *                  from a chunk of HTML
 *
 *        Version:  1.0
 *        Created:  12/14/2018 03:44:46 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
 *        License:  GNU General Public License v3
 *   Organization:  
 *
 * =====================================================================================
 */

	/* This file is part of Extractor_Markup. */

	/* Extractor_Markup is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* Extractor_Markup is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with Extractor_Markup.  If not, see <http://www.gnu.org/licenses/>. */

#include "AnchorsFromHTML.h"
#include "Extractor_Utils.h"

#include <boost/regex.hpp>

#include "spdlog/spdlog.h"

// gumbo-query

#include "gq/Document.h"
#include "gq/Node.h"
#include "gq/Selection.h"

using namespace std::string_literals;

/*
 *--------------------------------------------------------------------------------------
 *       Class:  AnchorsFromHTML
 *      Method:  AnchorsFromHTML
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
AnchorsFromHTML::AnchorsFromHTML (EM::HTMLContent html)
    : html_{html}
{
}  /* -----  end of method AnchorsFromHTML::AnchorsFromHTML  (constructor)  ----- */

AnchorsFromHTML::iterator AnchorsFromHTML::begin ()
{
    return iterator(this);
}		/* -----  end of method AnchorsFromHTML::begin  ----- */

AnchorsFromHTML::const_iterator AnchorsFromHTML::begin () const
{
    return const_iterator(this);
}		/* -----  end of method AnchorsFromHTML::begin  ----- */

AnchorsFromHTML::iterator AnchorsFromHTML::end ()
{
    return {};
}		/* -----  end of method AnchorsFromHTML::end  ----- */

AnchorsFromHTML::const_iterator AnchorsFromHTML::end () const
{
    return {};
}		/* -----  end of method AnchorsFromHTML::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  AnchorsFromHTML::iterator
 *      Method:  AnchorsFromHTML::iterator
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
AnchorsFromHTML::anchor_itor::anchor_itor (const AnchorsFromHTML* anchors)
    : anchors_{anchors}
{
    if (anchors == nullptr)
    {
        return;
    }
    html_ = anchors_->html_;
    auto val = html_.get();

    if (val.empty())
    {
        return;
    }
    anchor_search_start = val.begin();
    anchor_search_end = val.end();

    operator++();

}  /* -----  end of method AnchorsFromHTML::iterator::AnchorsFromHTML::iterator  (constructor)  ----- */

AnchorsFromHTML::anchor_itor& AnchorsFromHTML::anchor_itor::operator++ ()
{
    auto next_anchor = FindNextAnchor(anchor_search_start, anchor_search_end);

    if (! next_anchor)
    {
        anchor_search_start = nullptr;
        anchor_search_end = nullptr;
        the_anchor_ = {};
        return *this;
    }

    the_anchor_ = *next_anchor;
    anchor_search_start = the_anchor_.anchor_content_.get().end();

    return *this;
}		/* -----  end of method AnchorsFromHTML::iterator::operator++  ----- */

std::optional<AnchorData> AnchorsFromHTML::iterator::FindNextAnchor (const char* begin, const char* end)
{
    if (++using_saved_anchor < anchors_->found_anchors_.size())
    {
        return std::optional<AnchorData>{anchors_->found_anchors_[using_saved_anchor]};
    }
    static const boost::regex re_anchor_begin{R"***((?:<a>|<a |<a\n)[^>]*?>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    // we need to prime the pump by finding the beginning of the first anchor.

    boost::cmatch anchor_begin_match;
    bool found_it = boost::regex_search(begin, end, anchor_begin_match, re_anchor_begin);
    if (! found_it)
    {
        // we have no anchors in this document
        return std::nullopt;
    }

    auto anchor_end = FindAnchorEnd(anchor_begin_match[0].first,  end, 1);
    if (anchor_end == nullptr)
    {
        // this should not happen -- we have an incomplete anchor element.

        throw HTMLException("Missing anchor end.");
    }

    AnchorData anchor{ExtractDataFromAnchor(anchor_begin_match[0].first, anchor_end , html_)};
 
    // cache our anchor in case there is a 'next' time.

    anchors_->found_anchors_.push_back(anchor);
    return std::optional<AnchorData>{anchor};
}		/* -----  end of method AnchorsFromHTML::iterator::FindNextAnchor  ----- */

const char* AnchorsFromHTML::iterator::FindAnchorEnd (const char* begin, const char* end, int level)
{
    if (level >= 5)
    {   
        spdlog::info("Something wrong...anchors too deeply nested.");
        return nullptr;
    }

    // handle 'nested' anchors.

    static const boost::regex re_anchor_end_or_begin{R"***(</a>|(?:(?:<a>|<a |<a\n).*?>))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex re_anchor_begin{R"***((?:<a |<a>|<a\n).*?>)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    boost::cmatch anchor_end_or_begin;
    bool found_it = boost::regex_search(++begin, end, anchor_end_or_begin, re_anchor_end_or_begin);
    if (found_it)
    {
        // we have either an end anchor or begin anchor

        if (boost::regex_match(anchor_end_or_begin[0].first, anchor_end_or_begin[0].second, re_anchor_begin))
        {
            // found a nested anchor start

            return FindAnchorEnd(anchor_end_or_begin[0].first, end, ++level);
        }

        // at this point, we are working with an anchor end. let's see if we're done

        --level;
        if (level > 0)
        {
            // not finished but we have completed a nested anchor

            return FindAnchorEnd(anchor_end_or_begin[0].first, end, level);
        }

        // now I should be looking for the end of the top-level anchor.

        return anchor_end_or_begin[0].second;
    }
    return nullptr;
}		/* -----  end of method AnchorsFromHTML::iterator::FindAnchorEnd  ----- */

AnchorData AnchorsFromHTML::iterator::ExtractDataFromAnchor (const char* start, const char* end, EM::HTMLContent html)
{
    CDocument whole_anchor;
    const std::string working_copy{start, end};
    whole_anchor.parse(working_copy);
    auto the_anchor = whole_anchor.find("a"s);
    BOOST_ASSERT_MSG(the_anchor.nodeNum() > 0, "No anchor found in extractecd anchor!!");

    AnchorData result{the_anchor.nodeAt(0).attribute("href"), the_anchor.nodeAt(0).attribute("name"),
        the_anchor.nodeAt(0).text(), EM::AnchorContent{EM::sv(start, end - start)}, html};

    // href sometimes is quoted, so remove them.

    if (result.href_[0] == '"' || result.href_[0] == "(')"[0])
    {
        result.href_.erase(0, 1);
        result.href_.resize(result.href_.size() - 1);
    }
    // check for name too

    if (result.name_[0] == '"' || result.name_[0] == "(')"[0])
    {
        result.name_.erase(0, 1);
        result.name_.resize(result.name_.size() - 1);
    }
//    result.CleanData();
    return result;
}		/* -----  end of method AnchorsFromHTML::iterator::ExtractDataFromAnchor  ----- */

