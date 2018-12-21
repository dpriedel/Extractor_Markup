/*
 * =====================================================================================
 *
 *       Filename:  HTML_FromFile.h
 *
 *    Description:  class to make working with HTML content in EDGAR files
 *                  fit in better with STL and maybe ranges
 *
 *        Version:  1.0
 *        Created:  12/13/2018 02:26:57 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
 *        License:  GNU General Public License v3
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef  _HTML_FROMFILE_INC_
#define  _HTML_FROMFILE_INC_

#include <iterator>
#include <string>
#include <string_view>

using sview = std::string_view;

#include <boost/regex.hpp>

/*
 * =====================================================================================
 *        Class:  HTML_FromFile
 *  Description:  make HTML content available via iterators.
 *
 *  I'm using iterators for now because I'm not ready to jump fully into ranges.
 * =====================================================================================
 */
class HTML_FromFile
{
public:

    class iterator: public std::iterator<
                    std::forward_iterator_tag,      // iterator_category
                    sview                           // value_type
                    >
    {
        const boost::regex regex_doc_{R"***(<DOCUMENT>.*?</DOCUMENT>)***"};
        boost::cregex_token_iterator doc_;
        boost::cregex_token_iterator end_;

        sview file_content_;
        mutable sview html_;

    public:

        iterator();
        explicit iterator(sview file_content);

        iterator& operator++();
        iterator operator++(int) { iterator retval = *this; ++(*this); return retval; }

        bool operator==(iterator other) const { return doc_ == other.doc_; }
        bool operator!=(iterator other) const { return !(*this == other); }

        reference operator*() const { return html_; }
    };

public:
    /* ====================  LIFECYCLE     ======================================= */
    HTML_FromFile (sview file_content);                /* constructor */

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

    sview file_content_;

}; /* -----  end of class HTML_FromFile  ----- */

#endif   /* ----- #ifndef HTML_FromFile_INC  ----- */
