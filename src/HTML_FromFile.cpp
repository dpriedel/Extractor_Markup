/*
 * =====================================================================================
 *
 *       Filename:  HTML_FromFile.cpp
 *
 *    Description:  class to make working with HTML content in EDGAR files
 *                  fit in better with STL and maybe ranges
 *
 *        Version:  1.0
 *        Created:  12/13/2018 02:31:12 PM
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


#include "HTML_FromFile.h"
#include "ExtractEDGAR_Utils.h"

#include <boost/algorithm/string/predicate.hpp>
/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTML_FromFile
 *      Method:  HTML_FromFile
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */

HTML_FromFile::HTML_FromFile (sview file_content)
    : file_content_{file_content}
{
}  /* -----  end of method HTML_FromFile::HTML_FromFile  (constructor)  ----- */


HTML_FromFile::iterator HTML_FromFile::begin () const
{
    iterator it{file_content_};
    return it;
}		/* -----  end of method HTML_FromFile::begin  ----- */

HTML_FromFile::iterator HTML_FromFile::end () const
{
    return {};
}		/* -----  end of method HTML_FromFile::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTML_FromFile::iterator
 *      Method:  HTML_FromFile::iterator
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
HTML_FromFile::iterator::iterator ()
{
}  /* -----  end of method HTML_FromFile::iterator::HTML_FromFile::iterator  (constructor)  ----- */

HTML_FromFile::iterator::iterator (sview file_content)
    : file_content_{file_content}
{
    doc_ = boost::cregex_token_iterator(file_content.cbegin(), file_content.cend(), regex_doc_);
    for (; doc_ != end_; ++doc_)
    {
        sview document(doc_->first, doc_->length());
        html_ = FindHTML(document);
        if (! html_.empty())
        {
            break;
        }
    }
}  /* -----  end of method HTML_FromFile::iterator::HTML_FromFile::iterator  (constructor)  ----- */

HTML_FromFile::iterator& HTML_FromFile::iterator::operator++ ()
{
    for (++doc_; doc_ != end_; ++doc_)
    {
        sview document(doc_->first, doc_->length());
        html_ = FindHTML(document);
        if (! html_.empty())
        {
            break;
        }
    }
    return *this;
}		/* -----  end of method HTML_FromFile::iterator::operator++  ----- */

