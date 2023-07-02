#pragma once

#include "core/core.h"
#include "core/log.h"

namespace gs::utils {

	class benchmark_timer
	{
	public:

		benchmark_timer(const std::string& text)
			: m_text(text)
		{
			m_start_point = std::chrono::high_resolution_clock::now();
		}

		~benchmark_timer()
		{
			stop();
		}
	private:
		void stop()
		{
			auto endPoint = std::chrono::high_resolution_clock::now();

			auto start = std::chrono::time_point_cast<std::chrono::microseconds>(m_start_point).time_since_epoch().count();
			auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endPoint).time_since_epoch().count();

			auto duration = end - start;
			double ms = duration * 0.001;

			#ifdef APP_ANDROID
			LOG_ENGINE(info, "BENCHMARK %s: %.4fms", m_text.c_str(), ms);
			#else
			printf("\033[0;33;44mBENCHMARK[%s]: %.4fms\033[0m\n", m_text.c_str(), ms);
			//printf("BENCHMARK[%s]: %.4fms\n", m_text.c_str(), ms);
			#endif
		}

	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_start_point;
		std::string m_text;
	};

}

#ifndef APP_SHIPPING

#if PRINT_BENCHMARK
#define BENCHMARK(tag) gs::utils::benchmark_timer CAT(benchmark_timer_, __COUNTER__) (tag);
#else
#define BENCHMARK(tag)
#endif

#if PRINT_BENCHMARK_VERBOSE
#define BENCHMARK_VERBOSE(tag) gs::utils::benchmark_timer CAT(benchmark_timer_verbose_, __COUNTER__) (tag);
#else
#define BENCHMARK_VERBOSE(tag)
#endif

#else
#define BENCHMARK(tag)
#define BENCHMARK_VERBOSE(tag)
#endif /* APP_SHIPPING */