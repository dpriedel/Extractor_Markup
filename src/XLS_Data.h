// =====================================================================================
//
//       Filename:  XLS_Data.h
//
//    Description: range compatible access to XBRL XLS data 
//
//        Version:  1.0
//        Created:  04/18/2020 09:26:00 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (), driedel@cox.net
//        License:  GNU General Public License -v3
//
// =====================================================================================

#ifndef  XLS_DATA_INC
#define  XLS_DATA_INC

#include <functional>
#include <iterator>
#include <memory>
#include <vector>

#include <xlsxio_read.h>

#include "Extractor.h"

class XLS_Sheet;
class XLS_Row;

// =====================================================================================
//        Class:  XLS_File
//  Description: manage access to XLS data. 
// =====================================================================================

class XLS_File
{
public:

    class sheet_itor;

    using iterator = sheet_itor;
    using const_iterator = sheet_itor;

public:
    // ====================  LIFECYCLE     ======================================= 

    XLS_File () = default;                             // constructor 
    XLS_File(const XLS_File& rhs) = delete;
    XLS_File(XLS_File&& rhs) noexcept;

    XLS_File(std::vector<char>&& content);

    ~XLS_File();

    // ====================  ACCESSORS     ======================================= 

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const { return content_.empty(); }

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    XLS_File& operator =(const XLS_File& rhs) = delete;
    XLS_File& operator =(XLS_File&& rhs);


protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:

//    friend class sheet_itor;

    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    std::vector<char> content_;

    xlsxioreader xlsxioread_ = nullptr;

}; // -----  end of class XLS_File  ----- 


// =====================================================================================
//        Class:  XLS_Sheet
//  Description: manage access to individual worksheet 
// =====================================================================================
class XLS_Sheet
{
public:

    class row_itor;

    using iterator = row_itor;
    using const_iterator = row_itor;

public:
    // ====================  LIFECYCLE     ======================================= 

    XLS_Sheet () = default;                             // constructor 
    XLS_Sheet(xlsxioreader xlsxioread, const char* sheet_name);
    
    XLS_Sheet(const XLS_Sheet& rhs);
    XLS_Sheet(XLS_Sheet&& rhs);

    ~XLS_Sheet();

    // ====================  ACCESSORS     ======================================= 

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const { return sheet_name_.empty(); }

    const EM::sv GetSheetName() const { return sheet_name_ ; }

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    XLS_Sheet& operator = (const XLS_Sheet& rhs);
    XLS_Sheet& operator = (XLS_Sheet&& rhs);

    bool operator==(const XLS_Sheet& rhs) const
        { 
            return xlsxioread_ == rhs.xlsxioread_
                && sheet_name_ == rhs.sheet_name_;
        }
    bool operator!=(const XLS_Sheet& rhs) const { return !(*this == rhs); }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    xlsxioreader xlsxioread_ = nullptr;
    xlsxioreadersheet  current_sheet_ = nullptr;
    EM::sv sheet_name_;

}; // -----  end of class XLS_Sheet  ----- 


// =====================================================================================
//        Class:  XLS_File::sheet_itor
//  Description: access to worksheets in XLS data. 
// =====================================================================================

class XLS_File::sheet_itor
{
public:

    using iterator_category = std::forward_iterator_tag;
    using value_type = XLS_Sheet;
    using difference_type = ptrdiff_t;
    using pointer = XLS_Sheet *;
    using reference = XLS_Sheet &;

public:
    // ====================  LIFECYCLE     ======================================= 

    sheet_itor () = default;                             // constructor 
    sheet_itor (xlsxioreader xlsxioread);

    sheet_itor (const sheet_itor& rhs);
    sheet_itor (sheet_itor&& rhs);

    ~sheet_itor ();

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    sheet_itor& operator++();
    sheet_itor operator++(int) { sheet_itor retval = *this; ++(*this); return retval; }

    // ====================  OPERATORS     ======================================= 

    sheet_itor& operator = (const sheet_itor& rhs);
    sheet_itor& operator = (sheet_itor&& rhs);

    bool operator==(const sheet_itor& rhs) const
        { 
            return sheet_list_ == rhs.sheet_list_
                && sheet_name_ == rhs.sheet_name_;
        }
    bool operator!=(const sheet_itor& rhs) const { return !(*this == rhs); }

    reference operator*() const { return current_sheet_; }
    pointer operator->() const { return &current_sheet_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 


    xlsxioreader xlsxioread_ = nullptr;
    xlsxioreadersheetlist sheet_list_ = nullptr;
    const char* sheet_name_ = nullptr;

    mutable XLS_Sheet current_sheet_;

}; // -----  end of class XLS_File::sheet_itor  ----- 


// =====================================================================================
//        Class:  XLS_Sheet::row_itor
//  Description:  extracts value pairs from the XLS sheet data 
// =====================================================================================
class XLS_Sheet::row_itor
{
public:

    using iterator_category = std::input_iterator_tag;
    using value_type = std::string;
    using difference_type = ptrdiff_t;
    using pointer = std::string *;
    using reference = std::string &;

public:
    // ====================  LIFECYCLE     ======================================= 
    row_itor () = default;                             // constructor 
    row_itor (xlsxioreadersheet current_sheet);       // constructor 

    row_itor(const row_itor& rhs) = delete;
    row_itor(row_itor&& rhs) noexcept;

    ~row_itor() = default;

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    row_itor& operator++();
//    row_itor operator++(int) { row_itor retval = *this; ++(*this); return retval; }

    // ====================  OPERATORS     ======================================= 

    row_itor& operator = (const row_itor& rhs) = delete;
    row_itor& operator = (row_itor&& rhs) noexcept ;

    bool operator==(const row_itor& rhs) const
        { 
            return current_sheet_ == rhs.current_sheet_
                && current_row_ == rhs.current_row_;
        }
    bool operator!=(const row_itor& rhs) const { return !(*this == rhs); }

    reference operator*() const { return current_row_; }
    pointer operator->() const { return &current_row_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    xlsxioreadersheet current_sheet_ = nullptr;
    
    mutable std::string current_row_;

}; // -----  end of class XLS_Sheet::row_itor  ----- 


#endif   // ----- #ifndef XLS_DATA_INC  ----- 
