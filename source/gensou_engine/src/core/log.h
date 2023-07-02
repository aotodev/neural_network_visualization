#pragma once

#include "core/core.h"

/* spdlog won't be linked at all when building for shipping */
#ifndef APP_SHIPPING

#ifndef APP_ANDROID
#if ((defined(_MSVC_LANG) && _MSVC_LANG  >= 202002L) || __cplusplus >= 202002L)
#define FMT_USE_NONTYPE_TEMPLATE_PARAMETERS 0
#endif

PUSH_IGNORE_WARNING
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/bundled/printf.h>
POP_IGNORE_WARNING

namespace gs {

	class log
	{
	public:
		static void init(const std::string& inAppName);

		static constexpr std::shared_ptr<spdlog::logger>& get_engine_logger() { return s_engine_logger; }
		static constexpr std::shared_ptr<spdlog::logger>& get_client_logger() { return s_client_logger; }

	private:
		static std::shared_ptr<spdlog::logger> s_engine_logger;
		static std::shared_ptr<spdlog::logger> s_client_logger;
	};

}

/* core log macros */
#define engine_trace(...)			::gs::log::get_engine_logger()->trace(fmt::sprintf(__VA_ARGS__).c_str())
#define engine_info(...)			::gs::log::get_engine_logger()->info(fmt::sprintf(__VA_ARGS__).c_str())
#define engine_warn(...)			::gs::log::get_engine_logger()->warn(fmt::sprintf(__VA_ARGS__).c_str())
#define engine_error(...)			::gs::log::get_engine_logger()->error(fmt::sprintf(__VA_ARGS__).c_str())
#define engine_critical(...)		::gs::log::get_engine_logger()->critical(fmt::sprintf(__VA_ARGS__).c_str())
									 
/* Client log macros */
#define log_trace(...)				::gs::log::get_client_logger()->trace(fmt::sprintf(__VA_ARGS__).c_str())
#define log_info(...)				::gs::log::get_client_logger()->info(fmt::sprintf(__VA_ARGS__).c_str())
#define log_warn(...)				::gs::log::get_client_logger()->warn(fmt::sprintf(__VA_ARGS__).c_str())
#define log_error(...)				::gs::log::get_client_logger()->error(fmt::sprintf(__VA_ARGS__).c_str())
#define log_critical(...)			::gs::log::get_client_logger()->critical(fmt::sprintf(__VA_ARGS__).c_str())

#define LOG_ENGINE(level, ...)		engine_##level(__VA_ARGS__)
#define LOG(level, ...)				log_##level(__VA_ARGS__)

#else /* APP_ANDROID is defined */
#include <android/log.h>

namespace gs {

	class log
	{
	public:
		static void init(const std::string& inAppName)
		{
		}
	};

}

/* core log macros */
#define engine_trace(...)			((void)__android_log_print(ANDROID_LOG_VERBOSE, "GENSOU-ENGINE", __VA_ARGS__))
#define engine_info(...)			((void)__android_log_print(ANDROID_LOG_INFO, "GENSOU-ENGINE", __VA_ARGS__))
#define engine_warn(...)			((void)__android_log_print(ANDROID_LOG_WARN, "GENSOU-ENGINE", __VA_ARGS__))
#define engine_error(...)			((void)__android_log_print(ANDROID_LOG_ERROR, "GENSOU-ENGINE", __VA_ARGS__))
#define engine_critical(...)		((void)__android_log_print(ANDROID_LOG_FATAL, "GENSOU-ENGINE", __VA_ARGS__))

/* Client log macros */
#define log_trace(...)				((void)__android_log_print(ANDROID_LOG_VERBOSE, GAME_NAME, __VA_ARGS__))
#define log_info(...)				((void)__android_log_print(ANDROID_LOG_INFO, GAME_NAME, __VA_ARGS__))
#define log_warn(...)				((void)__android_log_print(ANDROID_LOG_WARN, GAME_NAME, __VA_ARGS__))
#define log_error(...)				((void)__android_log_print(ANDROID_LOG_ERROR, GAME_NAME, __VA_ARGS__))
#define log_critical(...)			((void)__android_log_print(ANDROID_LOG_FATAL, GAME_NAME, __VA_ARGS__))

#define LOG_ENGINE(level, ...)		engine_##level(__VA_ARGS__)
#define LOG(level, ...)				log_##level(__VA_ARGS__)

#endif /* !APP_ANDROID */

#else 
namespace gs {

	class log
	{
	public:
		static void init(const std::string& inAppName) {}
	};

}

#define engine_trace(...)		
#define engine_info(...)			
#define engine_warn(...)			
#define engine_error(...)		
#define engine_critical(...)

/* client log macro */
#define log_trace(...)			
#define log_info(...)			
#define log_warn(...)			
#define log_error(...)			
#define log_critical(...)

#define LOG_ENGINE(level, ...)
#define LOG(level, ...)

#endif /* !defined(APP_SHIPPING) */