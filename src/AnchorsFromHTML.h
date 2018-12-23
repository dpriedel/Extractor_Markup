/*
 * =====================================================================================
 *
 *       Filename:  AnchorsFromHTML.h
 *
 *    Description:  Range compatible class to extract anchors
 *                  from a chunk of HTML
 *
 *        Version:  1.0
 *        Created:  12/14/2018 03:41:02 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
 *        License:  GNU General Public License v3
 *   Organization:  
 *
 * =====================================================================================
 */

	/* This file is part of ExtractEDGARData. */

	/* ExtractEDGARData is free software: you can redistribute it and/or modify */
	/* it under the terms of the GNU General Public License as published by */
	/* the Free Software Foundation, either version 3 of the License, or */
	/* (at your option) any later version. */

	/* ExtractEDGARData is distributed in the hope that it will be useful, */
	/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
	/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
	/* GNU General Public License for more details. */

	/* You should have received a copy of the GNU General Public License */
	/* along with ExtractEDGARData.  If not, see <http://www.gnu.org/licenses/>. */


#ifndef  _ANCHORSFROMHTML_INC_
#define  _ANCHORSFROMHTML_INC_

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
 
using sview = std::string_view;

struct AnchorData
{
    std::string href;
    std::string name;
    std::string text;
    sview anchor_content;
    sview html_document;
};				/* ----------  end of struct AnchorData  ---------- */

using AnchorList = std::vector<AnchorData>;

/*
 * =====================================================================================
 *        Class:  AnchorsFromHTML
 *  Description:  Range compatible class to extract anchors from a 
 *                  chunk of HTML
 * =====================================================================================
 */
class AnchorsFromHTML
{
public:

    class iterator: public std::iterator<
                    std::forward_iterator_tag,      // iterator_category
                    AnchorData                      // value_type
                    >
    {
        sview html_;
        const char* anchor_begin_ = nullptr;
        const char* anchor_end_ = nullptr;
        mutable AnchorData the_anchor_;

        std::optional<AnchorData> FindNextAnchor(const char* start, const char* end);
        const char* FindAnchorEnd(const char* start, const char* end, int level);
        AnchorData ExtractDataFromAnchor (const char* start, const char* end, sview html);
        
    public:

        iterator() = default;
        explicit iterator(sview html);

        iterator& operator++();
        iterator operator++(int) { iterator retval = *this; ++(*this); return retval; }

        bool operator==(iterator other) const
            { return anchor_begin_ == other.anchor_begin_ && anchor_end_ == other.anchor_end_; }
        bool operator!=(iterator other) const { return !(*this == other); }

        reference operator*() const { return the_anchor_; }
    };

    public:
        /* ====================  LIFECYCLE     ======================================= */
        AnchorsFromHTML (sview html);                             /* constructor */

        /* ====================  ACCESSORS     ======================================= */

    iterator begin() const;
    iterator end() const;

        /* ====================  MUTATORS      ======================================= */

        /* ====================  OPERATORS     ======================================= */

    protected:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

    private:
        /* ====================  METHODS       ======================================= */

        /* ====================  DATA MEMBERS  ======================================= */

        sview html_;

}; /* -----  end of class AnchorsFromHTML  ----- */


#endif   /* ----- #ifndef ANCHORSFROMHTML_INC  ----- */
