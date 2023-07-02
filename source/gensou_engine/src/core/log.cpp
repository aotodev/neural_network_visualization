#if !defined(APP_SHIPPING) && !defined(APP_ANDROID)

#include "core/core.h"
#include "core/log.h"

PUSH_IGNORE_WARNING
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
POP_IGNORE_WARNING

namespace gs {

	std::shared_ptr<spdlog::logger> log::s_engine_logger;
	std::shared_ptr<spdlog::logger> log::s_client_logger;

	void log::init(const std::string& inAppName)
	{
		spdlog::set_pattern("%^[%T] %n: %v%$");

		s_engine_logger = spdlog::stdout_color_mt("GENSOU-ENGINE");
		s_engine_logger->set_level(spdlog::level::trace);

		s_client_logger = spdlog::stdout_color_mt(inAppName);
		s_client_logger->set_level(spdlog::level::trace);

		LOG_ENGINE(trace, "init log");
	}

}

#endif