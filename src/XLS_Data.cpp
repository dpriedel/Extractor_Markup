// =====================================================================================
//
//       Filename:  XLS_Data.cpp
//
//    Description:  range compatible access to XBRL XLS data
//
//        Version:  1.0
//        Created:  04/18/2020 09:37:44 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

#include <algorithm>
#include <cctype>

#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/transform.hpp>
#include <range/v3/iterator.hpp>
#include "XLS_Data.h"

#include "Extractor_Utils.h"


//--------------------------------------------------------------------------------------
//       Class:  XLS_File
//      Method:  XLS_File
// Description:  constructor
//--------------------------------------------------------------------------------------

XLS_File::XLS_File (const std::vector<char>& content)
    : content_{content}
{
}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::XLS_File (std::vector<char>&& content)
    : content_{std::move(content)}
{
}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::XLS_File (const XLS_File& rhs)
    : content_{rhs.content_}
{
}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::XLS_File (XLS_File&& rhs) noexcept
    : content_{std::move(rhs.content_)}
{
}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::iterator XLS_File::begin ()
{
    if (content_.empty())
    {
        return {};
    }

    return {&content_} ;
}		// -----  end of method XLS_File::begin  ----- 

XLS_File::const_iterator XLS_File::begin () const
{
    if (content_.empty())
    {
        return {};
    }

    return {&content_} ;
}		// -----  end of method XLS_File::begin  ----- 

XLS_File::iterator XLS_File::end ()
{
    return {};

}		// -----  end of method XLS_File::end  ----- 

XLS_File::const_iterator XLS_File::end () const
{
    return {};

}		// -----  end of method XLS_File::end  ----- 

XLS_File& XLS_File::operator= (const XLS_File& rhs)
{
    if (&rhs != this)
    {
        content_ = rhs.content_;
    }
    return *this;
}		// -----  end of method XLS_File::operator=  ----- 

XLS_File& XLS_File::operator= (XLS_File&& rhs) noexcept
{
    if (&rhs != this)
    {
        content_ = std::move(rhs.content_);
    }
    return *this;
}		// -----  end of method XLS_File::operator=  ----- 

std::vector<std::string> XLS_File::GetSheetNames() const
{
    if (content_.empty())
    {
        return {};
    }

    auto file_closer = [](xlsxioreader file) { xlsxioread_close(file); };
    std::unique_ptr<xlsxio_read_struct, std::function<void(xlsxioreader)>>
        xlsxioread {xlsxioread_open_memory (const_cast<char*>(content_.data()), content_.size(), 0), file_closer};

    std::vector<std::string> results;

    if (xlsxioread)
    {
        auto list_closer = [](xlsxioreadersheetlist sheet_list) { xlsxioread_sheetlist_close(sheet_list); };

        std::unique_ptr<xlsxio_read_sheetlist_struct, std::function<void(xlsxioreadersheetlist)>> temp_list =
            {xlsxioread_sheetlist_open(xlsxioread.get()), list_closer};
        if (temp_list)
        {
            const XLSXIOCHAR* tempname = nullptr;
            while ((tempname = xlsxioread_sheetlist_next(temp_list.get())) != nullptr)
            {
                results.emplace_back(std::string{tempname});
                results.back() |= rng::actions::transform([](unsigned char c) { return std::tolower(c); });
            }
        }
    }
    return results;
}		// -----  end of method XLS_File::GetSheetNames  ----- 

std::optional<XLS_Sheet> XLS_File::FindSheetByName (EM::sv sheet_name) const
{
    std::string looking_for{sheet_name};
    looking_for |= rng::actions::transform([](unsigned char c) { return std::tolower(c); });

    if (! sheet_name.empty())
    {
        auto pos = rng::find_if(*this, [looking_for] (auto& x) { return x.GetSheetName() == looking_for; });
        if (pos != this->end())
        {
            return std::optional{*pos};
        }
    }
    return std::nullopt;
}		// -----  end of method XLS_File::FindSheetByName  ----- 

std::optional<XLS_Sheet> XLS_File::FindSheetByInternalName (EM::sv sheet_name) const
{
    std::string looking_for{sheet_name};
    looking_for |= rng::actions::transform([](unsigned char c) { return std::tolower(c); });

    if (! sheet_name.empty())
    {
        auto pos = rng::find_if(*this, [looking_for] (auto& x) { return x.GetSheetNameFromInside() == looking_for; });
        if (pos != this->end())
        {
            return std::optional{*pos};
        }
    }
    return std::nullopt;
}		// -----  end of method XLS_File::FindSheetByName  ----- 


//--------------------------------------------------------------------------------------
//       Class:  XLS_File::sheet_itor
//      Method:  XLS_File::sheet_itor
// Description:  constructor
//--------------------------------------------------------------------------------------

XLS_File::sheet_itor::sheet_itor (const std::vector<char>* content)
    : content_{content}
{
    if (content_ == nullptr)
    {
        return;
    }

    // it's possible we won't be able to read the data in which case
    // our handle will be null.  All downstream classes should be prepared for this.

    xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0);

    if (xlsxioread_ == nullptr)
    {
        return;
    }
    sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
    if (sheet_list_ == nullptr)
    {
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        return;
    }

    sheet_name_ = xlsxioread_sheetlist_next(sheet_list_);
    if (sheet_name_ == nullptr)
    {
        // end of sheets
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        return;
    }

    current_sheet_ = {content_, sheet_name_};

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::sheet_itor (const sheet_itor& rhs)
    : content_{rhs.content_}
{
    if (content_ == nullptr || rhs.sheet_name_ == nullptr)
    {
        return;
    }

    xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0);

    if (xlsxioread_ == nullptr)
    {
        return;
    }

    sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
    if (sheet_list_ == nullptr)
    {
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        return;
    }

    // need to match position of rhs.

    while (current_sheet_ != rhs.current_sheet_)
    {
        this->operator ++();
    }

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::sheet_itor (sheet_itor&& rhs) noexcept

{
    content_ = rhs.content_;
    sheet_name_ = rhs.sheet_name_;
    xlsxioread_ = rhs.xlsxioread_;
    sheet_list_ = rhs.sheet_list_;
    current_sheet_ = std::move(rhs.current_sheet_);

    rhs.content_ = nullptr;
    rhs.sheet_name_ = nullptr;
    rhs.xlsxioread_ = nullptr;
    rhs.sheet_list_ = nullptr;

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::~sheet_itor ()
{
    if (sheet_list_ != nullptr)
    {
        xlsxioread_sheetlist_close(sheet_list_);
    }
    if (xlsxioread_ != nullptr)
    {
        xlsxioread_close(xlsxioread_);
    }
}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor& XLS_File::sheet_itor::operator = (const sheet_itor& rhs)
{
    if (&rhs != this)
    {
        if (sheet_list_ != nullptr)
        {
            xlsxioread_sheetlist_close(sheet_list_);
            sheet_list_ = nullptr;
        }
        if (xlsxioread_ != nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
        }
        sheet_name_ = nullptr;
        current_sheet_ = {};

        content_ = rhs.content_;

        xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0);

        if (xlsxioread_ == nullptr)
        {
            return *this;
        }

        sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
        if (sheet_list_ == nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
            return *this;
        }

        // need to match position of rhs.

        while (true)
        {
            sheet_name_ = xlsxioread_sheetlist_next(sheet_list_);
            if (sheet_name_ == nullptr)
            {
                break;
            }
            if (strcmp(sheet_name_, rhs.sheet_name_) == 0)
            {
                break;
            }
        };

        if (sheet_name_ == nullptr)
        {
            // end of sheets
            xlsxioread_sheetlist_close(sheet_list_);
            sheet_list_ = nullptr;
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
            return *this;
        }

        current_sheet_ = {content_, sheet_name_};
    }
    return *this;
}		// -----  end of method XLS_File::sheet_itor::operator =  ----- 

XLS_File::sheet_itor& XLS_File::sheet_itor::operator = (sheet_itor&& rhs)
{
    if (&rhs != this)
    {
        if (sheet_list_ != nullptr)
        {
            xlsxioread_sheetlist_close(sheet_list_);
            sheet_list_ = nullptr;
        }
        if (xlsxioread_ != nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
        }
        sheet_name_ = nullptr;
        current_sheet_ = {};

        content_ = rhs.content_;
        sheet_name_ = rhs.sheet_name_;
        xlsxioread_ = rhs.xlsxioread_;
        sheet_list_ = rhs.sheet_list_;
        current_sheet_ = std::move(rhs.current_sheet_);

        rhs.content_ = nullptr;
        rhs.sheet_name_ = nullptr;
        rhs.xlsxioread_ = nullptr;
        rhs.sheet_list_ = nullptr;
    }
    return *this;
}		// -----  end of method XLS_File::sheet_itor::operator =  ----- 


XLS_File::sheet_itor& XLS_File::sheet_itor::operator++ ()
{
    sheet_name_ = xlsxioread_sheetlist_next(sheet_list_);
    if (sheet_name_ == nullptr)
    {
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        current_sheet_ = {};
        return *this;
    }

    current_sheet_ = {content_, sheet_name_};
    return *this;
}		// -----  end of method XLS_File::sheet_itor::operator++  ----- 


//--------------------------------------------------------------------------------------
//       Class:  XLS_Sheet
//      Method:  XLS_Sheet
// Description:  constructor
//--------------------------------------------------------------------------------------
XLS_Sheet::XLS_Sheet (const std::vector<char>* content, const XLSXIOCHAR* sheet_name)
    : content_{content}, sheet_name_mc_{sheet_name}
{
    rng::transform(sheet_name_mc_, rng::back_inserter(sheet_name_lc_), [](unsigned char c) { return std::tolower(c); });

}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet::XLS_Sheet (const XLS_Sheet& rhs)
    : content_{rhs.content_}, sheet_name_mc_{rhs.sheet_name_mc_},
    extended_sheet_name_{rhs.extended_sheet_name_}, sheet_name_lc_{rhs.sheet_name_lc_}
{
}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet::XLS_Sheet (XLS_Sheet&& rhs) noexcept
    : content_{rhs.content_}, extended_sheet_name_{std::move(rhs.extended_sheet_name_)},
    sheet_name_mc_{std::move(rhs.sheet_name_mc_)}, sheet_name_lc_{std::move(rhs.sheet_name_lc_)}
{
    rhs.content_ = nullptr;

}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet& XLS_Sheet::operator = (const XLS_Sheet& rhs)
{
    if (&rhs != this)
    {
        content_ = rhs.content_;
        extended_sheet_name_ = rhs.extended_sheet_name_;
        sheet_name_mc_ = rhs.sheet_name_mc_;
        sheet_name_lc_ = rhs.sheet_name_lc_;
    }
    return *this;
}		// -----  end of method XLS_Sheet::operator =  ----- 

XLS_Sheet& XLS_Sheet::operator = (XLS_Sheet&& rhs)
{
    if (&rhs != this)
    {
        content_ = rhs.content_;
        extended_sheet_name_ = std::move(rhs.extended_sheet_name_);
        sheet_name_mc_ = std::move(rhs.sheet_name_mc_);
        sheet_name_lc_ = std::move(rhs.sheet_name_lc_);

        rhs.content_ = nullptr;
    }
    return *this;
}		// -----  end of method XLS_Sheet::operator =  ----- 

const std::string& XLS_Sheet::GetSheetNameFromInside () const
{
    // we need to go and read the cells from our sheet
    // and get the content of the first cell.

    if (! extended_sheet_name_.empty())
    {
        // use cached result

        return extended_sheet_name_;
    }

    auto file_closer = [](xlsxioreader file) { xlsxioread_close(file); };
    std::unique_ptr<xlsxio_read_struct, std::function<void(xlsxioreader)>>
        xlsxioread {xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0), file_closer};

    auto sheet_closer = [](xlsxioreadersheet sheet_reader) { xlsxioread_sheet_close(sheet_reader); };

    if (xlsxioread && ! sheet_name_mc_.empty())
    {
        std::unique_ptr<xlsxio_read_sheet_struct, std::function<void(xlsxioreadersheet)>> temp_sheet =
            {xlsxioread_sheet_open(xlsxioread.get(), sheet_name_mc_.c_str(), 0), sheet_closer};
        if (temp_sheet)
        {
            row_itor itor{content_, sheet_name_mc_};
            if (itor != row_itor{})
            {
                // tab delimited content
                std::string first_row = *itor;
                if (! first_row.empty())
                {
                    extended_sheet_name_ = first_row.substr(0, first_row.find('\t'));
                    extended_sheet_name_ |= rng::actions::transform([](unsigned char c) { return std::tolower(c); });
                }
            }
        }
    }
    return extended_sheet_name_;
}		// -----  end of method XLS_Sheet::GetSheetNameFromInside  ----- 

XLS_Sheet::iterator XLS_Sheet::begin ()
{
    return {content_, sheet_name_mc_};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::const_iterator XLS_Sheet::begin () const
{
    return {content_, sheet_name_mc_};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::iterator XLS_Sheet::end ()
{
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::const_iterator XLS_Sheet::end () const
{
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

//--------------------------------------------------------------------------------------
//       Class:  XLS_Sheet::row_itor
//      Method:  XLS_Sheet::row_itor
// Description:  constructor
//--------------------------------------------------------------------------------------
XLS_Sheet::row_itor::row_itor (const std::vector<char>* content, const std::string& sheet_name)
    : content_{content}, sheet_name_mc_{sheet_name}
{
    if (content == nullptr || sheet_name.empty())
    {
        content_ = nullptr;
        sheet_name_mc_ = {};
        return;
    }
    xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0);

    if (xlsxioread_ == nullptr)
    {
        content_ = nullptr;
        sheet_name_mc_ = {};
        return;
    }

    current_sheet_ = xlsxioread_sheet_open(xlsxioread_, sheet_name_mc_.c_str(), 0);
    if (current_sheet_ == nullptr)
    {
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        content_ = nullptr;
        sheet_name_mc_ = {};
        return;
    }

    this->operator++();

}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor::row_itor (const row_itor& rhs)
    : row_itor(rhs.content_, rhs.sheet_name_mc_)
{
    while (current_row_ != rhs.current_row_)
    {
        this->operator++();
    }
}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor::row_itor (row_itor&& rhs) noexcept
    : content_{rhs.content_}, xlsxioread_{rhs.xlsxioread_}, current_sheet_{rhs.current_sheet_},   sheet_name_mc_{std::move(rhs.sheet_name_mc_)}, current_row_{std::move(rhs.current_row_)}
{
    rhs.content_ = nullptr;
    rhs.xlsxioread_ = nullptr;
    rhs.current_sheet_ = nullptr;

}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor::~row_itor ()
{
    if (current_sheet_ != nullptr)
    {
        xlsxioread_sheet_close(current_sheet_);
    }
    if (xlsxioread_ != nullptr)
    {
        xlsxioread_close(xlsxioread_);
    }

}  // -----  end of method XLS_Sheet::XLS_Sheet  (destructor)  ----- 

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator=(const row_itor& rhs) 
{
    if (&rhs != this)
    {
        if (current_sheet_ != nullptr)
        {
            xlsxioread_sheet_close(current_sheet_);
            current_sheet_ = nullptr;
        }
        if (xlsxioread_ != nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
        }
        current_row_.clear();
        content_ = rhs.content_;
        sheet_name_mc_ = rhs.sheet_name_mc_;

        xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_->data()), content_->size(), 0);

        if (xlsxioread_ == nullptr || sheet_name_mc_.empty())
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
            return *this;
        }
        current_sheet_ = xlsxioread_sheet_open(xlsxioread_, sheet_name_mc_.c_str(), 0);
        if (current_sheet_ == nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
            sheet_name_mc_ = {};
            return *this;
        }
        // we need to match up to the rhs
        //
        while (current_row_ != rhs.current_row_)
        {
            this->operator++();
        }
    }
    return *this;
}

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator=(row_itor&& rhs) noexcept
{
    if (&rhs != this)
    {
        if (current_sheet_ != nullptr)
        {
            xlsxioread_sheet_close(current_sheet_);
            current_sheet_ = nullptr;
        }
        if (xlsxioread_ != nullptr)
        {
            xlsxioread_close(xlsxioread_);
            xlsxioread_ = nullptr;
        }

        content_ = rhs.content_;
        xlsxioread_ = rhs.xlsxioread_;
        current_sheet_ = rhs.current_sheet_;
        sheet_name_mc_ = std::move(rhs.sheet_name_mc_);
        current_row_ = std::move(rhs.current_row_);

        rhs.content_ = nullptr;
        rhs.xlsxioread_ = nullptr;
        rhs.current_sheet_ = nullptr;
    }
    return *this;
}

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator++ ()
{
    if (current_sheet_ == nullptr)
    {
        return *this;
    }

    auto got_row = xlsxioread_sheet_next_row(current_sheet_);
    if (! got_row)
    {
        xlsxioread_sheet_close(current_sheet_);
        current_sheet_ = nullptr;
        xlsxioread_close(xlsxioread_);
        xlsxioread_ = nullptr;
        content_ = nullptr;
        current_row_.clear();
        sheet_name_mc_ = {};
        return *this;
    }

    current_row_.clear();

    // let's build this just once
    auto cell_deleter = [](XLSXIOCHAR* cell) { if (cell) free(cell); };

    while (true)
    {
        std::unique_ptr<XLSXIOCHAR, std::function<void(XLSXIOCHAR*)>> next_cell =
            {xlsxioread_sheet_next_cell(current_sheet_), cell_deleter};
        if (! next_cell)
        {
            break;
        }
        current_row_ += next_cell.get();
        current_row_ += '\t';
    }
    current_row_ += '\n';
    return *this;
}		// -----  end of method XLS_Sheet::row_itor::operator++  ----- 

