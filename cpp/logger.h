#pragma once

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <cstdint>


class Logger {
    enum class Level : uint8_t {
        DEBUG   = 0,
        INFO    = 1,
        WARNING = 2,
        ERROR   = 3
    };
        
    private:
        static std::string get_timestamp() {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            ss << "," << std::setfill('0') << std::setw(3) << ms.count();
            return ss.str();
        }
        
        static std::string get_level_string(Level level) {
            switch (level) {
                case Level::DEBUG: return "DEBUG";
                case Level::INFO: return "INFO";
                case Level::WARNING: return "WARNING";
                case Level::ERROR: return "ERROR";
                default: return "OTHER";
            }
        }

        template<typename... Args>
        static void log(Level message_level, Args&&... args) {
            if (message_level >= level) {
                std::cout << "[" << get_timestamp() << "] " << get_level_string(message_level) << ": ";
                ((std::cout << args), ...);
                std::cout << std::endl;
            }
        }

    public:
        inline static Level level = Logger::Level::INFO;
        
        template<typename... Args>
        static void debug(Args&&... args) {
            log(Level::DEBUG, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void info(Args&&... args) {
            log(Level::INFO, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void warning(Args&&... args) {
            log(Level::WARNING, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void error(Args&&... args) {
            log(Level::ERROR, std::forward<Args>(args)...);
        }
};
