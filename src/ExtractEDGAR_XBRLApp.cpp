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

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <exception>
#include <experimental/iterator>
#include <fstream>
#include <fstream>
#include <future>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
// #include <parallel/algorithm>
// #include <streambuf>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "Poco/ConsoleChannel.h"
#include "Poco/LogStream.h"
#include "Poco/SimpleFileChannel.h"
#include "Poco/Util/AbstractConfiguration.h"
#include "Poco/Util/OptionException.h"

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

// let's add tuples...
// based on code techniques from C++17 STL Cookbook zipping tuples.
// (works for any class which supports the '+' operator)

template <typename ...Ts>
std::tuple<Ts...> AddTs(std::tuple<Ts...> const & t1, std::tuple<Ts...> const & t2)
{
    auto z_([] (auto ...xs)
    {
        return [xs ...] (auto ...ys)
        {
            return std::make_tuple((xs + ys) ...);
        };
    });

    return  std::apply(std::apply(z_, t1), t2);
}

// let's sum the contents of a single tuple
// (from C++ Templates...second edition p.58
// and C++17 STL Cookbook.

template <typename ...Ts>
auto SumT(const std::tuple<Ts...>& t)
{
    auto z_([] (auto ...ys)
    {
        return (... + ys);
    });
    return std::apply(z_, t);
}

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

ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp (int argc, char* argv[])
    : Poco::Util::Application(argc, argv)
{
}  // -----  end of method ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp  (constructor)  -----

void ExtractEDGAR_XBRLApp::initialize(Application& self)
{
    loadConfiguration(); // load default configuration files, if present
    Application::initialize(self);
    // add your own initialization code here

    if (! log_file_path_name_.empty())
    {
        logger_file_ = new Poco::SimpleFileChannel;
        logger_file_->setProperty("path", log_file_path_name_.string());
        logger_file_->setProperty("rotation", "2 M");
    }
    else
    {
        logger_file_ = new Poco::ConsoleChannel;
    }

    decltype(auto) the_logger = Poco::Logger::root();
    the_logger.setChannel(logger_file_);
    the_logger.setLevel(logging_level_);

    setLogger(the_logger);

    the_logger.information("Command line:");
    std::ostringstream ostr;
    for (const auto& it : argv())
    {
        ostr << it << ' ';
    }
    the_logger.information(ostr.str());
    the_logger.information("Arguments to main():");
    auto args = argv();
    for (const auto& it : argv())
    {
        the_logger.information(it);
    }
    the_logger.information("Application properties:");
    printProperties("");

    // set log level here since options will have been parsed before we get here.
}

void ExtractEDGAR_XBRLApp::printProperties(const std::string& base)
{
    Poco::Util::AbstractConfiguration::Keys keys;
    config().keys(base, keys);
    if (keys.empty())
    {
        if (config().hasProperty(base))
        {
            std::string msg;
            msg.append(base);
            msg.append(" = ");
            msg.append(config().getString(base));
            logger().information(msg);
        }
    }
    else
    {
        for (const auto& it : keys)
        {
            std::string fullKey = base;
            if (!fullKey.empty())
            {
                fullKey += '.';
            }
            fullKey.append(it);
            printProperties(fullKey);
        }
    }
}

void  ExtractEDGAR_XBRLApp::uninitialize()
{
    // add your own uninitialization code here
    Application::uninitialize();
}

void  ExtractEDGAR_XBRLApp::reinitialize(Application& self)
{
    Application::reinitialize(self);
    // add your own reinitialization code here

    if (! log_file_path_name_.empty())
    {
        logger_file_ = new Poco::SimpleFileChannel;
        logger_file_->setProperty("path", log_file_path_name_.string());
        logger_file_->setProperty("rotation", "2 M");
    }
    else
    {
        logger_file_ = new Poco::ConsoleChannel;
    }

    decltype(auto) the_logger = Poco::Logger::root();
    the_logger.setChannel(logger_file_);

    the_logger.information("\n\n*** Restarting run " +
            boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");

    setLogger(the_logger);
}

void  ExtractEDGAR_XBRLApp::defineOptions(Poco::Util::OptionSet& options)
{
    Application::defineOptions(options);

    options.addOption(
        Poco::Util::Option("help", "h", "display help information on command line arguments")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::handleHelp)));

    options.addOption(
        Poco::Util::Option("gtest_filter", "", "select which tests to run.")
            .required(false)
            .repeatable(true)
            .argument("name=value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::handleDefine)));

    options.addOption(
        Poco::Util::Option("begin-date", "", "retrieve files with dates greater than or equal to.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_begin_date)));

    options.addOption(
        Poco::Util::Option("end-date", "", "retrieve files with dates less than or equal to.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_end_date)));

    options.addOption(
        Poco::Util::Option("replace-DB-content", "R", "replace all DB content for each file. Default is 'false'")
            .required(false)
            .repeatable(false)
            .noArgument()
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this,
                    &ExtractEDGAR_XBRLApp::store_replace_DB_content)));

    options.addOption(
        Poco::Util::Option("form-dir", "", "directory of form files to be processed.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_form_dir)));

    options.addOption(
        Poco::Util::Option("file", "f", "single file to be processed.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this,
                    &ExtractEDGAR_XBRLApp::store_single_file_to_process)));

    options.addOption(
        Poco::Util::Option("mode", "Must be either 'HTML' or 'XBRL'.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_mode)));

    options.addOption(
        Poco::Util::Option("list", "", "path to file with list of files to process.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this,
                    &ExtractEDGAR_XBRLApp::store_list_of_files_to_process_path)));

    options.addOption(
        Poco::Util::Option("form", "", "name of form type we are processing. May be comma-delimited list. Default is '10-Q'.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_form)));

    options.addOption(
        Poco::Util::Option("CIK", "",
            "Zero-padded-on-left 10 digit CIK[s] we are processing. <CIK>,<CIK> for range of CIKS. Default is all.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_CIK)));

    options.addOption(
        Poco::Util::Option("SIC", "", "SIC we are processing. May be comma-delimited list. Default is all.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_SIC)));

    options.addOption(
        Poco::Util::Option("log-path", "", "path name for log file.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_log_path)));

    options.addOption(
        Poco::Util::Option("max", "",
            "Maximun number of forms to process -- mainly for testing. Default of -1 means no limit.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_max)));

    options.addOption(
        Poco::Util::Option("log-level", "l",
            "logging level. Must be 'none|error|information|debug'. Default is 'information'.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_log_level)));

    options.addOption(
        Poco::Util::Option("concurrent", "k", "Maximun number of concurrent processes. Default of -1 -- system defined.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_concurrency_limit)));

    options.addOption(
        Poco::Util::Option("filename-has-form", "", "form number is in file path. Default is 'false'")
            .required(false)
            .repeatable(false)
            .noArgument()
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_filename_has_form)));

    options.addOption(
        Poco::Util::Option("resume-at", "", "find this file name in list and resume processing there.")
            .required(false)
            .repeatable(false)
            .argument("value")
            .callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this,
                    &ExtractEDGAR_XBRLApp::store_resume_at_filename)));

}

void  ExtractEDGAR_XBRLApp::handleHelp(const std::string& name, const std::string& value)
{
    help_requested_ = true;
    displayHelp();
    stopOptionsProcessing();
}

void  ExtractEDGAR_XBRLApp::handleDefine(const std::string& name, const std::string& value)
{
    defineProperty(value);
}

void  ExtractEDGAR_XBRLApp::handleConfig(const std::string& name, const std::string& value)
{
    loadConfiguration(value);
}

void ExtractEDGAR_XBRLApp::displayHelp()
{
    Poco::Util::HelpFormatter helpFormatter(options());
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Program which manages the download of selected files from the SEC's EDGAR FTP server.");
    helpFormatter.format(std::cout);
}

void  ExtractEDGAR_XBRLApp::defineProperty(const std::string& def)
{
    std::string name;
    std::string value;
    std::string::size_type pos = def.find('=');
    if (pos != std::string::npos)
    {
        name.assign(def, 0, pos);
        value.assign(def, pos + 1, def.length() - pos);
    }
    else
    {
        name = def;
    }
    config().setString(name, value);
}

int  ExtractEDGAR_XBRLApp::main(const ArgVec& args)
{
    if (!help_requested_)
    {
#ifdef _DEBUG
        Do_Test();
#else
        Do_Main();
#endif
    }
    return Application::EXIT_OK;
}

void ExtractEDGAR_XBRLApp::Do_Test()
{
    // we let exceptions propagate out so testing framework
    // can see them.

    Do_StartUp();
    Do_CheckArgs();
    Do_Run();
    Do_Quit();
}

void ExtractEDGAR_XBRLApp::Do_Main()
{
    Do_StartUp();
    try
    {
        Do_CheckArgs();
        Do_Run();
    }
    catch (Poco::AssertionViolationException& e)
    {
        std::cout << e.displayText() << '\n';
    }
    catch (std::system_error& e)
    {
        // any system problems, we eventually abort, but only after finishing work in process.

        poco_error(logger(), e.what());
        auto ec = e.code();
        poco_error(logger(), std::string{"Category: "} + ec.category().name() + ". Value: "
                + std::to_string(ec.value()) + ". Message: " + ec.message());
    }
    catch (std::exception& e)
    {
        std::cout << "Problem collecting files: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cout << "Unknown problem collecting files." << '\n';
    }

    Do_Quit();
}


void ExtractEDGAR_XBRLApp::Do_StartUp ()
{
    logger().information("\n\n*** Begin run "
           + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");
}       // -----  end of method ExtractEDGAR_XBRLApp::Do_StartUp  -----

void ExtractEDGAR_XBRLApp::Do_CheckArgs ()
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
        logger().information("Neither begin date nor end date specified. No date range filtering to be done.");
    }

    //  the user may specify multiple form types in a comma delimited list. We need to parse the entries out
    //  of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

    if (! mode_.empty())
    {
        poco_assert_msg(mode_ == "HTML"s || mode_ == "XBRL"s, "Mode must be: 'HTML' or 'XBRL'.");
    }

    if (! form_.empty())
    {
        form_list_ = split_string(form_, ',');
    }

    if (! CIK_.empty())
    {
        CIK_list_ = split_string(CIK_, ',');
        poco_assert_msg(std::all_of(CIK_list_.cbegin(), CIK_list_.cend(), [](auto e) { return e.size() == 10; }),
                "All CIKs must be 10 digits in length.");
    }

    if (! SIC_.empty())
    {
        SIC_list_ = split_string(SIC_, ',');
    }

    if (! single_file_to_process_.empty())
    {
        poco_assert_msg(fs::exists(single_file_to_process_), ("Can't find file: " + single_file_to_process_.string()).c_str());
        poco_assert_msg(fs::is_regular_file(single_file_to_process_), ("Path :"s + single_file_to_process_.string()
                    + " is not a regular file.").c_str());
    }

    if (! local_form_file_directory_.empty())
    {
        poco_assert_msg(fs::exists(local_form_file_directory_), ("Can't find EDGAR file directory: "
                    + local_form_file_directory_.string()).c_str());
        poco_assert_msg(fs::is_directory(local_form_file_directory_),
                ("Path :"s + local_form_file_directory_.string() + " is not a directory.").c_str());
    }
    
    if (! resume_at_this_filename_.empty())
    {
        poco_assert_msg(list_of_files_to_process_.empty(),
                "You must provide a list of files to process when specifying file to resume at.");
    }

    if (! list_of_files_to_process_path_.empty())
    {
        poco_assert_msg(fs::exists(list_of_files_to_process_path_),
                ("Can't find file: " + list_of_files_to_process_path_.string()).c_str());
        poco_assert_msg(fs::is_regular_file(list_of_files_to_process_path_),
                ("Path :"s + list_of_files_to_process_path_.string() + " is not a regular file.").c_str());
        BuildListOfFilesToProcess();
    }

    poco_assert_msg(NotAllEmpty(single_file_to_process_, local_form_file_directory_, list_of_files_to_process_),
            "No files to process found.");

    BuildFilterList();
}       // -----  end of method ExtractEDGAR_XBRLApp::Do_CheckArgs  -----

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

    logger().debug("Found: " + std::to_string(list_of_files_to_process_.size()) + " files in list.");

    if (resume_at_this_filename_.empty())
    {
        return;
    }

    auto pos = std::find(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), resume_at_this_filename_);
    if (pos != std::end(list_of_files_to_process_))
    {
        list_of_files_to_process_.erase(std::begin(list_of_files_to_process_), pos);
    }
    else
    {
        throw std::range_error("File: " + resume_at_this_filename_ + " not found in list of files.");
    }

    logger().information("Resuming with: " + std::to_string(list_of_files_to_process_.size()) + " files in list.");
}

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
}

void ExtractEDGAR_XBRLApp::Do_Run ()
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

    poco_error(logger(), "Processed: "s + std::to_string(SumT(counters)) + " files. Successes: "
            + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
            + ". Errors: " + std::to_string(error_counter) + ".");
}       // -----  end of method ExtractEDGAR_XBRLApp::Do_Run  -----

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadSingleFileToDB(const fs::path& input_file_name)
{
    if (mode_ == "XBRL")
    {
        return LoadSingleFileToDB_XBRL(input_file_name);
    }
    return LoadSingleFileToDB_HTML(input_file_name);
}

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
        poco_assert_msg(use_file, "Specified file does not meet other criteria.");

        auto document_sections{LocateDocumentSections(file_content)};

        auto labels_document = LocateLabelDocument(document_sections);
        auto labels_xml = ParseXMLContent(labels_document);

        auto instance_document = LocateInstanceDocument(document_sections);
        auto instance_xml = ParseXMLContent(instance_document);

        auto filing_data = ExtractFilingData(instance_xml);
        auto gaap_data = ExtractGAAPFields(instance_xml);
        auto context_data = ExtractContextDefinitions(instance_xml);
        auto label_data = ExtractFieldLabels(labels_xml);

        if (LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger()))
        {
            did_load = true;
        }
    }
    catch(const std::exception& e)
    {
        poco_error(logger(), "Problem processing file: " + input_file_name.string() + ". " + e.what());
        return {0, 0, 1};
    }

    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}

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
        poco_assert_msg(use_file, "Specified file does not meet other criteria.");

        auto documents = LocateDocumentSections(file_content);
        auto all_anchors = FindAllDocumentAnchors(documents);
        auto statement_anchors = FilterFinancialAnchors(all_anchors);
        auto destination_anchors = FindAnchorDestinations(statement_anchors, all_anchors);
        auto multipliers = FindDollarMultipliers(destination_anchors);
        auto financial_tables = LocateFinancialTables(multipliers);

        FinancialStatements the_tables;
        the_tables.balance_sheet_ = ExtractBalanceSheet(financial_tables);
        the_tables.statement_of_operations_ = ExtractStatementOfOperations(financial_tables);
        the_tables.cash_flows_ = ExtractCashFlowStatement(financial_tables);
        the_tables.stockholders_equity_ = ExtractStatementOfStockholdersEquity(financial_tables);

        poco_assert_msg(the_tables.is_complete(), ("Can't find all HTML financial tables: "
                    + input_file_name.string()).c_str());

//        if (LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger()))
//        {
//            did_load = true;
//        }
    }
    catch(const std::exception& e)
    {
        poco_error(logger(), "Problem processing file: " + input_file_name.string() + ". " + e.what());
        return {0, 0, 1};
    }

    if (did_load)
    {
        return {1, 0, 0};
    }
    return {0, 1, 0};
}
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
                logger().debug("Scanning file: " + file_name);
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
            catch(std::range_error& e)
            {
                ++error_counter;
                poco_error(logger(), "Processed: "s + std::to_string(success_counter + skipped_counter + error_counter)
                    + " files. Successes: " + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
                    + ". Errors: " + std::to_string(error_counter) + ".");
                throw;
            }
            catch(std::exception& e)
            {
                ++error_counter;
                poco_error(logger(), "Processed: "s + std::to_string(success_counter + skipped_counter + error_counter)
                    + " files. Successes: " + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
                    + ". Errors: " + std::to_string(error_counter) + ".");
                poco_error(logger(), "Problem processing file: " + file_name + ". " + e.what());
            }
        }
    });

    std::for_each(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), process_file);

    return {success_counter, skipped_counter, error_counter};
}

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
                logger().debug("Scanning file: " + dir_ent.path().string());
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
            catch(std::range_error& e)
            {
                ++error_counter;
                poco_error(logger(), "Processed: "s + std::to_string(success_counter + skipped_counter + error_counter)
                    + " files. Successes: " + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
                    + ". Errors: " + std::to_string(error_counter) + ".");
                throw;
            }
            catch(std::exception& e)
            {
                ++error_counter;
                poco_error(logger(), "Processed: "s + std::to_string(success_counter + skipped_counter + error_counter)
                    + " files. Successes: " + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
                    + ". Errors: " + std::to_string(error_counter) + ".");
                poco_error(logger(), "Problem processing file: " + dir_ent.path().string() + ". " + e.what());
            }
        }
    });

    std::for_each(fs::recursive_directory_iterator(local_form_file_directory_), fs::recursive_directory_iterator(), test_file);

    return {success_counter, skipped_counter, error_counter};
}

bool ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields,
        sview file_content)
{
    logger().debug("Loading contents from file: " + file_name);

    if (mode_ == "XBRL")
    {
        return LoadFileFromFolderToDB_XBRL(file_name, SEC_fields, file_content);
    }
    return LoadFileFromFolderToDB_HTML(file_name, SEC_fields, file_content);
}

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

    return LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger());
}

bool ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB_HTML(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields,
        sview file_content)
{
    auto documents = LocateDocumentSections(file_content);
    auto all_anchors = FindAllDocumentAnchors(documents);
    auto statement_anchors = FilterFinancialAnchors(all_anchors);
    auto destination_anchors = FindAnchorDestinations(statement_anchors, all_anchors);
    auto multipliers = FindDollarMultipliers(destination_anchors);
    auto financial_tables = LocateFinancialTables(multipliers);

    FinancialStatements the_tables;
    the_tables.balance_sheet_ = ExtractBalanceSheet(financial_tables);
    the_tables.statement_of_operations_ = ExtractStatementOfOperations(financial_tables);
    the_tables.cash_flows_ = ExtractCashFlowStatement(financial_tables);
    the_tables.stockholders_equity_ = ExtractStatementOfStockholdersEquity(financial_tables);

    poco_assert_msg(the_tables.is_complete(), ("Can't find all HTML financial tables." + file_name).c_str());

//    return LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger());
    return the_tables.is_complete();
}

void ExtractEDGAR_XBRLApp::Do_Quit ()
{
    logger().information("\n\n*** End run "
            + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");
}       // -----  end of method ExtractEDGAR_XBRLApp::Do_Quit  -----

void ExtractEDGAR_XBRLApp::LogLevelValidator::validate(const Poco::Util::Option& option, const std::string& value)
{
    if (value != "error" && value != "none" && value != "information" && value != "debug")
    {
        throw Poco::Util::OptionException("Log level must be: 'none|error|information|debug'");
    }
}

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
    
    logger().debug("Scanning file: " + file_name);
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
        catch(const ExtractException& e)
        {
            poco_error(logger(), e.what());
            ++error_counter;
        }
        catch(const pqxx::sql_error& e)
        {
            poco_error(logger(), "Database error: "s + e.what());
            poco_error(logger(), "Query was: " + e.query());
            ++error_counter;
        }
    }
    else
    {
        logger().debug("Skipping file: " + file_name);
        ++skipped_counter;
    }

    return {success_counter, skipped_counter, error_counter};
}


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
    // If some kind of system error occurs, it may affect more than 1
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

            poco_error(logger(), e.what());
            auto ec = e.code();
            poco_error(logger(), "Category: "s + ec.category().name() + ". Value: " + std::to_string(ec.value())
                   + ". Message: " + ec.message());
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

            poco_error(logger(), e.what());
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

            poco_error(logger(), "Unknown problem with async file processing");
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
        catch (ExtractException& e)
        {
            // any problems, we'll document them and continue.

            poco_error(logger(), e.what());
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
        poco_error(logger(), "Processed: "s + std::to_string(SumT(counters)) + " files. Successes: "
                + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
                + ". Errors: " + std::to_string(error_counter) + ".");
        std::rethrow_exception(ep);
    }

    if (ExtractEDGAR_XBRLApp::had_signal_)
    {
        poco_error(logger(), "Processed: "s + std::to_string(SumT(counters)) + " files. Successes: "
               + std::to_string(success_counter) + ". Skips: " + std::to_string(skipped_counter)
               + ". Errors: " + std::to_string(error_counter) + ".");
        throw std::runtime_error("Received keyboard interrupt.  Processing manually terminated after loading: "
            + std::to_string(success_counter) + " files.");
    }

    // if we return successfully, let's just restore the default

    sigaction(SIGINT, &sa_old, 0);

    return counters;

}       // -----  end of method HTTPS_Downloader::DownloadFilesConcurrently  -----

void ExtractEDGAR_XBRLApp::HandleSignal(int signal)

{
    std::signal(SIGINT, ExtractEDGAR_XBRLApp::HandleSignal);

    // only thing we need to do

    ExtractEDGAR_XBRLApp::had_signal_ = true;

}
