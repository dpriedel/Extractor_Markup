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

#include <fstream>
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
		Poco::Util::Option("pause", "", "how many seconds to wait between downloads. Default: 1 second.")
			.required(false)
			.repeatable(false)
			.argument("value")
			.callback(Poco::Util::OptionCallback<ExtractEDGAR_XBRLApp>(this, &ExtractEDGAR_XBRLApp::store_pause)));

	options.addOption(
		Poco::Util::Option("max", "", "Maximun number of forms to download -- mainly for testing. Default of -1 means no limit.")
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
		Poco::Util::Option("concurrent", "k", "Maximun number of concurrent downloads. Default of 10.")
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
	poco_assert_msg(mode_ == "daily" || mode_ == "quarterly" || mode_ == "ticker-only", ("Mode must be either 'daily','quarterly' or 'ticker-only' ==> " + mode_).c_str());

	//	the user may specify multiple stock tickers in a comma delimited list. We need to parse the entries out
	//	of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

	poco_assert_msg(begin_date_ != bg::date(), "Must specify 'begin-date' for index and/or form downloads.");

	if (end_date_ == bg::date())
		end_date_ = begin_date_;

	//	the user may specify multiple form types in a comma delimited list. We need to parse the entries out
	//	of that list and place into ultimate home.  If just a single entry, copy it to our form list destination too.

	if (! form_.empty())
	{
		comma_list_parser x(form_list_, ",");
		x.parse_string(form_);
	}

}		// -----  end of method ExtractEDGAR_XBRLApp::Do_CheckArgs  -----

void ExtractEDGAR_XBRLApp::Do_Run (void)
{
	// for now, I know this is all we are doing.

	this->LoadSingleFileToDB(single_file_to_process_);

}		// -----  end of method ExtractEDGAR_XBRLApp::Do_Run  -----

void ExtractEDGAR_XBRLApp::LoadSingleFileToDB(const fs::path& input_file_name)
{
    std::ifstream input_file{input_file_name};
    const std::string file_content{std::istreambuf_iterator<char>{input_file}, std::istreambuf_iterator<char>{}};
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
	auto SEC_fields = SEC_data.GetFields();

	LoadDataToDB(SEC_fields, filing_data, gaap_data, label_data, context_data);
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
