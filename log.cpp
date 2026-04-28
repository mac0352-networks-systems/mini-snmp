
// log.cpp: implements the Logger Structure operations to be used by the server.cpp for message registration.

#include "log.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

Logger::Logger(const string &folderPath, const string &fileName, OutputMode mode)
	: outputMode(mode)
{
	if (folderPath.empty() || fileName.empty())
		throw invalid_argument("folderPath and fileName must not be empty");

	filesystem::create_directories(folderPath);

	const filesystem::path filePath = filesystem::path(folderPath) / fileName;
	fileStream.open(filePath, ios::app);

	if (!fileStream.is_open())
	{
		throw runtime_error("Unable to open log file: " + filePath.string());
	}
}

Logger::~Logger()
{
	if (fileStream.is_open())
	{
		fileStream.close();
	}
}

void Logger::log(Level level, const string &message)
{
	lock_guard<mutex> lock(writeMutex);

	const string line = formatLine(level, message);

	if (outputMode == OutputMode::FILE_ONLY || outputMode == OutputMode::BOTH)
	{
		fileStream << line << '\n';
		fileStream.flush();
	}

	if (outputMode == OutputMode::TERMINAL_ONLY || outputMode == OutputMode::BOTH)
	{
		cout << line << '\n';
	}
}

void Logger::info(const string &message)
{
	log(Level::INFO, message);
}

void Logger::error(const string &message)
{
	log(Level::ERROR, message);
}

void Logger::debug(const string &message)
{
	log(Level::DEBUG, message);
}

string Logger::timestamp() const
{
	const auto now = chrono::system_clock::now();
	const time_t nowTime = chrono::system_clock::to_time_t(now);

	tm tmSnapshot;
	localtime_r(&nowTime, &tmSnapshot);

	ostringstream oss;
	oss << put_time(&tmSnapshot, "%Y-%m-%d %H:%M:%S");
	return oss.str();
}

string Logger::levelToString(Level level) const
{
	switch (level)
	{
	case Level::INFO:
		return "INFO";
	case Level::ERROR:
		return "ERROR";
	case Level::DEBUG:
		return "DEBUG";
	}

	return "INFO";
}

string Logger::formatLine(Level level, const string &message) const
{
	return "[" + timestamp() + "] [" + levelToString(level) + "] " + message;
}
