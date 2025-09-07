#include <filesystem>
#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using namespace std::string_literals;

#include "Extractor.h"

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

void SetupProgramOptions(Options &program_options);
void ParseProgramOptions(int argc, const char *argv[]);
void CheckArgs();
std::vector<std::string> MakeListOfFilesToProcess(EM::FileName input_directory, EM::FileName file_list,
                                                  bool file_name_has_form, const std::string &form_type);

CLI::App app("EDGAR_forms_DB_load");

void SetupProgramOptions(Options &program_options)
{

    auto form_option = app.add_option("-f,--form", program_options.forms_,
                                      "Name of form type[s] we are processing. Process all if not specified.");

    // 1. Handle comma-delimited lists:
    // This modifier tells CLI11 to split the argument string by commas.
    // e.g., --form A,B,C will result in three separate entries.
    form_option->delimiter(',');

    form_option->transform([](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return ::toupper(c); });
        return s;
    });

    // --- New options for input directory and file list ---

    // 1. Create an option group to enforce mutual exclusion.
    auto input_source_group =
        app.add_option_group("InputSource", "Specify the source of files to process. Only 1 can be specified.");

    input_source_group
        ->add_option("-i,--input-dir", program_options.input_directory_,
                     "Path to the directory of forms to be processed.")
        ->check(CLI::ExistingDirectory) // Validator: Checks if path exists and is a directory.
        ->check([](const std::string &path_str) {
            // The path is guaranteed to exist and be a directory at this point.
            if (std::filesystem::is_empty(path_str))
            {
                return "Input directory: '" + path_str + "' contains no files.";
            }
            return std::string{}; // Return an empty string for success
        });

    input_source_group
        ->add_option("--file-list", program_options.file_list_,
                     "Path to a file containing a list of files to process.")
        ->check(CLI::ExistingFile) // Validator: Checks if path exists and is a regular file.
        ->check([](const std::string &path_str) {
            // The path is guaranteed to exist and be a file.
            std::error_code ec;
            const auto size = std::filesystem::file_size(path_str, ec);

            // It's good practice to check for errors when querying the filesystem.
            if (ec)
            {
                return "Could not determine size of file: '" + path_str + "'.";
            }

            if (size == 0)
            {
                return "File list file: '" + path_str + "' is empty.";
            }
            return std::string{}; // Success
        });
    // specify the min and max number of options from the group to be allowed.
    input_source_group->require_option(1, 1);

    app.add_option("--log-file-name", program_options.log_file_path_name_,
                   "Path name of file to write logging data to. If not provided, will log to console.");

    app.add_option(
           "-l,--log-level", program_options.logging_level_,
           "Logging level. Must be one of: 'none', 'error', 'information', or 'debug'. Defaults to: 'information'.")
        ->check(CLI::IsMember(
            {"none", "error", "information", "debug"})); // Restricts input to one of the strings in the set.

    app.add_option("-d,--db-mode", program_options.DB_mode_,
                   "The database mode to use. Must be either 'test' or 'live'.")
        ->required()                              // Makes this option mandatory on the command line.
        ->check(CLI::IsMember({"test", "live"})); // Restricts input to one of the strings in the set.

    // This is an optional field that must be a positive integer if provided.
    app.add_option("--max-files", program_options.max_files_, "Maximum number of files to process.")
        ->check(CLI::PositiveNumber); // Validator: Ensures the integer is > 0.

    // A flag is an option that does not take a value. Its presence sets a boolean to true.
    app.add_flag("--replace-db-data", program_options.replace_DB_content_, "Replace existing content in the database.");

    app.add_flag("--filename-has-form", program_options.file_name_has_form_,
                 "Indicates that the input filename contains the form type.");
}
void ConfigureLogging(Options &program_options)
{
    // this logging code comes from gemini

    if (!program_options.log_file_path_name_.get().empty())
    {
        fs::path log_dir = program_options.log_file_path_name_.get().parent_path();
        if (!fs::exists(log_dir))
        {
            fs::create_directories(log_dir);
        }

        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(program_options.log_file_path_name_.get(), true);

        auto app_logger = std::make_shared<spdlog::logger>("EDGAR_Index_logger", file_sink);

        spdlog::set_default_logger(app_logger);
    }
    else
    {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        // 3. Create an asynchronous logger using the console sink.
        auto app_logger = std::make_shared<spdlog::logger>("EDGAR_Index_logger", // Name for the console logger
                                                           console_sink);

        spdlog::set_default_logger(app_logger);
    }

    // we are running before 'CheckArgs' so we need to do a little editiing
    // ourselves.

    const std::map<std::string, spdlog::level::level_enum> levels{{"none", spdlog::level::off},
                                                                  {"error", spdlog::level::err},
                                                                  {"information", spdlog::level::info},
                                                                  {"debug", spdlog::level::debug}};

    auto which_level = levels.find(program_options.logging_level_);
    if (which_level != levels.end())
    {
        spdlog::set_level(which_level->second);
    }
    else
    {
        spdlog::set_level(spdlog::level::info);
    }
}

void CheckArgs(Options &program_options)
{
    // if (fs::exists(output_directory.get()))
    // {
    //     fs::remove_all(output_directory.get());
    // }
    // fs::create_directories(output_directory.get());

    // right now, we have nothing to do.  CLI11 takes care of edits.
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
    // start logging here.  will possible change once we have parsed
    // command line.

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
        CLI11_PARSE(app, argc, argv);
        ConfigureLogging(program_options);
        CheckArgs(program_options);

        //     auto the_filters = SelectExtractors(app);
        //
        //     std::atomic<int> files_processed{0};
        //
        //     auto files_to_scan = MakeListOfFilesToProcess(input_directory, file_list, file_name_has_form, form_type);
        //
        //     std::cout << "Found: " << files_to_scan.size() << " files to process.\n";
        //
        //     auto scan_file([&the_filters, &files_processed](const auto &input_file_name) {
        //         spdlog::info(catenate("Processing file: ", input_file_name));
        //         const std::string file_content_text = LoadDataFileForUse(EM::FileName{input_file_name});
        //         EM::FileContent file_content(file_content_text);
        //
        //         try
        //         {
        //             auto use_file = FilterFiles(file_content, form_type, MAX_FILES, files_processed);
        //
        //             if (use_file)
        //             {
        //                 for (auto &e : the_filters)
        //                 {
        //                     try
        //                     {
        //                         std::visit(
        //                             [&input_file_name, file_content, &use_file](auto &&x) {
        //                                 x.UseExtractor(EM::FileName{input_file_name}, file_content, output_directory,
        //                                                use_file.value());
        //                             },
        //                             e);
        //                     }
        //                     catch (std::exception &ex)
        //                     {
        //                         std::cerr << ex.what() << '\n';
        //                     }
        //                 }
        //             }
        //         }
        //         catch (std::exception &ex)
        //         {
        //             std::cerr << ex.what() << '\n';
        //         }
        //     });
        //
        //     std::for_each(std::execution::seq, std::begin(files_to_scan), std::end(files_to_scan), scan_file);
        //
        //     for (const auto &e : the_filters)
        //     {
        //         if (auto f = std::get_if<Count_XLS>(&e))
        //         {
        //             std::cout << "Found: " << f->XLS_counter << " spread sheets.\n";
        //         }
        //     }
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
