#include "FileSystem.h"

#include <string>
#include <json/reader.h>
#include <json/writer.h>
#include <filesystem>
#include <fstream>


#include "logsModule/logger.h"

namespace SFE {
	bool FileSystem::isFileExists(std::string_view path) {
		return std::filesystem::exists(path);
	}

	bool FileSystem::readFile(std::string_view path, std::string& file) {
		std::ifstream inputFile;

		inputFile.open(path.data());
		if (!inputFile.is_open()) {
			LogsModule::Logger::LOG_ERROR("FileSystem::FILE_NOT_SUCCESSFULLY_READ: %s", path);
			return false;
		}

		std::string line;
		while (std::getline(inputFile, line)) {
			file += line + "\n";
		}

		inputFile.close();

		return true;
	}

	bool FileSystem::writeFile(std::string_view path, std::string& file) {
		std::ofstream outputFile;

		outputFile.open(path.data());
		if (!outputFile.is_open()) {
			LogsModule::Logger::LOG_ERROR("FileSystem::FILE_NOT_SUCCESSFULLY_WRITE: %s", path);
			return false;
		}

		outputFile.write(file.c_str(), file.size());
		outputFile.close();

		return true;
	}

	bool FileSystem::readJson(std::string_view path, Json::Value& root, bool withComments) {
		std::ifstream ifs;
		ifs.open(path.data());
		if (!ifs.is_open()) {
			LogsModule::Logger::LOG_ERROR("FileSystem::readJson can not open file: %s", path.data());
			return false;
		}

		static Json::CharReaderBuilder builder;
		builder["collectComments"] = withComments;

		static JSONCPP_STRING errs;
		const bool success = parseFromStream(builder, ifs, &root, &errs);

		ifs.close();

		if (!success) {
			LogsModule::Logger::LOG_ERROR("FileSystem::readJson %s", errs.c_str());
			return false;
		}

		return true;
	}

	bool FileSystem::writeJson(std::string_view path, const Json::Value& root) {
		std::ofstream ofs;
		ofs.open(path.data());
		if (!ofs.is_open()) {
			LogsModule::Logger::LOG_ERROR("FileSystem::writeJson can not open file: %s", path.data());
			return false;
		}

		static Json::StreamWriterBuilder builder;
		static const std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());

		writer->write(root, &ofs);

		ofs.close();

		return true;
	}

	Json::Value FileSystem::readJson(std::string_view path, bool withComments) {
		Json::Value json;
		readJson(path, json, withComments);

		return json;
	}
}

