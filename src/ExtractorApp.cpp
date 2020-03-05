// =====================================================================================
//
//       Filename:  ExtractorApp.cpp
//
//    Description:  main application
//
//        Version:  1.0
//        Created:  04/23/2018 09:50:10 AM
//       Revision:  none
//       Compiler:  g++
//
//         Author:  David P. Riedel (dpr), driedel@cox.net
//        License:  GNU General Public License v3
//        Company:
//
// =====================================================================================

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


//--------------------------------------------------------------------------------------
//       Class:  ExtractorApp
//      Method:  ExtractorApp
// Description:  constructor
//--------------------------------------------------------------------------------------

#include "ExtractorApp.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <range/v3/action/transform.hpp>
#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/for_each.hpp>

#include "date/tz.h"

#include "spdlog/sinks/basic_file_sink.h"

#include <pqxx/pqxx>

#include "Extractor_HTML_FileFilter.h"
#include "Extractor_XBRL_FileFilter.h"
#include "SEC_Header.h"

using namespace std::string_literals;

// This ctype facet does NOT classify spaces and tabs as whitespace
// from cppreference example

//struct line_only_whitespace : std::ctype<char>
//{
//    static const mask* make_table()
//    {
//        // make a copy of the "C" locale table
//        static std::vector<mask> v(classic_table(), classic_table() + table_size);
//        v['\t'] &= ~space;      // tab will not be classified as whitespace
//        v[' '] &= ~space;       // space will not be classified as whitespace
//        return &v[0];
//    }
//    explicit line_only_whitespace(std::size_t refs = 0) : ctype(make_table(), false, refs) {}
//};

bool ExtractorApp::had_signal_ = false;

// code from "The C++ Programming Language" 4th Edition. p. 1243.
// with modifications.
//
// return index of ready future
// if no future is ready, wait for d before trying again

template<typename T>
int wait_for_any(std::vector<std::future<T>>& vf, int continue_here, std::chrono::steady_clock::duration d)
{
    while(true)
    {
        for (int i=continue_here; i!=vf.size(); ++i)
        {
            if (!vf[i].valid())
            {
                continue;
            }
            switch (vf[i].wait_for(std::chrono::seconds{0}))
            {
            case std::future_status::ready:
                    return i;

            case std::future_status::timeout:
                break;

            case std::future_status::deferred:
                throw std::runtime_error("wait_for_all(): deferred future");
            }
        }
        continue_here = 0;

        if (ExtractorApp::SignalReceived())
        {
            break;
        }

        std::this_thread::sleep_for(d);
    }

    return -1;
}

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractorApp
 *      Method:  ExtractorApp
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractorApp::ExtractorApp (int argc, char* argv[])
    : mArgc{argc}, mArgv{argv}
{
}  /* -----  end of method ExtractorApp::ExtractorApp  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractorApp
 *      Method:  ExtractorApp
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractorApp::ExtractorApp (const std::vector<std::string>& tokens)
    : tokens_{tokens}
{
}  /* -----  end of method ExtractorApp::ExtractorApp  (constructor)  ----- */

void ExtractorApp::ConfigureLogging()
{
    // we need to set log level if specified and also log file.

    if (! log_file_path_name_.get().empty())
    {
        // if we are running inside our test harness, logging may already by
        // running so we don't want to clobber it.
        // different tests may use different names.

        auto logger_name = log_file_path_name_.get().filename();
        logger_ = spdlog::get(logger_name);
        if (! logger_)
        {
            fs::path log_dir = log_file_path_name_.get().parent_path();
            if (! fs::exists(log_dir))
            {
                fs::create_directories(log_dir);
            }

            logger_ = spdlog::basic_logger_mt(logger_name, log_file_path_name_.get().c_str());
            spdlog::set_default_logger(logger_);
        }
    }

    // we are running before 'CheckArgs' so we need to do a little editiing ourselves.

    std::map<std::string, spdlog::level::level_enum> levels
    {
        {"none", spdlog::level::off},
        {"error", spdlog::level::err},
        {"information", spdlog::level::info},
        {"debug", spdlog::level::debug}
    };

    auto which_level = levels.find(logging_level_);
    if (which_level != levels.end())
    {
        spdlog::set_level(which_level->second);
    }

}

bool ExtractorApp::Startup()
{
    spdlog::info(catenate("\n\n*** Begin run ", LocalDateTimeAsString(std::chrono::system_clock::now()), " ***\n"));
    bool result{true};
	try
	{	
		SetupProgramOptions();
        if (tokens_.empty())
        {
            ParseProgramOptions();
        }
        else
        {
            ParseProgramOptions(tokens_);
        }
        ConfigureLogging();
		result = CheckArgs ();
	}
	catch(std::exception& e)
	{
        spdlog::error(catenate("Problem in startup: ", e.what(), '\n'));
		//	we're outta here!

		this->Shutdown();
        result = false;
    }
	catch(...)
	{
        spdlog::error("Unexpectd problem during Starup processing\n");

		//	we're outta here!

		this->Shutdown();
        result = false;
	}
    return result;
}		/* -----  end of method ExtractorApp::Startup  ----- */

void ExtractorApp::SetupProgramOptions ()
{
    mNewOptions = std::make_unique<po::options_description>();

	mNewOptions->add_options()
		("help,h", "produce help message")
		("begin-date", po::value<std::string>(&this->start_date_), "retrieve files with dates greater than or equal to")
		("end-date", po::value<std::string>(&this->stop_date_), "retrieve files with dates less than or equal to")
		("form-dir", po::value<EM::FileName>(&local_form_file_directory_),	"directory of form files to be processed")
//		("SS-form-dir", po::value<EM::FileName>(&SS_export_directory_),	"directory to write Excel data files to.")
		("HTML-forms-to-dir", po::value<EM::FileName>(&HTML_export_target_directory_),	"directory to write exported HTML data files to.")
		("HTML-forms-from-dir", po::value<EM::FileName>(&HTML_export_source_directory_),	"directory to read exported HTML data files from.")
		("file,f", po::value<EM::FileName>(&single_file_to_process_),	"single file to be processed.")
		("replace-DB-content,R", po::value<bool>(&replace_DB_content_)->default_value(false)->implicit_value(true),
            "replace all DB content for each file. Default is 'false'")
		("export-XLS-data", po::value<bool>(&export_XLS_files_)->default_value(false)->implicit_value(true),
            "export Excel data if any. Default is 'false'")
		("export-HTML-data", po::value<bool>(&export_HTML_forms_)->default_value(false)->implicit_value(true),
            "export problem HTML data if any. Default is 'false'")
		("UpdateSharesOutstanding", po::value<bool>(&update_shares_outstanding_)->default_value(false)->implicit_value(true),
            "Update Shares outstanding value in DB.")
		("list-file", po::value<EM::FileName>(&list_of_files_to_process_path_),"path to file with list of files to process.")
		("log-level,l", po::value<std::string>(&logging_level_),
         "logging level. Must be 'none|error|information|debug'. Default is 'information'.")
		("mode,m", po::value<std::string>(&data_source_)->required(), "Must be either 'BOTH' or 'HTML' or 'XBRL'.")
		("DB-mode", po::value<std::string>(&DB_mode_), "Must be either 'test' or 'live'. Default is 'test'.")
		("form", po::value<std::string>(&form_)->default_value("10-Q"),
         "name of form type we are processing. May be comma-delimited list. Default is '10-Q'.")
		("CIK",	po::value<std::string>(&CIK_),
         "Zero-padded-on-left 10 digit CIK[s] we are processing. May be comma-delimited list. Default is all.")
		("SIC",	po::value<std::string>(&SIC_),
         "SIC we are processing. May be comma-delimited list. Default is all.")
		("log-path", po::value<EM::FileName>(&log_file_path_name_),	"path name for log file.")
		("max-files", po::value<int>(&max_forms_to_process_)->default_value(-1),
         "Maximun number of forms to process -- mainly for testing. Default of -1 means no limit.")
		("concurrent,k", po::value<int>(&max_at_a_time_)->default_value(-1),
         "Maximun number of concurrent processes. Default of -1 -- system defined.")
		("filename-has-form", po::value<bool>(&filename_has_form_)->default_value(false)->implicit_value(true),
            "form number is in file path. Default is 'false'")
		("resume-at", po::value<std::string>(&resume_at_this_filename_),
         "find this file name in list of files to process and resume processing there.")
		;
}		/* -----  end of method ExtractorApp::SetupProgramOptions  ----- */

void ExtractorApp::ParseProgramOptions ()
{
	auto options = po::parse_command_line(mArgc, mArgv, *mNewOptions);
	po::store(options, mVariableMap);
	if (this->mArgc == 1 ||	mVariableMap.count("help") == 1)
	{
		std::cout << *mNewOptions << "\n";
		throw std::runtime_error("\nExiting after 'help'.");
	}
	po::notify(mVariableMap);    

}		/* -----  end of method ExtractorApp::ParseProgramOptions  ----- */

void ExtractorApp::ParseProgramOptions (const std::vector<std::string>& tokens)
{
	auto options = po::command_line_parser(tokens).options(*mNewOptions).run();
	po::store(options, mVariableMap);
	if (mVariableMap.count("help") == 1)
	{
		std::cout << *mNewOptions << "\n";
		throw std::runtime_error("\nExiting after 'help'.");
	}
	po::notify(mVariableMap);    
}		/* -----  end of method ExtractorApp::ParseProgramOptions  ----- */

bool ExtractorApp::CheckArgs ()
{
    if (! start_date_.empty())
    {
        std::istringstream in{start_date_};
        date::sys_days tp;
        in >> date::parse("%F", tp);
        if (in.fail())
        {
            // try an alternate representation

            in.clear();
            in.rdbuf()->pubseekpos(0);
            in >> date::parse("%Y-%b-%d", tp);
        }
        BOOST_ASSERT_MSG(! in.fail() && ! in.bad(), catenate("Unable to parse begin date: ", start_date_).c_str());
        begin_date_ = tp;
        BOOST_ASSERT_MSG(begin_date_.ok(), catenate("Invalid begin date: ", start_date_).c_str());
    }
    if (! stop_date_.empty())
    {
        std::istringstream in{stop_date_};
        date::sys_days tp;
        in >> date::parse("%F", tp);
        if (in.fail())
        {
            // try an alternate representation

            in.clear();
            in.rdbuf()->pubseekpos(0);
            in >> date::parse("%Y-%b-%d", tp);
        }
        BOOST_ASSERT_MSG(! in.fail() && ! in.bad(), catenate("Unable to parse end date: ", end_date_).c_str());
        end_date_ = tp;
        BOOST_ASSERT_MSG(end_date_.ok(), catenate("Invalid end date: ", stop_date_).c_str());
    }

    if (! start_date_.empty())
    {
        if (stop_date_.empty())
        {
            end_date_ = begin_date_;
        }
    }

    if (start_date_.empty() && stop_date_.empty())
    {
        spdlog::info("Neither begin date nor end date specified. No date range filtering to be done.");
    }

    if (! data_source_.empty())
    {
        BOOST_ASSERT_MSG(data_source_ == "BOTH" || data_source_ == "HTML" || data_source_ == "XBRL", "Mode must be: 'BOTH' or 'HTML' or 'XBRL'.");
    }

    if (! DB_mode_.empty())
    {
        BOOST_ASSERT_MSG(DB_mode_ == "test" || DB_mode_ == "live", "DB-mode must be: 'test' or 'live'.");
        schema_prefix_ = (DB_mode_ == "test" ? "" : "live_");
    }

    //  the user may specify multiple form types in a comma delimited list. We need to parse the entries out
    //  of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

    if (! form_.empty())
    {
        // since we use form type as part of our file name for forms stored on disk,
        // we can't have the '/' character in it.  Our Collect program replaces the '/' with '_'
        // so we do the same here.

        form_ |= ranges::actions::transform([](char c) { return (c == '/' ? '_' : c); });
        form_list_ = split_string<std::string>(form_, ',');
    }

    if (! CIK_.empty())
    {
        CIK_list_ = split_string<std::string>(CIK_, ',');
        BOOST_ASSERT_MSG(std::all_of(CIK_list_.cbegin(), CIK_list_.cend(), [](auto e) { return e.size() == 10; }),
                "All CIKs must be 10 digits in length.");
    }

    if (! SIC_.empty())
    {
        SIC_list_ = split_string<std::string>(SIC_, ',');
    }

    if (! single_file_to_process_.get().empty())
    {
        BOOST_ASSERT_MSG(fs::exists(single_file_to_process_.get()), catenate("Can't find file: ", single_file_to_process_.get().string()).c_str());
        BOOST_ASSERT_MSG(fs::is_regular_file(single_file_to_process_.get()), catenate("Path :", single_file_to_process_.get().string(),
                    " is not a regular file.").c_str());
    }

    if (! local_form_file_directory_.get().empty())
    {
        BOOST_ASSERT_MSG(fs::exists(local_form_file_directory_.get()), catenate("Can't find SEC file directory: ",
                    local_form_file_directory_.get()).c_str());
        BOOST_ASSERT_MSG(fs::is_directory(local_form_file_directory_.get()),
                catenate("Path: ", local_form_file_directory_.get(), " is not a directory.").c_str());
    }
    
    if (! resume_at_this_filename_.empty())
    {
        BOOST_ASSERT_MSG(list_of_files_to_process_.empty(),
                "You must provide a list of files to process when specifying file to resume at.");
    }

    auto list_of_files_to_process_path_val = list_of_files_to_process_path_.get();
    if (! list_of_files_to_process_path_val.empty())
    {
        BOOST_ASSERT_MSG(fs::exists(list_of_files_to_process_path_val),
                catenate("Can't find file: ", list_of_files_to_process_path_val).c_str());
        BOOST_ASSERT_MSG(fs::is_regular_file(list_of_files_to_process_path_val),
                catenate("Path: ", list_of_files_to_process_path_val, " is not a regular file.").c_str());
        BuildListOfFilesToProcess();
    }

    BOOST_ASSERT_MSG(NotAllEmpty(single_file_to_process_.get(), local_form_file_directory_.get(), list_of_files_to_process_),
            "No files to process found.");

    BuildFilterList();

    if (export_XLS_files_)
    {
        BOOST_ASSERT_MSG(! SS_export_directory_.get().empty(), "Must specify SS export directory.");
    }

    if (export_HTML_forms_)
    {
        BOOST_ASSERT_MSG(! HTML_export_source_directory_.get().empty(), "Must specify HTML export source directory.");
        BOOST_ASSERT_MSG(! HTML_export_target_directory_.get().empty(), "Must specify HTML export target directory.");
        html_hierarchy_converter_ = ConvertInputHierarchyToOutputHierarchy(HTML_export_source_directory_, HTML_export_target_directory_);
    }

    if (update_shares_outstanding_)
    {
        BOOST_ASSERT_MSG(data_source_ == "HTML", "Must use HTML mode.");
    }

    return true;
}       // -----  end of method ExtractorApp::CheckArgs  -----

void ExtractorApp::BuildListOfFilesToProcess()
{
    list_of_files_to_process_.clear();      //  in case of reprocessing.

    file_list_data_ = LoadDataFileForUse(list_of_files_to_process_path_);

    // coult be a million files so use list of string_views

    list_of_files_to_process_ = split_string<EM::sv>(file_list_data_, '\n');

    spdlog::info(catenate("Found: ", list_of_files_to_process_.size(), " files in list."));

    if (resume_at_this_filename_.empty())
    {
        return;
    }

    auto pos = ranges::find(list_of_files_to_process_, resume_at_this_filename_);
    BOOST_ASSERT_MSG(pos != std::end(list_of_files_to_process_),
            catenate("File: ", resume_at_this_filename_, " not found in list of files.").c_str());

    list_of_files_to_process_.erase(list_of_files_to_process_.begin(), pos);

    spdlog::info(catenate("Resuming with: ", list_of_files_to_process_.size(), " files in list."));
}		/* -----  end of method ExtractorApp::BuildListOfFilesToProcess  ----- */

void ExtractorApp::BuildFilterList()
{
    //  // XBRL and HTML filters will be applied manually, later
    
    //  we always need to do this first since it just needs the SEC header fields.

    if (! start_date_.empty() || ! stop_date_.empty())
    {
        filters_.emplace_back(FileIsWithinDateRange{begin_date_, end_date_});
    }

    if (! (export_HTML_forms_ || update_shares_outstanding_))
    {
        filters_.emplace_back(NeedToUpdateDBContent{schema_prefix_, data_source_, replace_DB_content_});
    }

    if (! form_.empty())
    {
        filters_.emplace_back(FileHasFormType{form_list_});
    }

    if (! CIK_.empty())
    {
        filters_.emplace_back(FileHasCIK{CIK_list_});
    }

    if (! SIC_.empty())
    {
        filters_.emplace_back(FileHasSIC{SIC_list_});
    }
}		/* -----  end of method ExtractorApp::BuildFilterList  ----- */

void ExtractorApp::Run()
{
    std::tuple<int, int, int> single_counters{0, 0, 0};

    // for now, I know this is all we are doing.

    if (! single_file_to_process_.get().empty())
    {
        single_counters = this->LoadSingleFileToDB(single_file_to_process_);
    }

    std::tuple<int, int, int> list_counters{0, 0, 0};

    if (! list_of_files_to_process_.empty())
    {
        if (max_at_a_time_ < 1)
        {
            list_counters = this->LoadFilesFromListToDB();
        }
        else
        {
            list_counters = this->LoadFilesFromListToDBConcurrently();
        }
    }

    std::tuple<int, int, int> local_counters{0, 0, 0};

    if (! local_form_file_directory_.get().empty())
    {
        local_counters = this->ProcessDirectory();
    }

    std::tuple<int, int, int> counters{0, 0, 0};
    counters = AddTs(counters, single_counters);
    counters = AddTs(counters, list_counters);
    counters = AddTs(counters, local_counters);

    auto [success_counter, skipped_counter, error_counter] = counters;

    spdlog::info(catenate("Processed: ", SumT(counters), " files. Successes: ",
            success_counter, ". Skips: ", skipped_counter , ". Errors: ", error_counter, "."));
}		/* -----  end of method ExtractorApp::Run  ----- */

std::optional<ExtractorApp::FileMode> ExtractorApp::ApplyFilters(const EM::SEC_Header_fields& SEC_fields, EM::FileName file_name, const EM::DocumentSectionList& sections,
        std::atomic<int>* forms_processed)
{
    bool use_file{true};
    for (const auto& filter : filters_)
    {
        use_file = std::visit([&SEC_fields, sections](auto& f) -> bool { return f(SEC_fields, sections); }, filter);
        if (! use_file)
        {
            spdlog::info(catenate(file_name.get(), ": File skipped because of filter: ",
                std::visit([](auto& f) -> std::string { return f.filter_name_; }, filter), "."));
            return std::nullopt;
        }
    }

    // these 2 filters are a little more expensive to use, so do after the above tests
    // also, if we find we have XBRL, don't look for HTML.

    if (data_source_ == "BOTH" || data_source_ == "XBRL")
    {
        FileHasXBRL filter1;
        use_file = filter1(EM::SEC_Header_fields{}, sections);
        if (use_file)
        {
            auto x = forms_processed->fetch_add(1);
            if (max_forms_to_process_ > 0 && x >= max_forms_to_process_)
            {
                throw MaxFilesException(catenate("Exceeded file limit: ", max_forms_to_process_, '\n'));
            }
            return FileMode{FileMode::e_XBRL};
        }
        else if (data_source_ == "XBRL")
        {
            spdlog::info(catenate(file_name.get(), ": File skipped because of filter: ", filter1.filter_name_));
            return std::nullopt;
        }
    }
    if (data_source_ == "BOTH" || data_source_ == "HTML")
    {
        FileHasHTML filter1{form_list_};
        use_file = filter1(EM::SEC_Header_fields{}, sections);
        if (use_file)
        {
            auto x = forms_processed->fetch_add(1);
            if (max_forms_to_process_ > 0 && x >= max_forms_to_process_)
            {
                throw MaxFilesException(catenate("Exceeded file limit: ", max_forms_to_process_, '\n'));
            }
            return FileMode{FileMode::e_HTML};
        }
        else
        {
            spdlog::info(catenate(file_name.get(), ": File skipped because of filter: ", filter1.filter_name_));
        }
    }

    return std::nullopt;
}

std::tuple<int, int, int> ExtractorApp::LoadSingleFileToDB(EM::FileName input_file_name)
{
    std::atomic<int> forms_processed{0};
    try
    {
        const std::string content{LoadDataFileForUse(input_file_name)};
        EM::FileContent file_content{content};
        const auto document_sections = LocateDocumentSections(file_content);

        SEC_Header SEC_data;
        SEC_data.UseData(file_content);
        SEC_data.ExtractHeaderFields();
        decltype(auto) SEC_fields = SEC_data.GetFields();

        auto use_file = this->ApplyFilters(SEC_fields, input_file_name, document_sections, &forms_processed);
        BOOST_ASSERT_MSG(use_file, "Specified file does not meet other criteria.");

        if (use_file.value() == FileMode::e_XBRL)
        {
            return LoadSingleFileToDB_XBRL(file_content, document_sections, SEC_fields, input_file_name);
        }
        return LoadSingleFileToDB_HTML(file_content, document_sections, SEC_data.GetHeader(), SEC_fields, input_file_name);
    }
    catch(const std::system_error& e)
    {
        spdlog::error(catenate("System error while processing file: ", input_file_name.get().string(), ". ", e.what(), " Processing stopped."));
        throw;
    }
    catch(const std::exception& e)
    {
        spdlog::error(catenate("Problem processing file: ", input_file_name.get().string(), ". ", e.what()));
        return {0, 0, 1};
    }

}		/* -----  end of method ExtractorApp::LoadSingleFileToDB  ----- */

std::tuple<int, int, int> ExtractorApp::LoadSingleFileToDB_XBRL(EM::FileContent file_content, const EM::DocumentSectionList& document_sections,
        const EM::SEC_Header_fields& SEC_fields, EM::FileName input_file_name)
{
    auto labels_document = LocateLabelDocument(document_sections);
    auto labels_xml = ParseXMLContent(labels_document);

    auto instance_document = LocateInstanceDocument(document_sections);
    auto instance_xml = ParseXMLContent(instance_document);

    auto filing_data = ExtractFilingData(instance_xml);
    auto gaap_data = ExtractGAAPFields(instance_xml);
    auto context_data = ExtractContextDefinitions(instance_xml);
    auto label_data = ExtractFieldLabels(labels_xml);

    bool did_load = LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, schema_prefix_ + "unified_extracts");

    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}		/* -----  end of method ExtractorApp::LoadSingleFileToDB_XBRL  ----- */

std::tuple<int, int, int> ExtractorApp::LoadSingleFileToDB_HTML(EM::FileContent file_content, const EM::DocumentSectionList& document_sections,
        EM::sv sec_header, const EM::SEC_Header_fields& SEC_fields, EM::FileName input_file_name)
{
    if (update_shares_outstanding_)
    {
        UpdateOutstandingShares(so_, document_sections, SEC_fields, form_list_, schema_prefix_ + "unified_extracts", input_file_name);
        return {1, 0, 0};
    }

    if (export_HTML_forms_)
    {
        if (ExportHtmlFromSingleFile(document_sections, input_file_name, sec_header))
        {
            return {1, 0, 0};
        }
        return {0, 0, 1};
    }

    auto the_tables = FindAndExtractFinancialStatements(so_, &document_sections, form_list_);
    BOOST_ASSERT_MSG(the_tables.has_data(), catenate("Can't find required HTML financial tables: ",
        input_file_name.get()).c_str());

    BOOST_ASSERT_MSG(! the_tables.ListValues().empty(), catenate("Can't find any data fields in tables: ",
        input_file_name.get()).c_str());

//        did_load = true;
    bool did_load = LoadDataToDB(SEC_fields, the_tables, schema_prefix_ + "unified_extracts");
    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}		/* -----  end of method ExtractorApp::LoadSingleFileToDB_HTML  ----- */


bool ExtractorApp::ExportHtmlFromSingleFile (const EM::DocumentSectionList& sections, EM::FileName file_name, EM::sv sec_header)
{
    // reuse top level logic from financial statements.
    // we don't need to actually isolate the financial data, just be sure it's there.

    static const boost::regex regex_finance_statements{R"***(financ.+?statement)***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_operations{R"***((?:statement|statements)\s+?of.*?(?:oper|loss|income|earning))***",
        boost::regex_constants::normal | boost::regex_constants::icase};
    static const boost::regex regex_cash_flow{R"***((?:statement|statements)\s+?of\s+?cash\sflow)***",
        boost::regex_constants::normal | boost::regex_constants::icase};

    auto regex_document_filter([] (const auto& html_info)
        {
            auto html_val = html_info.html_.get();
            if (boost::regex_search(html_val.cbegin(), html_val.cend(), regex_finance_statements))
            {
                if (boost::regex_search(html_val.cbegin(), html_val.cend(), regex_operations))
                {
                    if (boost::regex_search(html_val.cbegin(), html_val.cend(), regex_cash_flow))
                    {
                        return true;
                    }
                }
            }
            return false;
        });

    HTML_FromFile htmls{&sections};

    auto financial_content = ranges::find_if(htmls, regex_document_filter);
    if (financial_content == htmls.end())
    {
        spdlog::info(catenate("Unable to find financial content in file: ", file_name.get(), " Looking for forms..."));
        FinancialDocumentFilter document_filter(form_list_);
        financial_content = std::find_if(std::begin(htmls), std::end(htmls), document_filter);
    }
    if (financial_content != htmls.end())
    {
        auto output_path_name = html_hierarchy_converter_(file_name, financial_content->file_name_.get().string());

        if (! replace_DB_content_ && fs::exists(output_path_name))
        {
            spdlog::info(catenate("File: ", output_path_name.string(), " exists and 'replace' not specified."));
            return false;
        }

        auto output_directory = output_path_name.parent_path();
        if (! fs::exists(output_directory))
        {
            fs::create_directories(output_directory);
        }

        std::ofstream exported_file(output_path_name);
        if (not exported_file)
        {
            throw(std::runtime_error(catenate("Can't open output file: ", output_path_name.string())));
        }

        // we can just dump out our content but...we need to verify that we actually wrote it
        // the disk could be full or some other problem could occur so, let's check..

        errno = 0;
        exported_file.write(sec_header.data(), sec_header.size());
        if (exported_file.fail())
        {
            std::error_code err{errno, std::system_category()};
            throw std::system_error{err, catenate("Unable to complete export of HTML content: ", file_name.get())};
        }
        exported_file.put('\n');

        errno = 0;
        exported_file.write(financial_content->document_.get().data(), financial_content->document_.get().size());
        exported_file.close();
        if (exported_file.fail())
        {
            std::error_code err{errno, std::system_category()};
            throw std::system_error{err, catenate("Unable to complete export of HTML content: ", file_name.get())};
        }
        
        return true;
    }
    spdlog::info(catenate("Unable to find any form content in file: ", file_name.get()));

    return false;
}		// -----  end of method ExtractorApp::ExportHtmlFromSingleFile  ----- 

std::tuple<int, int, int> ExtractorApp::LoadFilesFromListToDB()
{
    int success_counter{0};
    int skipped_counter{0};
    int error_counter{0};

    std::atomic<int> forms_processed{0};

    auto process_file([this, &forms_processed, &success_counter, &skipped_counter, &error_counter](const auto& file_name)
    {
        Do_SingleFile(&forms_processed, success_counter, skipped_counter, error_counter, EM::FileName{file_name});
    });

    ranges::for_each(list_of_files_to_process_, process_file);

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractorApp::LoadFilesFromListToDB  ----- */

void ExtractorApp::Do_SingleFile(std::atomic<int>* forms_processed, int& success_counter, int& skipped_counter,
        int& error_counter, EM::FileName file_name)
{
    if (fs::is_regular_file(file_name.get()))
    {
        try
        {
            if (filename_has_form_)
            {
                if (! FormIsInFileName(form_list_, file_name))
                {
                    ++skipped_counter;
                    spdlog::info(catenate(file_name.get(), ": File skipped because path is supposed to contain form name but doesn't."));
                    return;
                }
            }
            spdlog::info(catenate("Scanning file: ", file_name.get()));
            const std::string content{LoadDataFileForUse(file_name)};
            EM::FileContent file_content{content};
            const auto document_sections = LocateDocumentSections(file_content);

            SEC_Header SEC_data;
            SEC_data.UseData(file_content);
            SEC_data.ExtractHeaderFields();
            decltype(auto) SEC_fields = SEC_data.GetFields();
            auto sec_header = SEC_data.GetHeader();

            if (auto use_file = this->ApplyFilters(SEC_fields, file_name,  document_sections, forms_processed); use_file)
            {
                LoadFileFromFolderToDB(file_name, SEC_fields, document_sections, sec_header, use_file.value()) ? ++success_counter : ++skipped_counter;
            }
            else
            {
                ++skipped_counter;
            }
        }
        catch(MaxFilesException& e)
        {
            // reached our limit of files to process, so let's get out of here.

            ++error_counter;
            spdlog::error(catenate("Max files reached: ", file_name.get(), ". ", e.what()));
            spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                    " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                    ". Errors: ", error_counter, "."));
            throw;
        }
        catch(std::system_error& e)
        {
            // reached our limit of files to process, so let's get out of here.

            ++error_counter;
            spdlog::error(catenate("Problem processing file: ", file_name.get(), ". ", e.what()));
            spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                    " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                    ". Errors: ", error_counter, "."));
            throw;
        }
        catch(std::exception& e)
        {
            ++error_counter;
            spdlog::error(catenate("Problem processing file: ", file_name.get(), ". ", e.what()));
            spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                    " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                    ". Errors: ", error_counter, "."));
        }
    }
}		/* -----  end of method ExtractorApp::Do_SingleFile  ----- */

std::tuple<int, int, int> ExtractorApp::ProcessDirectory()
{
    int success_counter{0};
    int skipped_counter{0};
    int error_counter{0};

    std::atomic<int> forms_processed{0};

    auto process_file([this, &forms_processed, &success_counter, &skipped_counter, &error_counter](const auto& dir_ent)
    {
        if (dir_ent.status().type() == fs::file_type::regular)
        {
            Do_SingleFile(&forms_processed, success_counter, skipped_counter, error_counter, EM::FileName{dir_ent.path()});
        }
    });

    ranges::for_each(fs::recursive_directory_iterator(local_form_file_directory_.get()), fs::recursive_directory_iterator(),
            process_file);

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractorApp::ProcessDirectory  ----- */

bool ExtractorApp::LoadFileFromFolderToDB(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields,
        const EM::DocumentSectionList& sections, EM::sv sec_header, FileMode file_mode)
{
    spdlog::info(catenate("Loading contents from file: ", file_name.get()));

    if (file_mode == FileMode::e_XBRL)
    {
        return LoadFileFromFolderToDB_XBRL(file_name, SEC_fields, sections);
    }
    return LoadFileFromFolderToDB_HTML(file_name, SEC_fields, sections, sec_header);
}		/* -----  end of method ExtractorApp::LoadFileFromFolderToDB  ----- */

bool ExtractorApp::LoadFileFromFolderToDB_XBRL(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields,
        const EM::DocumentSectionList& document_sections)
{
    auto labels_document = LocateLabelDocument(document_sections);
    auto labels_xml = ParseXMLContent(labels_document);

    auto instance_document = LocateInstanceDocument(document_sections);
    auto instance_xml = ParseXMLContent(instance_document);

    auto filing_data = ExtractFilingData(instance_xml);
    auto gaap_data = ExtractGAAPFields(instance_xml);
    auto context_data = ExtractContextDefinitions(instance_xml);
    auto label_data = ExtractFieldLabels(labels_xml);

    return LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data,
                schema_prefix_ + "unified_extracts");
}		/* -----  end of method ExtractorApp::LoadFileFromFolderToDB_XBRL  ----- */

bool ExtractorApp::LoadFileFromFolderToDB_HTML(EM::FileName file_name, const EM::SEC_Header_fields& SEC_fields,
        const EM::DocumentSectionList& sections, EM::sv sec_header)
{
    if (update_shares_outstanding_)
    {
        UpdateOutstandingShares(so_, sections, SEC_fields, form_list_, schema_prefix_ + "unified_extracts", file_name);
        return true;
    }

    if (export_HTML_forms_)
    {
        return ExportHtmlFromSingleFile(sections, file_name, sec_header);
    }

    auto the_tables = FindAndExtractFinancialStatements(so_, &sections, form_list_);
    BOOST_ASSERT_MSG(the_tables.has_data(), catenate("Can't find required HTML financial tables: ", file_name.get()).c_str());

    BOOST_ASSERT_MSG(! the_tables.ListValues().empty(), catenate("Can't find any data fields in tables: ", file_name.get()).c_str());
    return LoadDataToDB(SEC_fields, the_tables, schema_prefix_ + "unified_extracts");
}		/* -----  end of method ExtractorApp::LoadFileFromFolderToDB_HTML  ----- */

std::tuple<int, int, int> ExtractorApp::LoadFileAsync(EM::FileName file_name, std::atomic<int>* forms_processed)
{
    int success_counter{0};
    int skipped_counter{0};
    int error_counter{0};

    if (filename_has_form_)
    {
        if (! FormIsInFileName(form_list_, file_name))
        {
            ++skipped_counter;
            return {success_counter, skipped_counter, error_counter};
        }
    }
    
    spdlog::info(catenate("Scanning file: ", file_name.get()));
    const std::string content(LoadDataFileForUse(file_name));
    EM::FileContent file_content{content};
    const auto document_sections = LocateDocumentSections(file_content);

    SEC_Header SEC_data;
    SEC_data.UseData(file_content);
    SEC_data.ExtractHeaderFields();
    decltype(auto) SEC_fields = SEC_data.GetFields();
    auto sec_header = SEC_data.GetHeader();

    if (auto use_file = this->ApplyFilters(SEC_fields, file_name, document_sections, forms_processed); use_file)
    {
        LoadFileFromFolderToDB(file_name, SEC_fields, document_sections, sec_header, use_file.value()) ? ++success_counter : ++skipped_counter;
    }
    else
    {
        spdlog::info(catenate("Skipping file: ", file_name.get()));
        ++skipped_counter;
    }

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractorApp::LoadFileAsync  ----- */

std::tuple<int, int, int> ExtractorApp::LoadFilesFromListToDBConcurrently()
{
    // since this code can potentially run for hours on end (depending on database throughput)
    // it's a good idea to provide a way to break into this processing and shut it down cleanly.
    // so, a little bit of C...(taken from "Advanced Unix Programming" by Warren W. Gay, p. 317)

    struct sigaction sa_old;
    struct sigaction sa_new;

    // ok, get ready to handle keyboard interrupts, if any.

    sa_new.sa_handler = ExtractorApp::HandleSignal;
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;
    sigaction(SIGINT, &sa_new, &sa_old);

    ExtractorApp::had_signal_= false;

    // make exception handling a little bit better (I think).
    // If some kind of system error occurs, it may affect more than 1 of
    // our our tasks so let's check each of them and log any exceptions
    // which occur. We'll then rethrow our first exception.

    std::exception_ptr ep{nullptr};

    std::tuple<int, int, int> counters{0, 0, 0};  // success, skips, errors

    std::atomic<int> forms_processed{0};

    // keep track of our async processes here.

    std::vector<std::future<std::tuple<int, int, int>>> tasks;
    tasks.reserve(max_at_a_time_);

    // prime the pump...

    size_t current_file{0};
    for ( ; tasks.size() < max_at_a_time_ && current_file < list_of_files_to_process_.size(); ++current_file)
    {
        // queue up our tasks up to the limit.

        tasks.emplace_back(std::async(std::launch::async, &ExtractorApp::LoadFileAsync, this,
        EM::FileName{list_of_files_to_process_[current_file]}, &forms_processed));
    }

    int continue_here{0};
    int ready_task{-1};

    for ( ; current_file < list_of_files_to_process_.size(); ++current_file)
    {
        // we want to keep max_at_a_time_ tasks going so, as one finishes,
        // we replace it with another
    
        ready_task = wait_for_any(tasks, continue_here, std::chrono::microseconds{100});
        if (ready_task < 0)
        {
            break;
        }
        try
        {
            auto result = tasks[ready_task].get();
            counters = AddTs(counters, result);
        }
        catch (std::system_error& e)
        {
            // any system problems, we eventually abort, but only after finishing work in process.

            spdlog::error(e.what());
            auto ec = e.code();
            spdlog::error(catenate("Category: ", ec.category().name(), ". Value: ", ec.value(),
                    ". Message: ", ec.message()));
            counters = AddTs(counters, {0, 0, 1});

            // OK, let's be sure this propagates

            ep = std::current_exception();
            break;
        }
        catch (MaxFilesException& e)
        {
            // any 'expected' problems, we'll document them and continue on.

            spdlog::error(e.what());
            counters = AddTs(counters, {0, 0, 1});

            if (! ep)
            {
                ep = std::current_exception();
            }
            break;
        }
        catch(const pqxx::sql_error& e)
        {
            spdlog::error(catenate("Database error: ", e.what()));
            spdlog::error(catenate("Query was: ", e.query()));
            counters = AddTs(counters, {0, 0, 1});

//            // OK, let's remember our first time here.

            if (! ep)
            {
                ep = std::current_exception();
            }
//            break;
        }
        catch (std::exception& e)
        {
            // any 'expected' problems, we'll document them and continue on.

            spdlog::error(e.what());
            counters = AddTs(counters, {0, 0, 1});

//            // OK, let's remember our first time here.

            if (! ep)
            {
                ep = std::current_exception();
            }
//            break;
        }
        catch (...)
        {
            // any problems, we'll document them and continue.

            spdlog::error("Unknown problem with async file processing");
            counters = AddTs(counters, {0, 0, 1});

            // OK, let's remember our first time here.

            if (! ep)
            {
                ep = std::current_exception();
            }
            break;
        }

//        if (ep || ExtractorApp::had_signal_)
        if (ExtractorApp::had_signal_)
        {
            break;
        }

        //  let's keep going

        if (current_file < list_of_files_to_process_.size())
        {
            tasks[ready_task] = std::async(std::launch::async, &ExtractorApp::LoadFileAsync, this,
                    EM::FileName{list_of_files_to_process_[current_file]}, &forms_processed);
            continue_here = (ready_task + 1) % max_at_a_time_;
            ready_task = -1;
        }
    }

    // need to clean up the last set of tasks
    for(int i = 0; i < max_at_a_time_; ++i)
    {
        try
        {
            if (ready_task > -1 && i == ready_task)
            {
                continue;
            }
            if (tasks[i].valid())
            {
                auto result = tasks[i].get();
                counters = AddTs(counters, result);
            }
        }
        catch (ExtractorException& e)
        {
            // any problems, we'll document them and continue.

            spdlog::error(e.what());
            counters = AddTs(counters, {0, 0, 1});

            // we ignore these...

            continue;
        }
        catch(std::exception& e)
        {
            counters = AddTs(counters, {0, 0, 1});
            if (! ep)
            {
                ep = std::current_exception();
            }
        }
    }

    auto [success_counter, skipped_counter, error_counter] = counters;

    if (ep)
    {
        spdlog::error(catenate("Processed: ", SumT(counters), " files. Successes: ", success_counter,
                ". Skips: ", skipped_counter, ". Errors: ", error_counter, "."));
        std::rethrow_exception(ep);
    }

    if (ExtractorApp::had_signal_)
    {
        spdlog::error(catenate("Processed: ", SumT(counters), " files. Successes: ", success_counter,
                ". Skips: ", skipped_counter, ". Errors: ", error_counter, "."));
        throw std::runtime_error("Received keyboard interrupt.  Processing manually terminated after loading: "
            + std::to_string(success_counter) + " files.");
    }

    // if we return successfully, let's just restore the default

    sigaction(SIGINT, &sa_old, 0);

    return counters;

}		/* -----  end of method ExtractorApp::LoadFilesFromListToDBConcurrently  ----- */

void ExtractorApp::HandleSignal(int signal)

{
    std::signal(SIGINT, ExtractorApp::HandleSignal);

    // only thing we need to do

    ExtractorApp::had_signal_ = true;

}		/* -----  end of method ExtractorApp::HandleSignal  ----- */

void ExtractorApp::Shutdown ()
{
    spdlog::info(catenate("\n\n*** End run ", LocalDateTimeAsString(std::chrono::system_clock::now()), " ***\n"));
}       // -----  end of method ExtractorApp::Shutdown  -----

