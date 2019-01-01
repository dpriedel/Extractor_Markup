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

    class anchor_itor: public std::iterator<
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

        anchor_itor() = default;
        explicit anchor_itor(sview html);

        anchor_itor& operator++();
        anchor_itor operator++(int) { anchor_itor retval = *this; ++(*this); return retval; }

        bool operator==(anchor_itor other) const
            { return anchor_begin_ == other.anchor_begin_ && anchor_end_ == other.anchor_end_; }
        bool operator!=(anchor_itor other) const { return !(*this == other); }

        reference operator*() const { return the_anchor_; }
    };

      typedef sview					value_type;
      typedef typename anchor_itor::pointer			pointer;
      typedef typename anchor_itor::pointer	const_pointer;
      typedef typename anchor_itor::reference		reference;
      typedef typename anchor_itor::reference	const_reference;
      typedef anchor_itor                          iterator;
      typedef anchor_itor                    const_iterator;
      typedef size_t					size_type;
      typedef ptrdiff_t					difference_type;

    public:
        /* ====================  LIFECYCLE     ======================================= */
        AnchorsFromHTML (sview html);                             /* constructor */

        /* ====================  ACCESSORS     ======================================= */

    const_iterator begin() const;
    const_iterator end() const;

    size_type size() const { return std::distance(begin(), end()); }
    bool empty() const { return html_.empty(); }

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
