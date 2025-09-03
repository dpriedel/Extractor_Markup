#include <CLI/CLI.hpp>
#include <execution>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

CLI::App app{"Process SEC EDGAR files and extract specified data."};

EM::FileName input_directory;
EM::FileName output_directory;
EM::FileName file_list;
std::string form_type;
int MAX_FILES{-1};
bool file_name_has_form{false};

void SetupProgramOptions()
{
    app.add_option("-f,--form", form_type, "Name of form type[s] we are processing")->required();
    app.add_option("--form-dir", input_directory, "Directory of form files to be processed");
    app.add_option("--list-file", file_list, "Path to file with list of files to process.");
    app.add_option("-o,--output-dir", output_directory, "Top level directory to save outputs to")->required();
    app.add_option("--max-files", MAX_FILES, "Maximum number of files to extract. Default of -1 means no limit.");
    app.add_flag("--path-has-form", file_name_has_form, "Form number is part of file path.");
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

void CheckArgs()
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
        SetupProgramOptions();
        ParseProgramOptions(argc, argv);
        CheckArgs();

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
