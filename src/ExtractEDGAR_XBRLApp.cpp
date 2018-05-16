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

#include <parallel/algorithm>
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
		Poco::Util::Option("list", "l", "path to file with list of files to process.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_list_of_files_to_process_path)));

	options.addOption(
		Poco::Util::Option("form", "", "name of form type[s] we are processing. Default is '10-Q'.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_form)));

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
		comma_list_parser x(form_list_, ",");
		x.parse_string(form_);
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
		filters_.emplace_back(FileHasFormType{form_});
	}

	if (begin_date_ != bg::date() || end_date_ != bg::date())
	{
		filters_.emplace_back(FileIsWithinDateRange{begin_date_, end_date_});
	}
}

void ExtractEDGAR_XBRLApp::Do_Run (void)
{
	// for now, I know this is all we are doing.

	if (! single_file_to_process_.empty())
		this->LoadSingleFileToDB(single_file_to_process_);

	if (! list_of_files_to_process_.empty())
		this->LoadFilesFromListToDB();

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
    auto process_file([this](const auto& file_name)
    {
        if (fs::is_regular_file(file_name))
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

			bool use_file = this->ApplyFilters(SEC_fields, file_content);
			if (use_file)
				LoadFileFromFolderToDB(file_name, SEC_fields, file_content);
		}
    });

    __gnu_parallel::for_each(std::begin(list_of_files_to_process_), std::end(list_of_files_to_process_), process_file);
}

void ExtractEDGAR_XBRLApp::ProcessDirectory(void)
{

	int files_with_form{0};

    auto test_file([this, &files_with_form](const auto& dir_ent)
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

			bool use_file = this->ApplyFilters(SEC_fields, file_content);
			if (use_file)
				LoadFileFromFolderToDB(dir_ent.path(), SEC_fields, file_content);
		}
    });

    std::for_each(fs::recursive_directory_iterator(local_form_file_directory_), fs::recursive_directory_iterator(), test_file);
}

bool ExtractEDGAR_XBRLApp::ApplyFilters(const EE::SEC_Header_fields& SEC_fields, std::string_view file_content)
{
	bool use_file{true};
	for (const auto& filter : filters_)
	{
		use_file = use_file && filter(SEC_fields, file_content);
		if (! use_file)
			break;
	}
	return use_file;
}

void ExtractEDGAR_XBRLApp::LoadFileFromFolderToDB(const std::string& file_name, const EE::SEC_Header_fields& SEC_fields, std::string_view file_content)
{

	logger().debug("Loading contents from file: " + file_name.string());

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
