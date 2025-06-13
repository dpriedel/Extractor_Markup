/*
 * =====================================================================================
 *
 *       Filename:  Extractor_Utils.cpp
 *
 *    Description:  Routines shared by XBRL and HTML extracts.
 *
 *        Version:  1.0
 *        Created:  11/14/2018 11:14:03 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  David P. Riedel (), driedel@cox.net
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

#include "Extractor_Utils.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stacktrace>

#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/trim.hpp>

namespace rng = ranges;

#include <pqxx/pqxx>

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

using namespace std::string_literals;

#include "Extractor.h"

date::year_month_day StringToDateYMD(const std::string &input_format,
                                     const std::string &the_date) {
  std::istringstream in{the_date};
  date::sys_days tp;
  date::from_stream(in, input_format.data(), tp);
  BOOST_ASSERT_MSG(!in.fail() && !in.bad(),
                   catenate("Unable to parse given date: ", the_date).c_str());
  date::year_month_day result = tp;
  BOOST_ASSERT_MSG(result.ok(), catenate("Invalid date: ", the_date).c_str());
  return result;
} // -----  end of method tringToDateYMD  -----

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractorException
 *      Method:  ExtractorException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractorException::ExtractorException(const char *text)
    : std::runtime_error(text) {
} /* -----  end of method ExtractorException::ExtractorException  (constructor)
     ----- */

ExtractorException::ExtractorException(const std::string &text)
    : std::runtime_error(text) {
} /* -----  end of method ExtractorException::ExtractorException  (constructor)
     ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  AssertionException
 *      Method:  AssertionException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
AssertionException::AssertionException(const char *text)
    : std::invalid_argument(text) {
} /* -----  end of method AssertionException::AssertionException  (constructor)
     ----- */

AssertionException::AssertionException(const std::string &text)
    : std::invalid_argument(text) {
} /* -----  end of method AssertionException::AssertionException  (constructor)
     ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  XBRLException
 *      Method:  XBRLException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
XBRLException::XBRLException(const char *text)
    : ExtractorException(text) {
} /* -----  end of method XBRLException::XBRLException  (constructor)  ----- */

XBRLException::XBRLException(const std::string &text)
    : ExtractorException(text) {
} /* -----  end of method XBRLException::XBRLException  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  HTMLException
 *      Method:  HTMLException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
HTMLException::HTMLException(const char *text)
    : ExtractorException(text) {
} /* -----  end of method HTMLException::HTMLException  (constructor)  ----- */

HTMLException::HTMLException(const std::string &text)
    : ExtractorException(text) {
} /* -----  end of method HTMLException::HTMLException  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  MaxFilesException
 *      Method:  MaxFilesException
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
MaxFilesException::MaxFilesException(const char *text)
    : std::range_error(text) {
} /* -----  end of method MaxFilesException::MaxFilesException  (constructor)
     ----- */

MaxFilesException::MaxFilesException(const std::string &text)
    : std::range_error(text) {
} /* -----  end of method MaxFilesException::MaxFilesException  (constructor)
     ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * LoadDataFileForUse Description:
 * =====================================================================================
 */
std::string LoadDataFileForUse(const EM::FileName &file_name) {
  std::string file_content(fs::file_size(file_name.get()), '\0');
  std::ifstream input_file{std::string{file_name.get()},
                           std::ios_base::in | std::ios_base::binary};
  input_file.read(&file_content[0], file_content.size());
  input_file.close();

  return file_content;
} /* -----  end of function LoadDataFileForUse  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * LocateDocumentSections Description:
 * =====================================================================================
 */
EM::DocumentSectionList LocateDocumentSections(EM::FileContent file_content) {
  const std::string doc_begin{"<DOCUMENT>"};
  const std::string doc_end{"</DOCUMENT>"};
  const auto doc_begin_len = doc_begin.size();
  const auto doc_end_len = doc_end.size();

  EM::DocumentSectionList result;

  auto found_begin = file_content.get().begin();
  auto content_end = file_content.get().end();

  auto doc_searcher = std::boyer_moore_searcher(doc_end.begin(), doc_end.end());

  while (found_begin != content_end) {
    // look for our begin tag

    found_begin = std::search(found_begin, content_end, doc_begin.begin(),
                              doc_begin.end());

    if (found_begin == content_end) {
      break;
    }

    // now, look for our end tag.
    // since this can be rather far away, try boyer-moore

    auto found_end =
        std::search(found_begin + doc_begin_len, content_end, doc_searcher);

    if (found_end == content_end) {
      throw ExtractorException("Can't find end of 'DOCUMENT'");
    }

    // but we need to check for nested tags which should not happen.

    //        if (*(found_end - 1) != '/' or *(found_end - 2) != '<')
    //        {
    //            throw ExtractorException("Looks like embedded 'DOCUMENT'
    //            sections.");
    //        }

    result.emplace_back(EM::DocumentSection{
        EM::sv(found_begin, found_end + doc_end_len - found_begin)});
    found_begin = found_end + doc_end_len;
  }
  return result;
} /* -----  end of function LocateDocumentSections  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * FindFileName Description:
 * =====================================================================================
 */
EM::FileName FindFileName(const EM::DocumentSection &document,
                          const EM::FileName &document_name) {
  const boost::regex regex_fname{R"***(^<FILENAME>(.*?)$)***"};
  boost::cmatch matches;

  if (bool found_it = boost::regex_search(
          document.get().cbegin(), document.get().cend(), matches, regex_fname);
      found_it) {
    EM::FileName file_name{std::string(matches[1].first, matches[1].length())};
    return file_name;
  }
  throw ExtractorException(
      catenate("Can't find file name in document: ", document_name.get()));
} /* -----  end of function FindFileName  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * FindFileType Description:
 * =====================================================================================
 */
EM::FileType FindFileType(const EM::DocumentSection &document) {
  const boost::regex regex_ftype{R"***(^<TYPE>(.*?)$)***"};
  boost::cmatch matches;

  if (bool found_it = boost::regex_search(
          document.get().cbegin(), document.get().cend(), matches, regex_ftype);
      found_it) {
    EM::FileType file_type{EM::sv(matches[1].first, matches[1].length())};
    return file_type;
  }
  throw ExtractorException("Can't find file type in document.\n");
} /* -----  end of function FindFileType  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * FindHTML Description:
 * =====================================================================================
 */

EM::HTMLContent FindHTML(const EM::DocumentSection &document,
                         const EM::FileName &document_name) {
  auto file_name = FindFileName(document, document_name);
  if (file_name.get().extension() == ".htm") {
    // now, we just need to drop the extraneous XML surrounding the data we
    // need.

    auto result{document.get()};
    auto text_begin_loc = result.find(R"***(<TEXT>)***");

    // skip 1 more line.

    text_begin_loc = result.find('\n', text_begin_loc + 1);

    result.remove_prefix(text_begin_loc);

    auto text_end_loc = result.rfind(R"***(</TEXT>)***");
    if (text_end_loc != EM::sv::npos) {
      result.remove_suffix(result.length() - text_end_loc);
    } else {
      throw HTMLException("Can't find end of HTML in document.\n");
    }

    // sometimes the document is actually XBRL with embedded HTML
    // we don't want that.

    if (result.find(R"***(<XBRL>)***") != EM::sv::npos) {
      //            // let's see if it contains HTML
      //
      //            const boost::regex regex_meta{R"***(<meta.*?</meta>)***"};
      //            boost::cmatch matches;
      //
      //            if (boost::regex_search(result.begin(), result.end(),
      //            matches, regex_meta))
      //            {
      //                EM::sv meta(result.data() + matches.position(),
      //                matches.length()); if (meta.find("text/html"))
      //                {
      //                    return result;
      //                }
      //            }
      spdlog::info("Looks like it's really XBRL.\n");
      return EM::HTMLContent{};
    }
    return EM::HTMLContent{result};
  }
  return EM::HTMLContent{};
} /* -----  end of function FindHTML  ----- */

bool FormIsInFileName(const std::vector<std::string> &form_types,
                      const EM::FileName &file_name) {
  auto check_for_form_in_name([&file_name](auto &form_type) {
    auto pos = file_name.get().string().find(catenate('/', form_type, '/'));
    return (pos != std::string::npos);
  });
  return std::any_of(std::begin(form_types), std::end(form_types),
                     check_for_form_in_name);
} /* -----  end of function FormIsInFileName  ----- */

bool FileHasXBRL::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  // need to do a little more detailed check.

  EM::FileName document_name(SEC_fields.at("file_name"));
  for (const auto &document : document_sections) {
    auto file_name = FindFileName(document, document_name);
    auto file_type = FindFileType(document);
    if (file_type.get().ends_with(".INS") &&
        file_name.get().extension() == ".xml") {
      return true;
    }
  }
  return false;
} /* -----  end of method FileHasXBRL::operator()  ----- */

bool FileHasXLS::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  // need to do a little more detailed check.

  EM::FileName document_name(SEC_fields.at("file_name"));
  for (const auto &document : document_sections) {
    auto file_name = FindFileName(document, document_name);
    if (file_name.get().extension() == ".xlsx") {
      return true;
    }
  }
  return false;
} /* -----  end of method FileHasXBRL::operator()  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * FileHasHTML::operator() Description:
 * =====================================================================================
 */
bool FileHasHTML::operator()(
    const EM::SEC_Header_fields &header_fields,
    const EM::DocumentSectionList &document_sections) const {
  // need to do a little more detailed check.

  EM::FileName document_name(header_fields.at("file_name"));
  for (const auto &document : document_sections) {
    auto file_name = FindFileName(document, document_name);
    auto file_type = FindFileType(document);

    if (file_name.get().extension() == ".htm" &&
        rng::find(form_list_, file_type.get()) != rng::end(form_list_)) {
      auto content = FindHTML(document, file_name);
      if (!content.get().empty()) {
        return true;
      }
    }
  }
  return false;
} /* -----  end of function FileHasHTML::operator()  ----- */

bool FileHasFormType::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  return (rng::find(form_list_, SEC_fields.at("form_type")) !=
          rng::end(form_list_));
} /* -----  end of method FileHasFormType::operator()  ----- */

bool FileHasCIK::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  // if our list has only 2 elements, the consider this a range.  otherwise,
  // just a list.

  if (CIK_list_.size() == 2) {
    return (CIK_list_[0] <= SEC_fields.at("cik") &&
            SEC_fields.at("cik") <= CIK_list_[1]);
  }

  return (rng::find(CIK_list_, SEC_fields.at("cik")) != rng::end(CIK_list_));
} /* -----  end of method FileHasCIK::operator()  ----- */

bool FileHasSIC::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  return (rng::find(SIC_list_, SEC_fields.at("sic")) != rng::end(SIC_list_));
} /* -----  end of method FileHasSIC::operator()  ----- */

bool NeedToUpdateDBContent::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  // we need to work with just the base form name

  auto form_type = SEC_fields.at("form_type");
  EM::sv base_form_type{form_type};
  if (base_form_type.ends_with("_A")) {
    base_form_type.remove_suffix(2);
  }

  pqxx::connection c{"dbname=sec_extracts user=extractor_pg"};
  pqxx::work trxn{c};

  std::string check_for_existing_content_cmd;
  if (mode_ == "BOTH") {
    check_for_existing_content_cmd = fmt::format(
        "SELECT count(*) AS how_many FROM {3}unified_extracts.sec_filing_id "
        "WHERE"
        " cik = {0} AND form_type = {1} AND period_ending = {2}",
        trxn.quote(SEC_fields.at("cik")), trxn.quote(base_form_type),
        trxn.quote(SEC_fields.at("quarter_ending")), schema_prefix_);
  } else {
    check_for_existing_content_cmd = fmt::format(
        "SELECT count(*) AS how_many FROM {3}unified_extracts.sec_filing_id "
        "WHERE"
        " cik = {0} AND form_type = {1} AND period_ending = {2} AND "
        "data_source = {4}",
        trxn.quote(SEC_fields.at("cik")), trxn.quote(base_form_type),
        trxn.quote(SEC_fields.at("quarter_ending")), schema_prefix_,
        trxn.quote(mode_));
  }

  auto have_data = trxn.query_value<int>(check_for_existing_content_cmd);
  trxn.commit();

  if (have_data != 0 && !replace_DB_content_ && !form_type.ends_with("_A")) {
    // simple case here

    spdlog::info(catenate(
        "Skipping: Form data exists and Replace not specifed for file: ",
        SEC_fields.at("file_name")));
    return false;
  }

  // at this point, we need to check whether we have an amended return
  // and if so, is there data from a previous amended return.
  // we do that by checking for an amended_date_filed value in the DB
  // then, is our current amended_date_filed newer.

  if (have_data != 0 && !replace_DB_content_ && form_type.ends_with("_A")) {
    check_for_existing_content_cmd = fmt::format(
        "SELECT amended_date_filed FROM {3}unified_extracts.sec_filing_id WHERE"
        " cik = {0} AND form_type = {1} AND period_ending = {2}",
        trxn.quote(SEC_fields.at("cik")), trxn.quote(base_form_type),
        trxn.quote(SEC_fields.at("quarter_ending")), schema_prefix_);

    pqxx::work trxn{c};
    auto row = trxn.exec(check_for_existing_content_cmd).one_row();
    std::string amended_date;
    if (!row["amended_date_filed"].is_null()) {
      amended_date = row["amended_date_filed"].view();
    }
    trxn.commit();
    if (amended_date.empty()) {
      // no previously stored ameended data so
      // we need to use this.
      return true;
    }

    // lastly, let's see if this data is more recent.

    auto stored_amended_date = StringToDateYMD("%F", amended_date);
    auto date_filed = StringToDateYMD("%F", SEC_fields.at("date_filed"));
    if (date_filed <= stored_amended_date) {
      return false;
    }
  }

  return true;
} /* -----  end of method NeedToUpdateDBContent::operator()  ----- */

bool FileIsWithinDateRange::operator()(
    const EM::SEC_Header_fields &SEC_fields,
    const EM::DocumentSectionList &document_sections) const {
  auto report_date = StringToDateYMD("%F", SEC_fields.at("quarter_ending"));
  return (begin_date_ <= report_date && report_date <= end_date_);
} /* -----  end of method FileIsWithinDateRange::operator()  ----- */

fs::path ConvertInputHierarchyToOutputHierarchy::operator()(
    const EM::FileName &source_file_path,
    const std::string &destination_file_name) const {
  // want keep the directory hierarchy the same as on the source directory
  // EXCEPT for the source directory prefix (which is given to our ctor)

  // we will assume there is not trailing delimiter on the stored remote prefix.
  // (even though we have no edit to enforce that for now.)

  std::string source_index_name = boost::algorithm::replace_first_copy(
      source_file_path.get().string(), source_prefix_.string(), "");

  //  there seems to be a change in behaviour.  appending a path starting with
  //  '/' actually does an assignment, not an append.

  if (source_index_name[0] == '/') {
    source_index_name.erase(0, 1);
  }
  auto destination_path_name = destination_prefix_;
  destination_path_name /= source_index_name;
  //    destination_path_name.remove_filename();
  destination_path_name += ('_' + destination_file_name);
  return destination_path_name;

} // -----  end of method DailyIndexFileRetriever::MakeLocalIndexFileName  -----

// ===  FUNCTION
// ======================================================================
//         Name:  CleanLabel
//  Description:
// =====================================================================================

std::string CleanLabel(const std::string &label) {
  static const std::string delete_this{""};
  static const std::string single_space{" "};
  static const boost::regex regex_punctuation{R"***([[:punct:]])***"};
  static const boost::regex regex_leading_space{R"***(^[[:space:]]+)***"};
  static const boost::regex regex_trailing_space{R"***([[:space:]]{1,}$)***"};
  static const boost::regex regex_double_space{R"***([[:space:]]{2,})***"};

  std::string cleaned_label =
      boost::regex_replace(label, regex_punctuation, single_space);
  cleaned_label =
      boost::regex_replace(cleaned_label, regex_leading_space, delete_this);
  cleaned_label =
      boost::regex_replace(cleaned_label, regex_trailing_space, delete_this);
  cleaned_label =
      boost::regex_replace(cleaned_label, regex_double_space, single_space);

  // lastly, lowercase

  rng::for_each(cleaned_label, [](char &c) { c = std::tolower(c); });

  return cleaned_label;
} // -----  end of function CleanLabel  -----

namespace boost {
// these functions are declared in the library headers but left to the user to
// define. so here they are...
//
/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * assertion_failed_mgs Description: defined in boost header but left to us to
 * implement.
 * =====================================================================================
 */

void assertion_failed_msg(char const *expr, char const *msg,
                          char const *function, char const *file, long line) {
  std::cout << std::stacktrace::current() << std::endl;
  throw AssertionException(catenate(
      "\n*** Assertion failed *** test: ", expr, " in function: ", function,
      " from file: ", file, " at line: ", line, ".\nassertion msg: ", msg));
} /* -----  end of function assertion_failed_mgs  ----- */

/*
 * ===  FUNCTION
 * ====================================================================== Name:
 * assertion_failed Description:
 * =====================================================================================
 */
void assertion_failed(char const *expr, char const *function, char const *file,
                      long line) {
  std::cout << std::stacktrace::current() << std::endl;
  throw AssertionException(catenate("\n*** Assertion failed *** test: ", expr,
                                    " in function: ", function,
                                    " from file: ", file, " at line: ", line));
} /* -----  end of function assertion_failed  ----- */
} /* end namespace boost */
