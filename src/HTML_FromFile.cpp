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
 *   Organization:  
 *
 * =====================================================================================
 */

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


HTML_FromFile::iterator HTML_FromFile::begin ()
{
    iterator it{file_content_};
    return it;
}		/* -----  end of method HTML_FromFile::begin  ----- */

HTML_FromFile::iterator HTML_FromFile::end ()
{
    iterator it;
    return it;
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

