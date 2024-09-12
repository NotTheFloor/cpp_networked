#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <mutex>

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error
};

class Logger
{
public:
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    static Logger &getInstance()
    {
        static Logger logger;
        return logger;
    }

    void setLogLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        logLevel = level;
    }

    void enableConsoleLogging(bool enable)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        consoleLoggingEnabled = enable;
    }

    void enableFileLogging(bool enable, const std::string &filename = "log.txt")
    {
        std::lock_guard<std::mutex> lock(_mutex);
        fileLoggingEnabled = enable;

        if (enable)
        {
            logFile.open(filename, std::ios::out | std::ios::app);

            if (!logFile)
            {
                std::cerr << "Failed to open log file: " << filename << std::endl;
                fileLoggingEnabled = false;
            }
        }
    }

    void log(LogLevel level, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(_mutex);

        if (level >= logLevel)
        {
            std::string logMessage = getLogLevelString(level) + ": " + message;

            if (consoleLoggingEnabled)
            {
                std::cout << logMessage << std::endl;
            }

            if (fileLoggingEnabled && logFile.is_open())
            {
                logFile << logMessage << std::endl;
            }
        }
    }

private:
    LogLevel logLevel;
    bool consoleLoggingEnabled;
    bool fileLoggingEnabled;
    std::ofstream logFile;
    std::mutex _mutex;

    Logger() : logLevel(LogLevel::Debug), consoleLoggingEnabled(true), fileLoggingEnabled(true)
    {
        enableFileLogging(true);
    }

    ~Logger()
    {
        if (logFile.is_open())
        {
            logFile.close();
        }
    }

    std::string getLogLevelString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        }
    }
};
