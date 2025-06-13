/*
 * =====================================================================================
 *
 *       Filename:  HTML_FromFile.cpp
 *
 *    Description:  class to make working with HTML content in SEC files
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

#include "HTML_FromFile.h"

#include "Extractor_Utils.h"

/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTML_FromFile
 *      Method:  HTML_FromFile
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */

HTML_FromFile::HTML_FromFile(EM::DocumentSectionList const *document_sections, EM::FileName document_name)
    : document_sections_{document_sections}, document_name_{document_name}
{
} /* -----  end of method HTML_FromFile::HTML_FromFile  (constructor)  ----- */

HTML_FromFile::iterator HTML_FromFile::begin()
{
    iterator it{document_sections_, document_name_};
    return it;
} /* -----  end of method HTML_FromFile::begin  ----- */

HTML_FromFile::const_iterator HTML_FromFile::begin() const
{
    const_iterator it{document_sections_, document_name_};
    return it;
} /* -----  end of method HTML_FromFile::begin  ----- */

HTML_FromFile::iterator HTML_FromFile::end()
{
    return {};
} /* -----  end of method HTML_FromFile::end  ----- */

HTML_FromFile::const_iterator HTML_FromFile::end() const
{
    return {};
} /* -----  end of method HTML_FromFile::end  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTML_FromFile::html_itor
 *      Method:  HTML_FromFile::html_itor
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
HTML_FromFile::html_itor::html_itor(EM::DocumentSectionList const *document_sections, EM::FileName document_name)
    : document_sections_{document_sections}, document_name_{document_name}
{
    for (++current_doc_; current_doc_ < document_sections_->size(); ++current_doc_)
    {
        html_info_.html_ = FindHTML((*document_sections_)[current_doc_], document_name_);
        if (!html_info_.html_.get().empty())
        {
            html_info_.document_ = (*document_sections_)[current_doc_];
            html_info_.file_name_ = FindFileName((*document_sections_)[current_doc_], document_name_);
            html_info_.file_type_ = FindFileType((*document_sections_)[current_doc_]);
            return;
        }
    }
    current_doc_ = -1;
} /* -----  end of method HTML_FromFile::html_itor::HTML_FromFile::html_itor  (constructor)  ----- */

HTML_FromFile::html_itor &HTML_FromFile::html_itor::operator++()
{
    for (++current_doc_; current_doc_ < document_sections_->size(); ++current_doc_)
    {
        html_info_.html_ = FindHTML((*document_sections_)[current_doc_], document_name_);
        if (!html_info_.html_.get().empty())
        {
            html_info_.document_ = (*document_sections_)[current_doc_];
            html_info_.file_name_ = FindFileName((*document_sections_)[current_doc_], document_name_);
            html_info_.file_type_ = FindFileType((*document_sections_)[current_doc_]);
            return *this;
        }
    }
    current_doc_ = -1;
    return *this;
} /* -----  end of method HTML_FromFile::iterator::operator++  ----- */
