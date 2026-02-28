#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace astrocore {

class Logger {
public:
    static void init();
    static std::shared_ptr<spdlog::logger>& getCoreLogger() { return s_coreLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_coreLogger;
};

}  // namespace astrocore

// Convenience macros
#define LOG_TRACE(...)    ::astrocore::Logger::getCoreLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::astrocore::Logger::getCoreLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     ::astrocore::Logger::getCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::astrocore::Logger::getCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::astrocore::Logger::getCoreLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::astrocore::Logger::getCoreLogger()->critical(__VA_ARGS__)
