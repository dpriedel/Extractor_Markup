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

#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
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

    using value_type = XLS_Sheet;

    // we need to look like a vector so we'll use its types.

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = sheet_itor;
    using const_iterator = const sheet_itor;

public:
    // ====================  LIFECYCLE     ======================================= 

    XLS_File () = default;                             // constructor 
    XLS_File(const XLS_File& rhs);
    XLS_File(XLS_File&& rhs) noexcept;

    XLS_File(const std::vector<char>& content);
    XLS_File(std::vector<char>&& content);

    ~XLS_File() = default;

    // ====================  ACCESSORS     ======================================= 

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const { return content_.empty(); }

    std::vector<std::string> GetSheetNames(void) const;

    std::optional<XLS_Sheet> FindSheetByName(EM::sv sheet_name) const;
    std::optional<XLS_Sheet> FindSheetByInternalName(EM::sv sheet_name) const ;

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    XLS_File& operator =(const XLS_File& rhs);
    XLS_File& operator =(XLS_File&& rhs) noexcept;


protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:

    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    std::vector<char> content_;


}; // -----  end of class XLS_File  ----- 


// =====================================================================================
//        Class:  XLS_Sheet
//  Description: manage access to individual worksheet 
// =====================================================================================
class XLS_Sheet
{
public:

    class row_itor;

    using value_type = std::string;

    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = value_type &;
    using pointer = value_type *;
    using const_pointer = value_type const *;
    using iterator = row_itor;
    using const_iterator = const row_itor;

public:
    // ====================  LIFECYCLE     ======================================= 

    XLS_Sheet () = default;                             // constructor 
    XLS_Sheet(const std::vector<char>* content, const XLSXIOCHAR* sheet_name);
    
    XLS_Sheet(const XLS_Sheet& rhs);
    XLS_Sheet(XLS_Sheet&& rhs) noexcept;

    ~XLS_Sheet() = default;

    // ====================  ACCESSORS     ======================================= 

    [[nodiscard]] iterator begin();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator end() const;

    [[nodiscard]] bool empty() const { return sheet_name_lc_.empty(); }

    const std::string& GetSheetName() const { return sheet_name_lc_ ; }
    const std::string& GetSheetNameFromInside() const ;

    // ====================  MUTATORS      ======================================= 

    // ====================  OPERATORS     ======================================= 

    XLS_Sheet& operator = (const XLS_Sheet& rhs);
    XLS_Sheet& operator = (XLS_Sheet&& rhs);

    bool operator==(const XLS_Sheet& rhs) const
        { 
            return content_ == rhs.content_
                && sheet_name_mc_ == rhs.sheet_name_mc_;
        }
    bool operator!=(const XLS_Sheet& rhs) const { return !(*this == rhs); }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    const std::vector<char>* content_ = nullptr;
//    xlsxioreader xlsxioread_ = nullptr;
//    xlsxioreadersheet  current_sheet_ = nullptr;

    mutable std::string extended_sheet_name_;
    mutable std::string sheet_name_lc_;
    mutable std::string sheet_name_mc_;

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
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = value_type &;
    using pointer = value_type *;
    using const_pointer = value_type const *;

public:
    // ====================  LIFECYCLE     ======================================= 

    sheet_itor () = default;                             // constructor 
    sheet_itor (const std::vector<char>* content);

    sheet_itor (const sheet_itor& rhs);
    sheet_itor (sheet_itor&& rhs) noexcept;

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
            return sheet_list_ == rhs.sheet_list_ && sheet_name_ == rhs.sheet_name_;
        }
    bool operator!=(const sheet_itor& rhs) const { return !(*this == rhs); }

    reference operator*() { return current_sheet_; }
    const_reference operator*() const { return current_sheet_; }
    pointer operator->() { return &current_sheet_; }
    const_pointer operator->() const { return &current_sheet_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    const std::vector<char>* content_ = nullptr;
    const XLSXIOCHAR* sheet_name_ = nullptr;

    xlsxioreader xlsxioread_ = nullptr;
    xlsxioreadersheetlist sheet_list_ = nullptr;

    mutable XLS_Sheet current_sheet_;

}; // -----  end of class XLS_File::sheet_itor  ----- 


// =====================================================================================
//        Class:  XLS_Sheet::row_itor
//  Description:  extracts value pairs from the XLS sheet data 
// =====================================================================================
class XLS_Sheet::row_itor
{
public:

    using iterator_category = std::forward_iterator_tag;
    using value_type = std::string;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type &;
    using const_reference = value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;

public:
    // ====================  LIFECYCLE     ======================================= 
    row_itor () = default;                             // constructor 
    row_itor(const std::vector<char>* content, const std::string& sheet_name);

    row_itor(const row_itor& rhs);
    row_itor(row_itor&& rhs) noexcept;

    ~row_itor();

    // ====================  ACCESSORS     ======================================= 

    // ====================  MUTATORS      ======================================= 

    row_itor& operator++();
    row_itor operator++(int) { row_itor retval = *this; ++(*this); return retval; }

    // ====================  OPERATORS     ======================================= 

    row_itor& operator = (const row_itor& rhs);
    row_itor& operator = (row_itor&& rhs) noexcept;

    bool operator==(const row_itor& rhs) const
    { 
        return current_sheet_ == rhs.current_sheet_
            && current_row_ == rhs.current_row_;
    }
    bool operator!=(const row_itor& rhs) const { return !(*this == rhs); }

    reference operator*() { return current_row_; }
    const_reference operator*() const { return current_row_; }
    pointer operator->() { return &current_row_; }
    const_pointer operator->() const { return &current_row_; }

protected:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

private:
    // ====================  METHODS       ======================================= 

    // ====================  DATA MEMBERS  ======================================= 

    const std::vector<char>* content_ = nullptr;
    xlsxioreader xlsxioread_ = nullptr;
    xlsxioreadersheet  current_sheet_ = nullptr;
    
    mutable std::string sheet_name_mc_;
    mutable std::string current_row_;

}; // -----  end of class XLS_Sheet::row_itor  ----- 


#endif   // ----- #ifndef XLS_DATA_INC  ----- 
