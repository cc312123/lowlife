#include "logger.h"
#include <chrono>
#include <ctime>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    std::string filename = "cheat_debug.log";
    file_.open(filename, std::ios::out | std::ios::app);
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!file_.is_open()) return;
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    file_ << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << " [" << level << "] " << message << "\n";
    file_.flush();
}
