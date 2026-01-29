//  Copyright (C) 2024, Max Piochowiak and Reiner Dolp, Karlsruhe Institute of Technology
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef MINITIMER_HPP
#define MINITIMER_HPP

#include <chrono>
#include <iomanip>

/// @brief Lightweight (but inaccurate) timer class for measuring elapsed time in seconds using std::chrono::high_resolution_clock. Usage:
///
/// MiniTimer t;\n
/// // do stuff..\n
/// auto seconds_since_creation = t.elapsed();
class MiniTimer {
public:
    MiniTimer() : m_startTime(std::chrono::high_resolution_clock::now()) {}
    ~MiniTimer() = default;

    /// Restarts the timer.
    /// @return the time in seconds passed since the object was created or since the last time start was called.
    double restart() {
        auto ret = elapsed();
        m_startTime = std::chrono::high_resolution_clock::now();
        return ret;
    }

    /// @return the time in seconds passed since the object was created or since the last time start was called.
    inline double elapsed() { return static_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - m_startTime).count(); }

    static float getFloatSystemClock() {
        return static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()) / 1000.f;
    }

    static std::string getCurrentDateTime(const std::string &format = "%Y-%m-%d %X") {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), format.c_str());
        return ss.str();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
};

#endif //MINITIMER_HPP