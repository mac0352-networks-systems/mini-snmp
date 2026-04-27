
// log.h: implements the Logger Structure interface

#ifndef LOG_H
#define LOG_H

#include <fstream>
#include <mutex>
#include <string>

using namespace std;

class Logger
{
public:
	enum class Level
	{
		INFO,
		ERROR,
		DEBUG
	};

	enum class OutputMode
	{
		FILE_ONLY,
		TERMINAL_ONLY,
		BOTH
	};

	Logger(const string &folderPath,
		   const string &fileName = "app.log",
		   OutputMode mode = OutputMode::FILE_ONLY);
	~Logger();

	void log(Level level, const string &message);
	void info(const string &message);
	void error(const string &message);
	void debug(const string &message);

private:
	ofstream fileStream;
	mutex writeMutex;
	OutputMode outputMode;

	string timestamp() const;
	string levelToString(Level level) const;
	string formatLine(Level level, const string &message) const;
};

#endif
