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
AnchorsFromHTML::AnchorsFromHTML (sview html)
    : html_{html}
{
}  /* -----  end of method AnchorsFromHTML::AnchorsFromHTML  (constructor)  ----- */

AnchorsFromHTML::const_iterator AnchorsFromHTML::begin () const
{
    return iterator(html_);
}		/* -----  end of method AnchorsFromHTML::begin  ----- */

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
AnchorsFromHTML::anchor_itor::anchor_itor (sview html)
    : html_{html}
{
    if (html_.empty())
    {
        return;
    }
    anchor_begin_ = html_.begin();
    anchor_end_ = html.end();

    operator++();

}  /* -----  end of method AnchorsFromHTML::iterator::AnchorsFromHTML::iterator  (constructor)  ----- */

AnchorsFromHTML::anchor_itor& AnchorsFromHTML::anchor_itor::operator++ ()
{
    auto next_anchor = FindNextAnchor(anchor_begin_, anchor_end_);

    if (! next_anchor)
    {
        anchor_begin_ = nullptr;
        anchor_end_ = nullptr;
        return *this;
    }

    the_anchor_ = *next_anchor;
    anchor_begin_ = the_anchor_.anchor_content_.end();

    return *this;
}		/* -----  end of method AnchorsFromHTML::iterator::operator++  ----- */

std::optional<AnchorData> AnchorsFromHTML::iterator::FindNextAnchor (const char* begin, const char* end)
{
    static const boost::regex re_anchor_begin{R"***((?:<a>|<a[^>])[^>]*?>)***",
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
    auto anchor{ExtractDataFromAnchor(anchor_begin_match[0].first, anchor_end , html_)};
    return std::optional<AnchorData>{anchor};
}		/* -----  end of method AnchorsFromHTML::iterator::FindNextAnchor  ----- */

const char* AnchorsFromHTML::iterator::FindAnchorEnd (const char* begin, const char* end, int level)
{
    // handle 'nested' anchors.

    static const boost::regex re_anchor_end_or_begin{R"***(</a>|(?:(?:<a>|<a ).*?>))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex re_anchor_begin{R"***((?:<a |<a>).*?>)***",
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

AnchorData AnchorsFromHTML::iterator::ExtractDataFromAnchor (const char* start, const char* end, sview html)
{
    CDocument whole_anchor;
    const std::string working_copy{start, end};
    whole_anchor.parse(working_copy);
    auto the_anchor = whole_anchor.find("a"s);

    AnchorData result{the_anchor.nodeAt(0).attribute("href"), the_anchor.nodeAt(0).attribute("name"),
        the_anchor.nodeAt(0).text(), sview(start, end - start), html};

    // href sometimes is quoted, so remove them.

    if (result.href_[0] == '"')
    {
        result.href_.erase(0, 1);
        result.href_.resize(result.href_.size() - 1);
    }
//    result.CleanData();
    return result;
}		/* -----  end of method AnchorsFromHTML::iterator::ExtractDataFromAnchor  ----- */

