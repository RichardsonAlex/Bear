/*  Copyright (C) 2012-2020 by László Nagy
    This file is part of Bear.

    Bear is a tool to generate compilation database for clang tooling.

    Bear is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Bear is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include "Environment.h"
#include "Reporter.h"
#include "Result.h"
#include "Session.h"
#include "SystemCalls.h"

using rust::Result;
using rust::Ok;
using rust::Err;
using rust::merge;

namespace {

    std::ostream& error_stream()
    {
        std::cerr << "er: [pid: "
                  << er::SystemCalls::get_pid().unwrap_or(0)
                  << ", ppid: "
                  << er::SystemCalls::get_ppid().unwrap_or(0)
                  << "] ";
        return std::cerr;
    }

    std::vector<const char*> to_char_vector(const std::vector<std::string_view>& input)
    {
        auto result = std::vector<const char*>(input.size());
        std::transform(input.begin(), input.end(), result.begin(), [](auto it) { return it.data(); });
        result.push_back(nullptr);
        return result;
    }

    Result<pid_t> spawnp(const ::er::Execution& config,
        const ::er::EnvironmentPtr& environment) noexcept
    {
        auto command = to_char_vector(config.command);
        return er::SystemCalls::spawn(config.path.data(), command.data(), environment->data());
    }

    void report_start(Result<er::ReporterPtr> const& reporter, pid_t pid, const char** cmd) noexcept
    {
        merge(reporter, ::er::Event::start(pid, cmd))
            .and_then<int>([](auto tuple) {
                const auto& [rptr, eptr] = tuple;
                return rptr->send(eptr);
            })
            .unwrap_or_else([](auto message) {
                error_stream() << "report start: " << message.what() << std::endl;
                return 0;
            });
    }

    void report_exit(Result<er::ReporterPtr> const& reporter, pid_t pid, int exit) noexcept
    {
        merge(reporter, ::er::Event::stop(pid, exit))
            .and_then<int>([](auto tuple) {
                const auto& [rptr, eptr] = tuple;
                return rptr->send(eptr);
            })
            .unwrap_or_else([](auto message) {
                error_stream() << "report stop: " << message.what() << std::endl;
                return 0;
            });
    }

    ::er::EnvironmentPtr create_environment(char* original[], const ::er::Session& session)
    {
        return er::Environment::Builder(const_cast<const char**>(original))
            .add_reporter(session.context_.reporter.data())
            .add_destination(session.context_.destination.data())
            .add_verbose(session.context_.verbose)
            .add_library(session.library_.data())
            .build();
    }

    std::ostream& operator<<(std::ostream& os, char* const* values)
    {
        os << '[';
        for (char* const* it = values; *it != nullptr; ++it) {
            if (it != values) {
                os << ", ";
            }
            os << '"' << *it << '"';
        }
        os << ']';

        return os;
    }

}

int main(int argc, char* argv[], char* envp[])
{
    return ::er::parse(argc, argv)
        .map<er::Session>([&argv](auto arguments) {
            if (arguments.context_.verbose) {
                error_stream() << argv << std::endl;
            }
            return arguments;
        })
        .and_then<int>([&envp](auto arguments) {
            auto reporter = er::Reporter::tempfile(arguments.context_.destination.data());

            auto environment = create_environment(envp, arguments);
            return spawnp(arguments.execution_, environment)
                .template map<pid_t>([&arguments, &reporter](auto& pid) {
                    report_start(reporter, pid, to_char_vector(arguments.execution_.command).data());
                    return pid;
                })
                .template and_then<std::tuple<pid_t, int>>([](auto pid) {
                    return er::SystemCalls::wait_pid(pid)
                        .template map<std::tuple<pid_t, int>>([&pid](auto exit) {
                            return std::make_tuple(pid, exit);
                        });
                })
                .template map<int>([&reporter](auto tuple) {
                    const auto& [pid, exit] = tuple;
                    report_exit(reporter, pid, exit);
                    return exit;
                });
        })
        .unwrap_or_else([](auto message) {
            error_stream() << message.what() << std::endl;
            return EXIT_FAILURE;
        });
}
