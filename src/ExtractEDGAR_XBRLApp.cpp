// =====================================================================================
//
//       Filename:  ExtractEDGAR_XBRLApp.cpp
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

    /* This file is part of CollectEDGARData. */

    /* CollectEDGARData is free software: you can redistribute it and/or modify */
    /* it under the terms of the GNU General Public License as published by */
    /* the Free Software Foundation, either version 3 of the License, or */
    /* (at your option) any later version. */

    /* CollectEDGARData is distributed in the hope that it will be useful, */
    /* but WITHOUT ANY WARRANTY; without even the implied warranty of */
    /* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
    /* GNU General Public License for more details. */

    /* You should have received a copy of the GNU General Public License */
    /* along with CollectEDGARData.  If not, see <http://www.gnu.org/licenses/>. */


//--------------------------------------------------------------------------------------
//       Class:  ExtractEDGAR_XBRLApp
//      Method:  ExtractEDGAR_XBRLApp
// Description:  constructor
//--------------------------------------------------------------------------------------

#include "ExtractEDGAR_XBRLApp.h"

#include <map>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <exception>
#include <fstream>
#include <fstream>
#include <future>
#include <iterator>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
// #include <parallel/algorithm>
// #include <streambuf>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <pqxx/pqxx>

#include "EDGAR_HTML_FileFilter.h"
#include "EDGAR_XBRL_FileFilter.h"
#include "ExtractEDGAR_Utils.h"
#include "SEC_Header.h"

using namespace std::string_literals;

// This ctype facet does NOT classify spaces and tabs as whitespace
// from cppreference example

struct line_only_whitespace : std::ctype<char>
{
    static const mask* make_table()
    {
        // make a copy of the "C" locale table
        static std::vector<mask> v(classic_table(), classic_table() + table_size);
        v['\t'] &= ~space;      // tab will not be classified as whitespace
        v[' '] &= ~space;       // space will not be classified as whitespace
        return &v[0];
    }
    explicit line_only_whitespace(std::size_t refs = 0) : ctype(make_table(), false, refs) {}
};

bool ExtractEDGAR_XBRLApp::had_signal_ = false;

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

        if (ExtractEDGAR_XBRLApp::SignalReceived())
        {
            break;
        }

        std::this_thread::sleep_for(d);
    }

    return -1;
}

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractEDGAR_XBRLApp
 *      Method:  ExtractEDGAR_XBRLApp
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp (int argc, char* argv[])
    : mArgc{argc}, mArgv{argv}
{
}  /* -----  end of method ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp  (constructor)  ----- */

/*
 *--------------------------------------------------------------------------------------
 *       Class:  ExtractEDGAR_XBRLApp
 *      Method:  ExtractEDGAR_XBRLApp
 * Description:  constructor
 *--------------------------------------------------------------------------------------
 */
ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp (const std::vector<std::string>& tokens)
    : tokens_{tokens}
{
}  /* -----  end of method ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp  (constructor)  ----- */

void ExtractEDGAR_XBRLApp::ConfigureLogging()
{
    // we need to set log level if specified and also log file.

    // we are running before 'CheckArgs' so we need to do a little editiing ourselves.

    std::map<std::string, spdlog::level::level_enum> levels{
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

    if (! log_file_path_name_.empty())
    {
        fs::path log_dir = log_file_path_name_.parent_path();
        if (! fs::exists(log_dir))
        {
            fs::create_directories(log_dir);
        }

        auto file_logger = spdlog::basic_logger_mt("filelogger", log_file_path_name_.c_str());
        spdlog::set_default_logger(file_logger);
    }
}

bool ExtractEDGAR_XBRLApp::Startup()
{
    spdlog::info(("\n\n*** Begin run "
           + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n"));
    bool result{true};
	try
	{	
		SetupProgramOptions();
        if (tokens_.empty())
            ParseProgramOptions();
        else
            ParseProgramOptions(tokens_);
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
}		/* -----  end of method ExtractEDGAR_XBRLApp::Startup  ----- */

void ExtractEDGAR_XBRLApp::SetupProgramOptions ()
{
	mNewOptions.add_options()
		("help,h", "produce help message")
		("begin-date", po::value<bg::date>(&this->begin_date_), "retrieve files with dates greater than or equal to")
		("end-date", po::value<bg::date>(&this->end_date_), "retrieve files with dates less than or equal to")
		("form-dir", po::value<fs::path>(&local_form_file_directory_),	"directory of form files to be processed")
		("file,f", po::value<fs::path>(&single_file_to_process_),	"single file to be processed.")
		("replace-DB-content,R", po::value<bool>(&replace_DB_content_)->default_value(false)->implicit_value(true),
            "replace all DB content for each file. Default is 'false'")
		("list-file", po::value<fs::path>(&list_of_files_to_process_path_),"path to file with list of files to process.")
		("log-level,l", po::value<std::string>(&logging_level_),
         "logging level. Must be 'none|error|information|debug'. Default is 'information'.")
		("mode,m", po::value<std::string>(&mode_)->required(), "Must be either 'HTML' or 'XBRL'.")
		("form", po::value<std::string>(&form_)->default_value("10-Q"),
         "name of form type we are processing. May be comma-delimited list. Default is '10-Q'.")
		("CIK",	po::value<std::string>(&CIK_),
         "Zero-padded-on-left 10 digit CIK[s] we are processing. May be comma-delimited list. Default is all.")
		("SIC",	po::value<std::string>(&SIC_),
         "SIC we are processing. May be comma-delimited list. Default is all.")
		("log-path", po::value<fs::path>(&log_file_path_name_),	"path name for log file.")
		("max-files", po::value<int>(&max_forms_to_process_)->default_value(-1),
         "Maximun number of forms to process -- mainly for testing. Default of -1 means no limit.")
		("concurrent,k", po::value<int>(&max_at_a_time_)->default_value(-1),
         "Maximun number of concurrent processes. Default of -1 -- system defined.")
		("filename-has-form", po::value<bool>(&filename_has_form_)->default_value(false)->implicit_value(true),
            "form number is in file path. Default is 'false'")
		("resume-at", po::value<std::string>(&resume_at_this_filename_),
         "find this file name in list of files to process and resume processing there.")
		;
}		/* -----  end of method ExtractEDGAR_XBRLApp::SetupProgramOptions  ----- */

void ExtractEDGAR_XBRLApp::ParseProgramOptions ()
{
	auto options = po::parse_command_line(mArgc, mArgv, mNewOptions);
	po::store(options, mVariableMap);
	if (this->mArgc == 1	||	mVariableMap.count("help") == 1)
	{
		std::cout << mNewOptions << "\n";
		throw std::runtime_error("\nExiting after 'help'.");
	}
	po::notify(mVariableMap);    

}		/* -----  end of method ExtractEDGAR_XBRLApp::ParseProgramOptions  ----- */

void ExtractEDGAR_XBRLApp::ParseProgramOptions (const std::vector<std::string>& tokens)
{
	auto options = po::command_line_parser(tokens).options(mNewOptions).run();
	po::store(options, mVariableMap);
	if (mVariableMap.count("help") == 1)
	{
		std::cout << mNewOptions << "\n";
		throw std::runtime_error("\nExiting after 'help'.");
	}
	po::notify(mVariableMap);    
}		/* -----  end of method ExtractEDGAR_XBRLApp::ParseProgramOptions  ----- */

bool ExtractEDGAR_XBRLApp::CheckArgs ()
{
    if (begin_date_ != bg::date())
    {
        if (end_date_ == bg::date())
        {
            end_date_ = begin_date_;
        }
    }

    if (begin_date_ == bg::date() && end_date_ == begin_date_)
    {
        spdlog::info("Neither begin date nor end date specified. No date range filtering to be done.");
    }

    //  the user may specify multiple form types in a comma delimited list. We need to parse the entries out
    //  of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

    if (! mode_.empty())
    {
        BOOST_ASSERT_MSG(mode_ == "HTML"s || mode_ == "XBRL"s, "Mode must be: 'HTML' or 'XBRL'.");
    }

    if (! form_.empty())
    {
        form_list_ = split_string(form_, ',');
    }

    if (! CIK_.empty())
    {
        CIK_list_ = split_string(CIK_, ',');
        BOOST_ASSERT_MSG(std::all_of(CIK_list_.cbegin(), CIK_list_.cend(), [](auto e) { return e.size() == 10; }),
                "All CIKs must be 10 digits in length.");
    }

    if (! SIC_.empty())
    {
        SIC_list_ = split_string(SIC_, ',');
    }

    if (! single_file_to_process_.empty())
    {
        BOOST_ASSERT_MSG(fs::exists(single_file_to_process_), ("Can't find file: " + single_file_to_process_.string()).c_str());
        BOOST_ASSERT_MSG(fs::is_regular_file(single_file_to_process_), ("Path :"s + single_file_to_process_.string()
                    + " is not a regular file.").c_str());
    }

    if (! local_form_file_directory_.empty())
    {
        BOOST_ASSERT_MSG(fs::exists(local_form_file_directory_), ("Can't find EDGAR file directory: "
                    + local_form_file_directory_.string()).c_str());
        BOOST_ASSERT_MSG(fs::is_directory(local_form_file_directory_),
                ("Path :"s + local_form_file_directory_.string() + " is not a directory.").c_str());
    }
    
    if (! resume_at_this_filename_.empty())
    {
        BOOST_ASSERT_MSG(list_of_files_to_process_.empty(),
                "You must provide a list of files to process when specifying file to resume at.");
    }

    if (! list_of_files_to_process_path_.empty())
    {
        BOOST_ASSERT_MSG(fs::exists(list_of_files_to_process_path_),
                ("Can't find file: " + list_of_files_to_process_path_.string()).c_str());
        BOOST_ASSERT_MSG(fs::is_regular_file(list_of_files_to_process_path_),
                ("Path :"s + list_of_files_to_process_path_.string() + " is not a regular file.").c_str());
        BuildListOfFilesToProcess();
    }

    BOOST_ASSERT_MSG(NotAllEmpty(single_file_to_process_, local_form_file_directory_, list_of_files_to_process_),
            "No files to process found.");

    BuildFilterList();

    return true;
}       // -----  end of method ExtractEDGAR_XBRLApp::CheckArgs  -----

void ExtractEDGAR_XBRLApp::BuildListOfFilesToProcess()
{
    list_of_files_to_process_.clear();      //  in case of reprocessing.

    std::ifstream input_file{list_of_files_to_process_path_};

    // Tell the stream to use our facet, so only '\n' is treated as a space.

    input_file.imbue(std::locale(input_file.getloc(), new line_only_whitespace()));

    std::copy(
        std::istream_iterator<std::string>{input_file},
        std::istream_iterator<std::string>{},
        std::back_inserter(list_of_files_to_process_)
    );
    input_file.close();

    spdlog::debug(catenate("Found: ", list_of_files_to_process_.size(), " files in list."));

    if (resume_at_this_filename_.empty())
    {
        return;
    }

    auto pos = std::find(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), resume_at_this_filename_);
    BOOST_ASSERT_MSG(pos != std::end(list_of_files_to_process_),
            ("File: " + resume_at_this_filename_ + " not found in list of files.").c_str());

    list_of_files_to_process_.erase(std::begin(list_of_files_to_process_), pos);

    spdlog::info(catenate("Resuming with: ", list_of_files_to_process_.size(), " files in list."));
}		/* -----  end of method ExtractEDGAR_XBRLApp::BuildListOfFilesToProcess  ----- */

void ExtractEDGAR_XBRLApp::BuildFilterList()
{
    //  we always ned to do this first.

    if (mode_ == "HTML"s)
    {
        filters_.push_back(FileHasHTML{});
    }
    else
    {
        filters_.push_back(FileHasXBRL{});
    }

    if (! form_.empty())
    {
        filters_.emplace_back(FileHasFormType{form_list_});
    }

    if (begin_date_ != bg::date() || end_date_ != bg::date())
    {
        filters_.emplace_back(FileIsWithinDateRange{begin_date_, end_date_});
    }

    if (! CIK_.empty())
    {
        filters_.emplace_back(FileHasCIK{CIK_list_});
    }

    if (! SIC_.empty())
    {
        filters_.emplace_back(FileHasSIC{SIC_list_});
    }
}		/* -----  end of method ExtractEDGAR_XBRLApp::BuildFilterList  ----- */

void ExtractEDGAR_XBRLApp::Run()
{
    std::tuple<int, int, int> counters;

    // for now, I know this is all we are doing.

    if (! single_file_to_process_.empty())
    {
        counters = this->LoadSingleFileToDB(single_file_to_process_);
    }

    if (! list_of_files_to_process_.empty())
    {
        if (max_at_a_time_ < 1)
        {
            counters = this->LoadFilesFromListToDB();
        }
        else
        {
            counters = this->LoadFilesFromListToDBConcurrently();
        }
    }

    if (! local_form_file_directory_.empty())
    {
        counters = this->ProcessDirectory();
    }

    auto [success_counter, skipped_counter, error_counter] = counters;

    spdlog::info(catenate("Processed: ", SumT(counters), " files. Successes: ",
            success_counter, ". Skips: ", skipped_counter , ". Errors: ", error_counter, "."));
}		/* -----  end of method ExtractEDGAR_XBRLApp::Run  ----- */

bool ExtractEDGAR_XBRLApp::ApplyFilters(const EE::SEC_Header_fields& SEC_fields, sview file_content,
        std::atomic<int>& forms_processed)
{
    bool use_file{true};
    for (const auto& filter : filters_)
    {
        use_file = use_file && filter(SEC_fields, file_content);
        if (! use_file)
        {
            break;
        }
    }
    if (use_file)
    {
        auto x = forms_processed.fetch_add(1);
        if (max_forms_to_process_ > 0 && x >= max_forms_to_process_)
        {
            throw std::range_error("Exceeded file limit: " + std::to_string(max_forms_to_process_) + '\n');
        }
    }
    return use_file;
}

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadSingleFileToDB(const fs::path& input_file_name)
{
    if (mode_ == "XBRL")
    {
        return LoadSingleFileToDB_XBRL(input_file_name);
    }
    return LoadSingleFileToDB_HTML(input_file_name);
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadSingleFileToDB  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadSingleFileToDB_XBRL(const fs::path& input_file_name)
{
    bool did_load{false};
    std::atomic<int> forms_processed{0};

    try
    {
        const std::string file_content(LoadDataFileForUse(input_file_name.string().c_str()));

        SEC_Header SEC_data;
        SEC_data.UseData(file_content);
        SEC_data.ExtractHeaderFields();
        decltype(auto) SEC_fields = SEC_data.GetFields();

        bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
        BOOST_ASSERT_MSG(use_file, "Specified file does not meet other criteria.");

        auto document_sections{LocateDocumentSections(file_content)};

        auto labels_document = LocateLabelDocument(document_sections);
        auto labels_xml = ParseXMLContent(labels_document);

        auto instance_document = LocateInstanceDocument(document_sections);
        auto instance_xml = ParseXMLContent(instance_document);

        auto filing_data = ExtractFilingData(instance_xml);
        auto gaap_data = ExtractGAAPFields(instance_xml);
        auto context_data = ExtractContextDefinitions(instance_xml);
        auto label_data = ExtractFieldLabels(labels_xml);

        if (LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_))
        {
            did_load = true;
        }
    }
    catch(const std::exception& e)
    {
        spdlog::error(catenate("Problem processing file: ", input_file_name.string(), ". ", e.what()));
        return {0, 0, 1};
    }

    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadSingleFileToDB_XBRL  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadSingleFileToDB_HTML(const fs::path& input_file_name)
{
    bool did_load{false};
    std::atomic<int> forms_processed{0};

    try
    {
        const std::string file_content(LoadDataFileForUse(input_file_name.string().c_str()));

        SEC_Header SEC_data;
        SEC_data.UseData(file_content);
        SEC_data.ExtractHeaderFields();
        decltype(auto) SEC_fields = SEC_data.GetFields();

        bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
        BOOST_ASSERT_MSG(use_file, "Specified file does not meet other criteria.");

        auto the_tables = FindAndExtractFinancialStatements(file_content);
        BOOST_ASSERT_MSG(the_tables.has_data(), ("Can't find any HTML financial tables: "
                    + input_file_name.string()).c_str());

        the_tables.CollectValues();
        BOOST_ASSERT_MSG(! the_tables.ListValues().empty(), ("Can't find any data fields in tables: "
                    + input_file_name.string()).c_str());

        did_load = true;
//        if (LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_))
//        {
//            did_load = true;
//        }
    }
    catch(const std::exception& e)
    {
        spdlog::error(catenate("Problem processing file: ", input_file_name.string(), ". ", e.what()));
        return {0, 0, 1};
    }

    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadSingleFileToDB_HTML  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadFilesFromListToDB()
{
    int success_counter{0};
    int skipped_counter{0};
    int error_counter{0};

    std::atomic<int> forms_processed{0};

    auto process_file([this, &forms_processed, &success_counter, & skipped_counter, & error_counter](const auto& file_name)
    {
        if (fs::is_regular_file(file_name))
        {
            try
            {
                if (filename_has_form_)
                {
                    if (! FormIsInFileName(form_list_, file_name))
                    {
                        ++skipped_counter;
                        return;
                    }
                }
                spdlog::debug(catenate("Scanning file: ", file_name));
                const std::string file_content(LoadDataFileForUse(file_name.c_str()));

                SEC_Header SEC_data;
                SEC_data.UseData(file_content);
                SEC_data.ExtractHeaderFields();
                decltype(auto) SEC_fields = SEC_data.GetFields();

                bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
                if (use_file)
                {
                    if (LoadFileFromFolderToDB(file_name, SEC_fields, file_content))
                    {
                        ++success_counter;
                    }
                    else
                    {
                        ++skipped_counter;
                    }
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
                spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                        " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                        ". Errors: ", error_counter, "."));
                throw;
            }
            catch(std::exception& e)
            {
                ++error_counter;
                spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                        " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                        ". Errors: ", error_counter, "."));
                spdlog::error(catenate("Problem processing file: ", file_name, ". ", e.what()));
            }
        }
    });

    std::for_each(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), process_file);

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFilesFromListToDB  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::ProcessDirectory()
{
    int success_counter{0};
    int skipped_counter{0};
    int error_counter{0};

    std::atomic<int> forms_processed{0};

    auto test_file([this, &forms_processed, &success_counter, & skipped_counter, & error_counter](const auto& dir_ent)
    {
        if (dir_ent.status().type() == fs::file_type::regular)
        {
            try
            {
                if (filename_has_form_)
                {
                    if (! FormIsInFileName(form_list_, dir_ent.path().string()))
                    {
                        ++skipped_counter;
                        return;
                    }
                }
                spdlog::debug(catenate("Scanning file: ", dir_ent.path().string()));
                const std::string file_content(LoadDataFileForUse(dir_ent.path().c_str()));

                SEC_Header SEC_data;
                SEC_data.UseData(file_content);
                SEC_data.ExtractHeaderFields();
                decltype(auto) SEC_fields = SEC_data.GetFields();

                bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
                if (use_file)
                {
                    if (LoadFileFromFolderToDB(dir_ent.path(), SEC_fields, file_content))
                    {
                        ++success_counter;
                    }
                    else
                    {
                        ++skipped_counter;
                    }
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
                spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                        " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                        ". Errors: ", error_counter, "."));
                throw;
            }
            catch(std::exception& e)
            {
                ++error_counter;
                spdlog::error(catenate("Processed: ", (success_counter + skipped_counter + error_counter) ,
                        " files. Successes: ", success_counter, ". Skips: ", skipped_counter ,
                        ". Errors: ", error_counter, "."));
                spdlog::error(catenate("Problem processing file: ", dir_ent.path().string(), ". ", e.what()));
            }
        }
    });

    std::for_each(fs::recursive_directory_iterator(local_form_file_directory_), fs::recursive_directory_iterator(), test_file);

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractEDGAR_XBRLApp::ProcessDirectory  ----- */

bool ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields,
        sview file_content)
{
    spdlog::debug(catenate("Loading contents from file: ", file_name));

    if (mode_ == "XBRL")
    {
        return LoadFileFromFolderToDB_XBRL(file_name, SEC_fields, file_content);
    }
    return LoadFileFromFolderToDB_HTML(file_name, SEC_fields, file_content);
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB  ----- */

bool ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB_XBRL(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields,
        sview file_content)
{
    auto document_sections{LocateDocumentSections(file_content)};

    auto labels_document = LocateLabelDocument(document_sections);
    auto labels_xml = ParseXMLContent(labels_document);

    auto instance_document = LocateInstanceDocument(document_sections);
    auto instance_xml = ParseXMLContent(instance_document);

    auto filing_data = ExtractFilingData(instance_xml);
    auto gaap_data = ExtractGAAPFields(instance_xml);
    auto context_data = ExtractContextDefinitions(instance_xml);
    auto label_data = ExtractFieldLabels(labels_xml);

    return LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_);
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB_XBRL  ----- */

bool ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB_HTML(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields,
        sview file_content)
{
    auto the_tables = FindAndExtractFinancialStatements(file_content);
    BOOST_ASSERT_MSG(the_tables.has_data(), ("Can't find any HTML financial tables: " + file_name).c_str());

    the_tables.CollectValues();
    BOOST_ASSERT_MSG(! the_tables.ListValues().empty(), ("Can't find any data fields in tables: " + file_name).c_str());
//    return LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger());
    return true;
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB_HTML  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadFileAsync(const std::string& file_name, std::atomic<int>& forms_processed)
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
    
    spdlog::debug(catenate("Scanning file: ", file_name));
    const std::string file_content(LoadDataFileForUse(file_name.c_str()));

    SEC_Header SEC_data; SEC_data.UseData(file_content);
    SEC_data.ExtractHeaderFields(); decltype(auto) SEC_fields = SEC_data.GetFields();

    bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
    if (use_file)
    {
        try
        {
            if (LoadFileFromFolderToDB(file_name, SEC_fields, file_content))
            {
                ++success_counter;
            }
            else
            {
                ++skipped_counter;
            }
        }
        catch(const EDGARException& e)
        {
            spdlog::error(e.what());
            ++error_counter;
        }
        catch(const pqxx::sql_error& e)
        {
            spdlog::error(catenate("Database error: ", e.what()));
            spdlog::error(catenate("Query was: ", e.query()));
            ++error_counter;
        }
        catch(const std::exception& e)
        {
            spdlog::error(e.what());
            ++error_counter;
        }
    }
    else
    {
        spdlog::debug(catenate("Skipping file: ", file_name));
        ++skipped_counter;
    }

    return {success_counter, skipped_counter, error_counter};
}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFileAsync  ----- */

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadFilesFromListToDBConcurrently()
{
    // since this code can potentially run for hours on end (depending on database throughput)
    // it's a good idea to provide a way to break into this processing and shut it down cleanly.
    // so, a little bit of C...(taken from "Advanced Unix Programming" by Warren W. Gay, p. 317)

    struct sigaction sa_old;
    struct sigaction sa_new;

    sa_new.sa_handler = ExtractEDGAR_XBRLApp::HandleSignal;
    sigemptyset(&sa_new.sa_mask);
    sa_new.sa_flags = 0;
    sigaction(SIGINT, &sa_new, &sa_old);

    // make exception handling a little bit better (I think).
    // If some kind of system error occurs, it may affect more than 1 of
    // our our tasks so let's check each of them and log any exceptions
    // which occur. We'll then rethrow our first exception.

    // ok, get ready to handle keyboard interrupts, if any.

    ExtractEDGAR_XBRLApp::had_signal_= false;

    std::exception_ptr ep{nullptr};

    // TODO: use strong types here.
    std::tuple<int, int, int> counters{0, 0, 0};  // success, skips, errors

    std::atomic<int> forms_processed{0};

    // keep track of our async processes here.

    std::vector<std::future<std::tuple<int, int, int>>> tasks;
    tasks.reserve(max_at_a_time_);

    auto do_work([this, &forms_processed](int i)
    {
        return this->LoadFileAsync(list_of_files_to_process_[i], forms_processed);
    });

    // prime the pump...

    int current_file{0};
    for ( ; tasks.size() < max_at_a_time_ && current_file < list_of_files_to_process_.size(); ++current_file)
    {
        // queue up our tasks up to the limit.

        // for some strange reason, this does not compile (but it should)
        // tasks.emplace_back(std::async(std::launch::async, &ExtractEDGAR_XBRLApp::LoadFileAsync, this,
        // list_of_files_to_process_[i], forms_processed));

        // so, use this instead.
        tasks.emplace_back(std::async(std::launch::async, do_work, current_file));
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

            // OK, let's remember our first time here.

            if (! ep)
            {
                ep = std::current_exception();
            }
            break;
        }
        catch (std::exception& e)
        {
            // any problems, we'll document them and finish any other active tasks.

            spdlog::error(e.what());
            counters = AddTs(counters, {0, 0, 1});

            // OK, let's remember our first time here.

            if (! ep)
            {
                ep = std::current_exception();
            }
            break;
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

        if (ep || ExtractEDGAR_XBRLApp::had_signal_)
        {
            break;
        }

        //  let's keep going

        if (current_file < list_of_files_to_process_.size())
        {
            tasks[ready_task] = std::async(std::launch::async, do_work, current_file);
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
        catch (EDGARException& e)
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

    if (ExtractEDGAR_XBRLApp::had_signal_)
    {
        spdlog::error(catenate("Processed: ", SumT(counters), " files. Successes: ", success_counter,
                ". Skips: ", skipped_counter, ". Errors: ", error_counter, "."));
        throw std::runtime_error("Received keyboard interrupt.  Processing manually terminated after loading: "
            + std::to_string(success_counter) + " files.");
    }

    // if we return successfully, let's just restore the default

    sigaction(SIGINT, &sa_old, 0);

    return counters;

}		/* -----  end of method ExtractEDGAR_XBRLApp::LoadFilesFromListToDBConcurrently  ----- */

void ExtractEDGAR_XBRLApp::HandleSignal(int signal)

{
    std::signal(SIGINT, ExtractEDGAR_XBRLApp::HandleSignal);

    // only thing we need to do

    ExtractEDGAR_XBRLApp::had_signal_ = true;

}		/* -----  end of method ExtractEDGAR_XBRL::HandleSignal  ----- */

void ExtractEDGAR_XBRLApp::Shutdown ()
{
    spdlog::info("\n\n*** End run "
            + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");
}       // -----  end of method ExtractEDGAR_XBRLApp::Shutdown  -----

