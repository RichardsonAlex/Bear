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

#pragma once

#include <unistd.h>

#include <iosfwd>
#include <memory>
#include <utility>

#include "Result.h"

namespace er {

    class Event;
    using EventPtr = std::shared_ptr<Event>;

    class Event {
    public:
        virtual ~Event() noexcept = default;

        virtual const char* name() const = 0;

        virtual void to_json(std::ostream&) const = 0;

    public:
        static rust::Result<EventPtr> start(pid_t pid, const char** cmd) noexcept;
        static rust::Result<EventPtr> stop(pid_t pid, int exit) noexcept;
    };

    class Reporter;
    using ReporterPtr = std::shared_ptr<Reporter>;

    class Reporter {
    public:
        virtual ~Reporter() noexcept = default;

        virtual rust::Result<int> send(const EventPtr& event) noexcept = 0;

    public:
        static rust::Result<ReporterPtr> tempfile(char const*) noexcept;
    };
}
