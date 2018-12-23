/*
 * =====================================================================================
 *
 *       Filename:  TablesFromFile.cpp
 *
 *    Description:  Extract HTML tables from a block of text.
 *
 *        Version:  1.0
 *        Created:  12/21/2018 09:23:04 AM
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


#include "TablesFromFile.h"

using namespace std::string_literals;

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML
 *      Method:  TablesFromHTML
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::TablesFromHTML (sview html)
    : html_{html}
{
}  /* -----  end of method TablesFromHTML::TablesFromHTML  (constructor)  ----- */

TablesFromHTML::iterator TablesFromHTML::begin () const
{
    iterator it{html_};
    return it;
}		/* -----  end of method TablesFromHTML::begin  ----- */

TablesFromHTML::iterator TablesFromHTML::end () const
{
    return {};
}		/* -----  end of method TablesFromHTML::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  TablesFromHTML::iterator
 *      Method:  TablesFromHTML::iterator
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
TablesFromHTML::iterator::iterator(sview html)
    : html_{html}
{
    doc_ = boost::cregex_token_iterator(html_.cbegin(), html_.cend(), regex_table_);
    if (doc_ != end_)
    {
        current_table_ = sview(doc_->first, doc_->length());
    }
}  /* -----  end of method TablesFromHTML::iterator::iterator  (constructor)  ----- */

TablesFromHTML::iterator& TablesFromHTML::iterator::operator++ ()
{
    if (++doc_ != end_)
    {
        current_table_ = sview(doc_->first, doc_->length());
    }
    else
    {
        current_table_ = {};
    }
    return *this;
}		/* -----  end of method TablesFromHTML::iterator::operator++  ----- */

