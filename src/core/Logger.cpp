#include "core/Logger.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace astrocore {

std::shared_ptr<spdlog::logger> Logger::s_coreLogger;

void Logger::init() {
    std::vector<spdlog::sink_ptr> sinks;

    // Console sink with colors
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] [%l] %n: %v%$");
    sinks.push_back(consoleSink);

    // File sink
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("astrocore.log", true);
    fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %n: %v");
    sinks.push_back(fileSink);

    s_coreLogger = std::make_shared<spdlog::logger>("ASTRO", sinks.begin(), sinks.end());
    s_coreLogger->set_level(spdlog::level::trace);
    s_coreLogger->flush_on(spdlog::level::trace);

    spdlog::register_logger(s_coreLogger);
}

}  // namespace astrocore
