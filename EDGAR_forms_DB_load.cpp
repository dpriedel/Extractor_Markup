#include <execution>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using namespace std::string_literals;

#include "Extractor.h"
#include "Extractor_Utils.h"
#include "Extractors.h"

void SetupProgramOptions();
void ParseProgramOptions(int argc, const char *argv[]);
void CheckArgs();
std::vector<std::string> MakeListOfFilesToProcess(EM::FileName input_directory, EM::FileName file_list,
                                                  bool file_name_has_form, const std::string &form_type);

argparse::ArgumentParser app("program_name");

struct Options
{
    std::vector<std::string> forms_;
    EM::FileName input_directory_;
    EM::FileName output_directory_;
    EM::FileName file_list_;
    EM::FileName log_file_path_name_;
    std::string logging_level_{"information"};
    std::string DB_mode_{"test"};
    int max_files_{-1};
    bool replace_DB_content_{false};
    bool file_name_has_form_{false};
} program_options;

void SetupProgramOptions(Options &program_options)
{

    // Forms option with custom parsing for comma-separated values
    app.add_argument("-f", "--forms")
        .help("Name of form type[s] we are processing. Process all if not specified.")
        .nargs(argparse::nargs_pattern::any)
        .action([&program_options](const std::string &str) {
            std::stringstream ss(str);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                program_options.forms_.push_back(item);
            }
        });

    app.add_argument("--list-file").help("Path to file with list of files to process.").required();

    app.add_argument("--max-files")
        .help("Maximum number of files to extract. Default of -1 means no limit.")
        .nargs(1)
        .scan<'i', int>();

    app.add_argument("--log-path")
        .help("path name for log file.")
        .nargs(1)
        .scan<'s', std::string>(); // scan for string type

    app.add_argument("-l", "--log-level")
        .help("logging level. Must be 'none|error|information|debug'. Default is 'information'.")
        .default_value(std::string("information"))
        .nargs(1);

    app.add_argument("-R", "--replace-DB-content")
        .help("replace all DB content for each file. Default is 'false'")
        .default_value(false)
        .implicit_value(true)
        .nargs(0);

    app.add_argument("--DB-mode")
        .help("Must be either 'test' or 'live'. Default is 'test'.")
        .default_value(std::string("test"))
        .nargs(1);
}
void ConfigureLogging(Options &program_options)
{
    // // this logging code comes from gemini
    //
    // if (!program_options.log_file_path_name_.empty())
    // {
    //     fs::path log_dir = log_file_path_name_i_.parent_path();
    //     if (!fs::exists(log_dir))
    //     {
    //         fs::create_directories(log_dir);
    //     }
    //
    //     auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path_name_i_, true);
    //
    //     auto app_logger = std::make_shared<spdlog::logger>("Extractor_logger", file_sink);
    //
    //     spdlog::set_default_logger(app_logger);
    // }
    // else
    // {
    //     auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    //
    //     // 3. Create an asynchronous logger using the console sink.
    //     auto app_logger = std::make_shared<spdlog::logger>("Extractor_logger", // Name for the console logger
    //                                                        console_sink);
    //
    //     spdlog::set_default_logger(app_logger);
    // }
    //
    // // we are running before 'CheckArgs' so we need to do a little editiing
    // // ourselves.
    //
    // const std::map<std::string, spdlog::level::level_enum> levels{{"none", spdlog::level::off},
    //                                                               {"error", spdlog::level::err},
    //                                                               {"information", spdlog::level::info},
    //                                                               {"debug", spdlog::level::debug}};
    //
    // auto which_level = levels.find(logging_level_);
    // if (which_level != levels.end())
    // {
    //     spdlog::set_level(which_level->second);
    // }
    // else
    // {
    //     spdlog::set_level(spdlog::level::info);
    // }
}

void ParseProgramOptions(int argc, const char *argv[])
{
    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        std::cerr << e.what() << std::endl;
        std::exit(1);
    }
}

void CheckArgs(Options &program_options)
{
    if (!input_directory.get().empty() && !fs::exists(input_directory.get()))
    {
        throw std::runtime_error("Input directory is missing.\n");
    }

    if (!file_list.get().empty() && !fs::exists(file_list.get()))
    {
        throw std::runtime_error("List file is missing.\n");
    }

    if (input_directory.get().empty() && file_list.get().empty())
    {
        throw std::runtime_error("You must specify either a directory to process or a list of files to process.");
    }

    if (!input_directory.get().empty() && !file_list.get().empty())
    {
        throw std::runtime_error(
            "You must specify EITHER a directory to process or a list of files to process -- not both.");
    }

    if (fs::exists(output_directory.get()))
    {
        fs::remove_all(output_directory.get());
    }
    fs::create_directories(output_directory.get());
}

// This ctype facet does NOT classify spaces and tabs as whitespace
// from cppreference example

struct line_only_whitespace : std::ctype<char>
{
    static const mask *make_table()
    {
        // make a copy of the "C" locale table
        static std::vector<mask> v(classic_table(), classic_table() + table_size);
        v['\t'] &= ~space; // tab will not be classified as whitespace
        v[' '] &= ~space;  // space will not be classified as whitespace
        return &v[0];
    }
    explicit line_only_whitespace(std::size_t refs = 0) : ctype(make_table(), false, refs)
    {
    }
};

int main(int argc, const char *argv[])
{
    spdlog::set_level(spdlog::level::debug);

    const std::ctype<char> &ct(std::use_facet<std::ctype<char>>(std::locale()));

    for (size_t i(0); i != 256; ++i)
    {
        ct.narrow(static_cast<char>(i), '\0');
    }

    auto result{0};

    try
    {
        SetupProgramOptions(program_options);
        ParseProgramOptions(argc, argv);
        CheckArgs(program_options);

        auto the_filters = SelectExtractors(app);

        std::atomic<int> files_processed{0};

        auto files_to_scan = MakeListOfFilesToProcess(input_directory, file_list, file_name_has_form, form_type);

        std::cout << "Found: " << files_to_scan.size() << " files to process.\n";

        auto scan_file([&the_filters, &files_processed](const auto &input_file_name) {
            spdlog::info(catenate("Processing file: ", input_file_name));
            const std::string file_content_text = LoadDataFileForUse(EM::FileName{input_file_name});
            EM::FileContent file_content(file_content_text);

            try
            {
                auto use_file = FilterFiles(file_content, form_type, MAX_FILES, files_processed);

                if (use_file)
                {
                    for (auto &e : the_filters)
                    {
                        try
                        {
                            std::visit(
                                [&input_file_name, file_content, &use_file](auto &&x) {
                                    x.UseExtractor(EM::FileName{input_file_name}, file_content, output_directory,
                                                   use_file.value());
                                },
                                e);
                        }
                        catch (std::exception &ex)
                        {
                            std::cerr << ex.what() << '\n';
                        }
                    }
                }
            }
            catch (std::exception &ex)
            {
                std::cerr << ex.what() << '\n';
            }
        });

        std::for_each(std::execution::seq, std::begin(files_to_scan), std::end(files_to_scan), scan_file);

        for (const auto &e : the_filters)
        {
            if (auto f = std::get_if<Count_XLS>(&e))
            {
                std::cout << "Found: " << f->XLS_counter << " spread sheets.\n";
            }
        }
    }
    catch (std::exception &e)
    {
        std::cout << e.what();
        result = 1;
    }
    return result;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  MakeListOfFilesToProcess
 *  Description:
 * =====================================================================================
 */
std::vector<std::string> MakeListOfFilesToProcess(EM::FileName input_directory, EM::FileName file_list,
                                                  bool file_name_has_form, const std::string &form_type)
{
    std::vector<std::string> list_of_files_to_process;

    if (!input_directory.get().empty())
    {
        auto add_file_to_list([&list_of_files_to_process, file_name_has_form, form_type](const auto &dir_ent) {
            if (dir_ent.status().type() == fs::file_type::regular)
            {
                if (file_name_has_form)
                {
                    if (dir_ent.path().string().find("/"s + form_type + "/"s) != std::string::npos)
                    {
                        list_of_files_to_process.emplace_back(dir_ent.path().string());
                    }
                }
                else
                {
                    list_of_files_to_process.emplace_back(dir_ent.path().string());
                }
            }
        });

        std::for_each(fs::recursive_directory_iterator(input_directory.get()), fs::recursive_directory_iterator(),
                      add_file_to_list);
    }
    else
    {
        std::ifstream input_file{file_list.get()};

        // Tell the stream to use our facet, so only '\n' is treated as a space.

        input_file.imbue(std::locale(input_file.getloc(), new line_only_whitespace()));

        auto use_this_file([&list_of_files_to_process, file_name_has_form, form_type](const auto &file_name) {
            if (file_name_has_form)
            {
                if (file_name.find("/"s + form_type + "/"s) != std::string::npos)
                {
                    return true;
                }
                return false;
            }
            return true;
        });

        std::copy_if(std::istream_iterator<std::string>{input_file},
                     std::istream_iterator<std::string>{},
                     std::back_inserter(list_of_files_to_process),
                     use_this_file);
        input_file.close();
    }

    return list_of_files_to_process;
} /* -----  end of function MakeListOfFilesToProcess  ----- */
