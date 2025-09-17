// =====================================================================================
//
//       Filename:  Main.cpp
//
//    Description:  Driver program for application
//
//        Version:  1.0
//        Created:  02/28/2014 03:04:02 PM
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

#include <iostream>
#include <signal.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include "ExtractorApp.h"

int main(int argc, char **argv)
{
    //	help to optimize c++ stream I/O (may screw up threaded I/O though)

    std::ios_base::sync_with_stdio(false);

    // getting disconnected from DB can terminate the program
    // so, let's avoid it.  (apparently, this is an OK strategy)
    //
    signal(SIGPIPE, SIG_IGN);

    auto my_default_logger = spdlog::stdout_color_mt("ExtractorApp_logger");
    spdlog::set_default_logger(my_default_logger);

    int result = 0;
    try
    {
        ExtractorApp myApp(argc, argv);
        bool startup_ok = myApp.Startup();
        if (startup_ok)
        {
            myApp.Run();
            myApp.Shutdown();
        }
    }

    catch (std::system_error &e)
    {
        // any system problems, we eventually abort, but only after finishing work in process.

        auto ec = e.code();
        std::cerr << "Category: " << ec.category().name() << ". Value: " << ec.value() << ". Message: " << ec.message()
                  << '\n';
    }
    catch (std::exception &e)
    {
        std::cout << "Problem collecting files: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cout << "Unknown problem collecting files." << '\n';
    }

    return result;
}
