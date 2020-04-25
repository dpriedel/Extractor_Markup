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

#include <cstring>

#include "XLS_Data.h"

#include "Extractor_Utils.h"


//--------------------------------------------------------------------------------------
//       Class:  XLS_File
//      Method:  XLS_File
// Description:  constructor
//--------------------------------------------------------------------------------------
XLS_File::XLS_File (XLS_File&& rhs) noexcept
    : content_{rhs.content_}, xlsxioread_{rhs.xlsxioread_}
{
    rhs.content_ = {};
    rhs.xlsxioread_ = nullptr;

}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::XLS_File (std::vector<char>&& content)
    : content_{content}
{
    // it's possible we won't be able to read the data in which case
    // our handle will be null.  All downstream classes should be prepared for this.

    xlsxioread_ = xlsxioread_open_memory (const_cast<char*>(content_.data()), content_.size(), 0);

}  // -----  end of method XLS_File::XLS_File  (constructor)  ----- 

XLS_File::~XLS_File ()
{
    xlsxioread_close(xlsxioread_);

}  // -----  end of method XLS_File::~XLS_File  (destructor)  ----- 

XLS_File::iterator XLS_File::begin ()
{
    if (xlsxioread_ == nullptr)
    {
        return {};
    }

    return {xlsxioread_} ;

}		// -----  end of method XLS_File::begin  ----- 

XLS_File::const_iterator XLS_File::begin () const
{
    if (xlsxioread_ == nullptr)
    {
        return {};
    }

    return {xlsxioread_} ;

}		// -----  end of method XLS_File::begin  ----- 

XLS_File::iterator XLS_File::end ()
{
    return {};

}		// -----  end of method XLS_File::end  ----- 

XLS_File::const_iterator XLS_File::end () const
{
    return {};

}		// -----  end of method XLS_File::end  ----- 



//--------------------------------------------------------------------------------------
//       Class:  XLS_File::sheet_itor
//      Method:  XLS_File::sheet_itor
// Description:  constructor
//--------------------------------------------------------------------------------------

XLS_File::sheet_itor::sheet_itor (xlsxioreader xlsxioread)
    : xlsxioread_{xlsxioread}
{
    if (xlsxioread_ != nullptr)
    {
        sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
        if (sheet_list_ == nullptr)
        {
            return;
        }
    }

    sheet_name_ = xlsxioread_sheetlist_next(sheet_list_);
    if (sheet_name_ == nullptr)
    {
        // end of sheets
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
        return;
    }

    current_sheet_ = {xlsxioread_, sheet_name_};

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::sheet_itor (const sheet_itor& rhs)
    : xlsxioread_{rhs.xlsxioread_}, sheet_name_{rhs.sheet_name_}
{
    if (xlsxioread_ != nullptr)
    {
        sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
        if (sheet_list_ == nullptr)
        {
            return;
        }
    }

    if (sheet_name_ == nullptr)
    {
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
        return;
    }

    // need to match position of rhs.

    const char* next_sheet = nullptr;
    do
    {
        next_sheet = xlsxioread_sheetlist_next(sheet_list_);
        if (strcmp(next_sheet, sheet_name_) == 0)
        {
            break;
        }
    } while (next_sheet != nullptr);

    current_sheet_ = {xlsxioread_, sheet_name_};

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::sheet_itor (sheet_itor&& rhs)

{
    if (sheet_list_ != nullptr)
    {
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
    }
    xlsxioread_ = rhs.xlsxioread_;
    sheet_list_ = rhs.sheet_list_;
    sheet_name_ = rhs.sheet_name_;
    current_sheet_ = rhs.current_sheet_;

    rhs.xlsxioread_ = nullptr;
    rhs.sheet_list_ = nullptr;
    rhs.sheet_name_ = nullptr;
    rhs.current_sheet_ = {};

}  // -----  end of method XLS_File::sheet_itor::sheet_itor  (constructor)  ----- 

XLS_File::sheet_itor::~sheet_itor ()
{
    if (sheet_list_ != nullptr)
    {
        xlsxioread_sheetlist_close(sheet_list_);
        sheet_list_ = nullptr;
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
            sheet_name_ = nullptr;
            current_sheet_ = {};
        }

        xlsxioread_ = rhs.xlsxioread_;
        if (xlsxioread_ != nullptr)
        {
            sheet_list_ = xlsxioread_sheetlist_open(xlsxioread_);
            if (sheet_list_ == nullptr)
            {
                return *this;
            }
        }

        sheet_name_ = rhs.sheet_name_;

        if (sheet_name_ == nullptr)
        {
            xlsxioread_sheetlist_close(sheet_list_);
            sheet_list_ = nullptr;
            return *this;
        }

        // need to match position or rhs.

        const char* next_sheet = nullptr;
        do
        {
            next_sheet = xlsxioread_sheetlist_next(sheet_list_);
            if (strcmp(next_sheet, sheet_name_) == 0)
            {
                break;
            }
        } while (next_sheet != nullptr);

        current_sheet_ = {xlsxioread_, sheet_name_};
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
        xlsxioread_ = rhs.xlsxioread_;
        sheet_list_ = rhs.sheet_list_;
        sheet_name_ = rhs.sheet_name_;
        current_sheet_ = rhs.current_sheet_;

        rhs.xlsxioread_ = nullptr;
        rhs.sheet_list_ = nullptr;
        rhs.sheet_name_ = nullptr;
        rhs.current_sheet_ = {};
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
        current_sheet_ = {};
        return *this;
    }

    current_sheet_ = {xlsxioread_, sheet_name_};
    return *this;
}		// -----  end of method XLS_File::sheet_itor::operator++  ----- 



//--------------------------------------------------------------------------------------
//       Class:  XLS_Sheet
//      Method:  XLS_Sheet
// Description:  constructor
//--------------------------------------------------------------------------------------
XLS_Sheet::XLS_Sheet (xlsxioreader xlsxioread, const char* sheet_name)
    : xlsxioread_{xlsxioread}
{
    if (xlsxioread_ != nullptr && sheet_name != nullptr)
    {
        current_sheet_ = xlsxioread_sheet_open(xlsxioread_, sheet_name, 0);
        if (current_sheet_ != nullptr)
        {
            sheet_name_ = sheet_name;
        }
    }
    
}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet::XLS_Sheet (const XLS_Sheet& rhs)
    : xlsxioread_{rhs.xlsxioread_}, sheet_name_{rhs.sheet_name_}
{
    if (current_sheet_ != nullptr)
    {
        xlsxioread_sheet_close(current_sheet_);
        current_sheet_ = nullptr;
    }
    if (xlsxioread_ != nullptr && ! sheet_name_.empty())
    {
        current_sheet_ = xlsxioread_sheet_open(xlsxioread_, sheet_name_.data(), 0);
    }
}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet::XLS_Sheet (XLS_Sheet&& rhs)
    : xlsxioread_{rhs.xlsxioread_}, sheet_name_{rhs.sheet_name_}
{
    if (current_sheet_ != nullptr)
    {
        xlsxioread_sheet_close(current_sheet_);
        current_sheet_ = nullptr;
    }

    current_sheet_ = rhs.current_sheet_;

    rhs.xlsxioread_ = nullptr;
    rhs.current_sheet_ = nullptr;
    rhs.sheet_name_ = {};

}  // -----  end of method XLS_Sheet::XLS_Sheet  (constructor)  ----- 

XLS_Sheet::~XLS_Sheet ()
{
    if (current_sheet_ != nullptr)
    {
        xlsxioread_sheet_close(current_sheet_);
    }

}  // -----  end of method XLS_Sheet::XLS_Sheet  (destructor)  ----- 

XLS_Sheet& XLS_Sheet::operator = (const XLS_Sheet& rhs)
{
    if (&rhs != this)
    {
        if (current_sheet_ != nullptr)
        {
            xlsxioread_sheet_close(current_sheet_);
            current_sheet_ = nullptr;
        }

        xlsxioread_ = rhs.xlsxioread_;
        sheet_name_ = rhs.sheet_name_;

        if (xlsxioread_ != nullptr && ! sheet_name_.empty())
        {
            current_sheet_ = xlsxioread_sheet_open(xlsxioread_, sheet_name_.data(), 0);
        }
    }
    return *this;
}		// -----  end of method XLS_Sheet::operator =  ----- 

XLS_Sheet& XLS_Sheet::operator = (XLS_Sheet&& rhs)
{
    if (&rhs != this)
    {
        if (current_sheet_ != nullptr)
        {
            xlsxioread_sheet_close(current_sheet_);
            current_sheet_ = nullptr;
        }

        xlsxioread_ = rhs.xlsxioread_;
        sheet_name_ = rhs.sheet_name_;
        current_sheet_ = rhs.current_sheet_;

        rhs.xlsxioread_ = nullptr;
        rhs.sheet_name_ = {};
        rhs.current_sheet_ = nullptr;


    }
    return *this;
}		// -----  end of method XLS_Sheet::operator =  ----- 

XLS_Sheet::row_itor XLS_Sheet::begin ()
{
    if (current_sheet_ != nullptr)
    {
        return row_itor{current_sheet_};
    }
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::row_itor XLS_Sheet::begin () const
{
    if (current_sheet_ != nullptr)
    {
        return row_itor{current_sheet_};
    }
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::row_itor XLS_Sheet::end ()
{
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

XLS_Sheet::row_itor XLS_Sheet::end () const
{
    return {};
}		// -----  end of method XLS_Sheet::begin  ----- 

//--------------------------------------------------------------------------------------
//       Class:  XLS_Sheet::row_itor
//      Method:  XLS_Sheet::row_itor
// Description:  constructor
//--------------------------------------------------------------------------------------
XLS_Sheet::row_itor::row_itor (xlsxioreadersheet sheet_reader)
    : sheet_reader_{sheet_reader}
{
    // we collect all the cells, if any, in a row and
    // put them in a tab delimited string.

    // we might as well use a smart pointer here. it's
    // possible we could run into some kind of error processing
    // the cells.

    // let's build this just once
    auto cell_deleter = [](XLSXIOCHAR* cell) { free(cell); };

    if (sheet_reader_ != nullptr)
    {
        std::string row;

        bool done = false;

        while (! done && xlsxioread_sheet_next_row(sheet_reader_))
        {
            while (true)
            {
                std::unique_ptr<XLSXIOCHAR, std::function<void(XLSXIOCHAR*)>> next_cell =
                    {xlsxioread_sheet_next_cell(sheet_reader_), cell_deleter};
                if (! next_cell)
                {
                    done = true;
                    break;
                }
                row += next_cell.get();
                row += '\t';
            }
        }
        current_row_ = row;
        current_row_ += '\n';
    }
}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor::row_itor (const row_itor& rhs)
    : sheet_reader_{rhs.sheet_reader_}, current_row_{rhs.current_row_}
{

}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor::row_itor (row_itor&& rhs) noexcept
    : sheet_reader_{rhs.sheet_reader_}, current_row_{rhs.current_row_}
{
    rhs.sheet_reader_ = nullptr;
    rhs.current_row_ = {};

}  // -----  end of method XLS_Sheet::row_itor::row_itor  (constructor)  ----- 

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator=(const row_itor& rhs) 
{
    if (&rhs != this)
    {
        sheet_reader_ = rhs.sheet_reader_;
        current_row_ = rhs.current_row_;
    }
    return *this;
}

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator=(row_itor&& rhs) noexcept
{
    if (&rhs != this)
    {
        sheet_reader_ = rhs.sheet_reader_;
        current_row_ = rhs.current_row_;

        rhs.sheet_reader_ = nullptr;
        rhs.current_row_ = {};
    }
    return *this;
}

XLS_Sheet::row_itor& XLS_Sheet::row_itor::operator++ ()
{
    // let's build this just once

    auto cell_deleter = [](XLSXIOCHAR* cell) { free(cell); };

    if (sheet_reader_ != nullptr)
    {
        std::string row;

        bool done = false;

        while (! done && xlsxioread_sheet_next_row(sheet_reader_))
        {
            while (true)
            {
                std::unique_ptr<XLSXIOCHAR, std::function<void(XLSXIOCHAR*)>> next_cell =
                    {xlsxioread_sheet_next_cell(sheet_reader_), cell_deleter};
                if (! next_cell)
                {
                    done = true;
                    break;
                }
                row += next_cell.get();
                row += '\t';
            }
        }
        if (done)
        {
            current_row_ = row;
            current_row_ += '\n';
        }
        else
        {
            sheet_reader_ = nullptr;
            current_row_.clear();
        }
    }
    return *this;
}		// -----  end of method XLS_Sheet::row_itor::operator++  ----- 

//}		// -----  end of function CleanLabel  -----
