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

#include <cerrno>
#include <csignal>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <exception>

#include <chrono>
#include <future>
#include <thread>
#include <system_error>
// #include <parallel/algorithm>
#include <fstream>
#include <experimental/iterator>
#include <string>
// #include <streambuf>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "Poco/ConsoleChannel.h"
#include "Poco/SimpleFileChannel.h"
#include "Poco/LogStream.h"
#include "Poco/Util/OptionException.h"
#include "Poco/Util/AbstractConfiguration.h"

#include "ExtractEDGAR_XBRLApp.h"
#include "EDGAR_XML_FileFilter.h"
#include "SEC_Header.h"

using namespace std::string_literals;

// utility function

template<typename ...Ts>
auto NotAllEmpty(Ts ...ts)
{
	return ((! ts.empty()) || ...);
}

// This ctype facet does NOT classify spaces and tabs as whitespace
// from cppreference example

struct line_only_whitespace : std::ctype<char>
{
    static const mask* make_table()
    {
        // make a copy of the "C" locale table
        static std::vector<mask> v(classic_table(), classic_table() + table_size);
        v['\t'] &= ~space;		// tab will not be classified as whitespace
        v[' '] &= ~space;		// space will not be classified as whitespace
        return &v[0];
    }
    line_only_whitespace(std::size_t refs = 0) : ctype(make_table(), false, refs) {}
};

bool ExtractEDGAR_XBRLApp::had_signal_ = false;

// code from "The C++ Programming Language" 4th Edition. p. 1243.

template<typename T>
int wait_for_any(std::vector<std::future<T>>& vf, std::chrono::steady_clock::duration d)
// return index of ready future
// if no future is ready, wait for d before trying again
{
    while(true)
    {
        for (int i=0; i!=vf.size(); ++i)
        {
            if (!vf[i].valid()) continue;
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
    std::this_thread::sleep_for(d);
    }
}


ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp (int argc, char* argv[])
	: Poco::Util::Application(argc, argv)
{
}  // -----  end of method ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp  (constructor)  -----

ExtractEDGAR_XBRLApp::ExtractEDGAR_XBRLApp (void)
	: Poco::Util::Application()
{
}

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
	for (const auto it : argv())
	{
		ostr << it << ' ';
	}
	the_logger.information(ostr.str());
	the_logger.information("Arguments to main():");
    auto args = argv();
	for (const auto it : argv())
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
		for (const auto it : keys)
		{
			std::string fullKey = base;
			if (!fullKey.empty()) fullKey += '.';
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

	the_logger.information("\n\n*** Restarting run " + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");

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
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_replace_DB_content)));

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
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_single_file_to_process)));

	options.addOption(
		Poco::Util::Option("list", "", "path to file with list of files to process.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_list_of_files_to_process_path)));

	options.addOption(
		Poco::Util::Option("form", "", "name of form type we are processing. May be comma-delimited list. Default is '10-Q'.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_form)));

	options.addOption(
		Poco::Util::Option("CIK", "", "Zero-padded-on-left 10 digit CIK[s] we are processing. <CIK>,<CIK> for range of CIKS. Default is all.")
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
		Poco::Util::Option("max", "", "Maximun number of forms to process -- mainly for testing. Default of -1 means no limit.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_max)));

	options.addOption(
		Poco::Util::Option("log-level", "l", "logging level. Must be 'none|error|information|debug'. Default is 'information'.")
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
	else name = def;
	config().setString(name, value);
}

int  ExtractEDGAR_XBRLApp::main(const ArgVec& args)
{
	if (!help_requested_)
	{
        Do_Main();
	}
	return Application::EXIT_OK;
}

void ExtractEDGAR_XBRLApp::Do_Main(void)

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


void ExtractEDGAR_XBRLApp::Do_StartUp (void)
{
	logger().information("\n\n*** Begin run " + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");
}		// -----  end of method ExtractEDGAR_XBRLApp::Do_StartUp  -----

void ExtractEDGAR_XBRLApp::Do_CheckArgs (void)
{
	if (begin_date_ != bg::date())
	{
		if (end_date_ == bg::date())
			end_date_ = begin_date_;
	}

	if (begin_date_ == bg::date() && end_date_ == begin_date_)
		logger().information("Neither begin date nor end date specified. No date range filtering to be done.");

	//	the user may specify multiple form types in a comma delimited list. We need to parse the entries out
	//	of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

	if (! form_.empty())
	{
		form_list_ = split_string(form_, ',');
	}

	if (! CIK_.empty())
	{
		CIK_list_ = split_string(CIK_, ',');
		poco_assert_msg(std::all_of(CIK_list_.cbegin(), CIK_list_.cend(), [](auto e) { return e.size() == 10; }), "All CIKs must be 10 digits in length.");
	}

	if (! SIC_.empty())
	{
		SIC_list_ = split_string(SIC_, ',');
	}

	if (! single_file_to_process_.empty())
	{
		poco_assert_msg(fs::exists(single_file_to_process_), ("Can't find file: " + single_file_to_process_.string()).c_str());
        poco_assert_msg(fs::is_regular_file(single_file_to_process_), ("Path :"s + single_file_to_process_.string() + " is not a regular file.").c_str());
	}

	if (! local_form_file_directory_.empty())
	{
		poco_assert_msg(fs::exists(local_form_file_directory_), ("Can't find EDGAR file directory: " + local_form_file_directory_.string()).c_str());
        poco_assert_msg(fs::is_directory(local_form_file_directory_), ("Path :"s + local_form_file_directory_.string() + " is not a directory.").c_str());
	}

	if (! list_of_files_to_process_path_.empty())
	{
		poco_assert_msg(fs::exists(list_of_files_to_process_path_), ("Can't find file: " + list_of_files_to_process_path_.string()).c_str());
        poco_assert_msg(fs::is_regular_file(list_of_files_to_process_path_), ("Path :"s + list_of_files_to_process_path_.string()
			+ " is not a regular file.").c_str());
		BuildListOfFilesToProcess();
	}

	poco_assert_msg(NotAllEmpty(single_file_to_process_, local_form_file_directory_, list_of_files_to_process_), "No files to process found.");

	BuildFilterList();
}		// -----  end of method ExtractEDGAR_XBRLApp::Do_CheckArgs  -----

void ExtractEDGAR_XBRLApp::BuildListOfFilesToProcess(void)
{

	std::ifstream input_file{list_of_files_to_process_path_};

    // Tell the stream to use our facet, so only '\n' is treated as a space.

    input_file.imbue(std::locale(input_file.getloc(), new line_only_whitespace()));

	std::istream_iterator<std::string> itor{input_file};
    std::istream_iterator<std::string> itor_end;
	std::copy(
    	itor,
    	itor_end,
		std::back_inserter(list_of_files_to_process_)
	);
	input_file.close();

	logger().debug("Found: " + std::to_string(list_of_files_to_process_.size()) + " files in list.");
}

void ExtractEDGAR_XBRLApp::BuildFilterList(void)
{
	//	we always ned to do this first.

	filters_.push_back(FileHasXBRL{});

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

void ExtractEDGAR_XBRLApp::Do_Run (void)
{
	// for now, I know this is all we are doing.

	if (! single_file_to_process_.empty())
		this->LoadSingleFileToDB(single_file_to_process_);

	if (! list_of_files_to_process_.empty())
	{
		if (max_at_a_time_ < 1)
			this->LoadFilesFromListToDB();
		else
		{
			auto[succeesses, skips, errors] = this->LoadFilesFromListToDBConcurrently();
			poco_information(logger(), "F: Loaded to DB: " + std::to_string(succeesses) +
				". Skipped: " + std::to_string(skips) + ". Errors: " + std::to_string(errors));
		}
	}

	if (! local_form_file_directory_.empty())
		this->ProcessDirectory();

}		// -----  end of method ExtractEDGAR_XBRLApp::Do_Run  -----

void ExtractEDGAR_XBRLApp::LoadSingleFileToDB(const fs::path& input_file_name)
{
	std::string file_content(fs::file_size(input_file_name), '\0');
	std::ifstream input_file{input_file_name, std::ios_base::in | std::ios_base::binary};
	input_file.read(&file_content[0], file_content.size());
	input_file.close();

	auto document_sections{LocateDocumentSections(file_content)};

	auto labels_document = LocateLabelDocument(document_sections);
	auto labels_xml = ParseXMLContent(labels_document);

	auto instance_document = LocateInstanceDocument(document_sections);
	auto instance_xml = ParseXMLContent(instance_document);

	auto filing_data = ExtractFilingData(instance_xml);
    auto gaap_data = ExtractGAAPFields(instance_xml);
    auto context_data = ExtractContextDefinitions(instance_xml);
    auto label_data = ExtractFieldLabels(labels_xml);

	SEC_Header SEC_data;
	SEC_data.UseData(file_content);
	SEC_data.ExtractHeaderFields();
	decltype(auto) SEC_fields = SEC_data.GetFields();

	LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_);
}

void ExtractEDGAR_XBRLApp::LoadFilesFromListToDB(void)
{
    std::atomic<int> forms_processed{0};

    auto process_file([this, &forms_processed](const auto& file_name)
    {
        if (fs::is_regular_file(file_name))
        {
			try
			{
				logger().debug("Scanning file: " + file_name);
				std::string file_content(fs::file_size(file_name), '\0');
				std::ifstream input_file{file_name, std::ios_base::in | std::ios_base::binary};
				input_file.read(&file_content[0], file_content.size());
				input_file.close();

				SEC_Header SEC_data;
				SEC_data.UseData(file_content);
				SEC_data.ExtractHeaderFields();
				decltype(auto) SEC_fields = SEC_data.GetFields();

				bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
				if (use_file)
					LoadFileFromFolderToDB(file_name, SEC_fields, file_content);
			}
			catch(std::range_error& e)
			{
				throw;
			}
			catch(std::exception& e)
			{
				std::cout << "Problem processing file: " << file_name << ". " << e.what() << '\n';
			}
		}
    });

    std::for_each(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), process_file);
}
void ExtractEDGAR_XBRLApp::ProcessDirectory(void)
{
    std::atomic<int> forms_processed{0};

	int files_with_form{0};

    auto test_file([this, &files_with_form, &forms_processed](const auto& dir_ent)
    {
        if (dir_ent.status().type() == fs::file_type::regular)
        {
			logger().debug("Scanning file: " + dir_ent.path().string());
			std::string file_content(fs::file_size(dir_ent.path()), '\0');
			std::ifstream input_file{dir_ent.path(), std::ios_base::in | std::ios_base::binary};
			input_file.read(&file_content[0], file_content.size());
			input_file.close();

			SEC_Header SEC_data;
			SEC_data.UseData(file_content);
			SEC_data.ExtractHeaderFields();
			decltype(auto) SEC_fields = SEC_data.GetFields();

			bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
			if (use_file)
				LoadFileFromFolderToDB(dir_ent.path(), SEC_fields, file_content);
		}
    });

    std::for_each(fs::recursive_directory_iterator(local_form_file_directory_), fs::recursive_directory_iterator(), test_file);
}

bool ExtractEDGAR_XBRLApp::ApplyFilters(const EE::SEC_Header_fields& SEC_fields, std::string_view file_content, std::atomic<int>& forms_processed)
{
	bool use_file{true};
	for (const auto& filter : filters_)
	{
		use_file = use_file && filter(SEC_fields, file_content);
		if (! use_file)
			break;
	}
	if (use_file)
	{
	    auto x = forms_processed.fetch_add(1);
	    if (max_forms_to_process_ > 0 && x >= max_forms_to_process_)
	        throw std::range_error("Exceeded file limit: " + std::to_string(max_forms_to_process_) + '\n');
	}
	return use_file;
}

void ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, std::string_view file_content)
{

	logger().debug("Loading contents from file: " + file_name);

	auto document_sections{LocateDocumentSections(file_content)};

	auto labels_document = LocateLabelDocument(document_sections);
	auto labels_xml = ParseXMLContent(labels_document);

	auto instance_document = LocateInstanceDocument(document_sections);
	auto instance_xml = ParseXMLContent(instance_document);

	auto filing_data = ExtractFilingData(instance_xml);
    auto gaap_data = ExtractGAAPFields(instance_xml);
    auto context_data = ExtractContextDefinitions(instance_xml);
    auto label_data = ExtractFieldLabels(labels_xml);

	LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data, replace_DB_content_, &logger());
}

void ExtractEDGAR_XBRLApp::Do_Quit (void)
{
	logger().information("\n\n*** End run " + boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time()) + " ***\n");
}		// -----  end of method ExtractEDGAR_XBRLApp::Do_Quit  -----

void ExtractEDGAR_XBRLApp::comma_list_parser::parse_string (const std::string& comma_list)
{
	boost::algorithm::split(destination_, comma_list, boost::algorithm::is_any_of(seperator_));

	return ;
}		// -----  end of method comma_list_parser::parse_string  -----

void ExtractEDGAR_XBRLApp::LogLevelValidator::Validate(const Poco::Util::Option& option, const std::string& value)
{
    if (value != "error" && value != "none" && value != "information" && value != "debug")
        throw Poco::Util::OptionException("Log level must be: 'none|error|information|debug'");
}

bool ExtractEDGAR_XBRLApp::LoadFileAsync(const std::string& file_name, std::atomic<int>& forms_processed)
{
	logger().debug("Scanning file: " + file_name);
	std::string file_content(fs::file_size(file_name), '\0');
	std::ifstream input_file{file_name, std::ios_base::in | std::ios_base::binary};
	input_file.read(&file_content[0], file_content.size());
	input_file.close();

	SEC_Header SEC_data;
	SEC_data.UseData(file_content);
	SEC_data.ExtractHeaderFields();
	decltype(auto) SEC_fields = SEC_data.GetFields();

	bool use_file = this->ApplyFilters(SEC_fields, file_content, forms_processed);
	if (use_file)
		LoadFileFromFolderToDB(file_name, SEC_fields, file_content);

	return use_file;
}

std::tuple<int, int, int> ExtractEDGAR_XBRLApp::LoadFilesFromListToDBConcurrently(void)

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

    ExtractEDGAR_XBRLApp::had_signal_ = false;

    std::exception_ptr ep = nullptr;

    int success_counter{0};
    int error_counter{0};
	int skipped_counter{0};

    std::atomic<int> forms_processed{0};

    for (int i = 0; i < list_of_files_to_process_.size(); )
    {
        // keep track of our async processes here.

        std::vector<std::future<bool>> tasks;
        tasks.reserve(max_at_a_time_);

		auto do_work([this, &forms_processed](int i)
		{
			return this->LoadFileAsync(list_of_files_to_process_[i], forms_processed);
		});

        for ( ; tasks.size() < max_at_a_time_ && i < list_of_files_to_process_.size(); ++i)
        {
            // queue up our tasks up to the limit.

			// for some strange reason, this does not compile (but it should)
			// tasks.emplace_back(std::async(std::launch::async, &ExtractEDGAR_XBRLApp::LoadFileAsync, this, list_of_files_to_process_[i], forms_processed));

			// so, use this instead.
            tasks.emplace_back(std::async(std::launch::async, do_work, i));
        }

        // now, let's wait till they're all done
        // and then we'll do the next bunch.

        for (int count = tasks.size(); count; --count)
        {
            int k = wait_for_any(tasks, std::chrono::microseconds{100});
            // std::cout << "k: " << k << '\n';
            try
            {
                auto result = tasks[k].get();
				if (result)
                	++success_counter;
				else
					++skipped_counter;
            }
            catch (std::system_error& e)
            {
                // any system problems, we eventually abort, but only after finishing work in process.

                poco_error(logger(), e.what());
                auto ec = e.code();
                poco_error(logger(), std::string{"Category: "} + ec.category().name() + ". Value: " + std::to_string(ec.value()) + ". Message: "
                    + ec.message());
                ++error_counter;

                // OK, let's remember our first time here.

                if (! ep)
                    ep = std::current_exception();
                continue;
            }
            catch (std::exception& e)
            {
                // any problems, we'll document them and continue.

                poco_error(logger(), e.what());
                ++error_counter;

                // OK, let's remember our first time here.

                if (! ep)
                    ep = std::current_exception();
                continue;
            }
            catch (...)
            {
                // any problems, we'll document them and continue.

                poco_error(logger(), "Unknown problem with an async download process");
                ++error_counter;

                // OK, let's remember our first time here.

                if (! ep)
                    ep = std::current_exception();
                continue;
            }
        }

        if (ep || ExtractEDGAR_XBRLApp::had_signal_)
            break;
    }

    if (ep)
        std::rethrow_exception(ep);

    if (ExtractEDGAR_XBRLApp::had_signal_)
        throw std::runtime_error("Received keyboard interrupt.  Processing manually terminated after loading: "
			+ std::to_string(success_counter) + " files.");

    // if we return successfully, let's just restore the default

    sigaction(SIGINT, &sa_old, 0);

    return std::tuple(success_counter, skipped_counter, error_counter);

}		// -----  end of method HTTPS_Downloader::DownloadFilesConcurrently  -----


void ExtractEDGAR_XBRLApp::HandleSignal(int signal)

{
    std::signal(SIGINT, ExtractEDGAR_XBRLApp::HandleSignal);

    // only thing we need to do

    ExtractEDGAR_XBRLApp::had_signal_ = true;

}
