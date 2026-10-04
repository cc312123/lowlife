#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <chrono>
#include <iomanip>

class Logger {
public:
    static Logger& instance();
    void log(const std::string& level, const std::string& message);
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    std::ofstream file_;
    std::mutex mtx_;
};

#define LOG_INFO(msg) Logger::instance().log("INFO", msg)
#define LOG_ERROR(msg) Logger::instance().log("ERROR", msg)
