#pragma once

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <cstdint>


class Logger {
    enum class Level : uint8_t {
        Debug   = 0,
        Info    = 1,
        Warning = 2,
        Error   = 3
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
                case Level::Debug: return "DEBUG";
                case Level::Info: return "INFO";
                case Level::Warning: return "WARNING";
                case Level::Error: return "ERROR";
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
        inline static Level level = Logger::Level::Info;
        
        template<typename... Args>
        static void debug(Args&&... args) {
            log(Level::Debug, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void info(Args&&... args) {
            log(Level::Info, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void warning(Args&&... args) {
            log(Level::Warning, std::forward<Args>(args)...);
        }
        
        template<typename... Args>
        static void error(Args&&... args) {
            log(Level::Error, std::forward<Args>(args)...);
        }
};
