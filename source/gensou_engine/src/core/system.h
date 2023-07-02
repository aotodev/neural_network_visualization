#pragma once

#include "core/core.h"
#include "core/log.h"
#include "core/uuid.h"
#include "core/input_codes.h"
#include "core/cmd_queue.h"
#include "core/window.h"

#include <glm/glm.hpp>
#include <streambuf>
#include <type_traits>

namespace gs {

	/* to easily convert from our resource type into a c++ style stream buffer */
	class gs_stream_buffer : public std::streambuf
	{
	public:
		gs_stream_buffer(byte* start, size_t size)
		{
			char* pStart = (char*)start;
			char* pEnd = pStart + size;
			this->setg(pStart, pStart, pEnd);
		}
	};

	struct gensou_file
	{
		friend class system;

	private:
		std::unique_ptr<byte[]> m_data;
		size_t m_size = 0;
		uuid m_id{ 0ULL };

	public:
		gensou_file() = default;

		gensou_file(const gensou_file&) = delete;
		gensou_file& operator=(const gensou_file&) = delete;

		gensou_file(gensou_file&& other) noexcept
			: m_data(std::move(other.m_data)), m_size(other.m_size), m_id(other.m_id)
		{
			other.m_data = nullptr;
			other.m_size = 0;
			other.m_id = uuid(0ULL);
		}

		gensou_file& operator=(gensou_file&& other) noexcept
		{ 
			m_data = std::move(other.m_data);
			m_size = other.m_size;
			m_id = other.m_id;

			other.m_data = nullptr;
			other.m_size = 0;
			other.m_id = uuid(0ULL);
			
			return *this;
		}

		byte* data() const { return m_data.get(); }
		size_t size() const { return m_size; }
		uuid id() const { return m_id; }

		gs_stream_buffer data_as_buffer_stream()
		{
			return gs_stream_buffer(m_data.get(), m_size);
		}

		void reset()
		{
			m_data = nullptr;
			m_size = 0ULL;
			m_id = { 0LL };
		}

		/* yields ownership of the data and invalidates this gensou_file instance 
		 * (the cast is not safe, use at your own risk)
		 * mind that a gensou_file.data is simply a pointer to a buffer of raw data loaded from a file
		 * it only makes scene to cast it to trivially copyable types (scalars, POD, etc) 
		 */
		template<typename T>
		std::unique_ptr<T> get_data_as()
		{
			static_assert(std::is_trivially_copyable_v<T>);

			assert(m_data);
			if(sizeof(T) != m_size)
			{
				LOG_ENGINE(error, "sizes do not match | sizeof(T) == %zu, m_size == %zu", sizeof(T), m_size);
				assert(sizeof(T) == m_size);
			}

			std::unique_ptr<T> out{ reinterpret_cast<T*>(m_data.release()) };

			reset();

			return out;
		}

		bool valid() const { return m_data && m_size && m_id; }

		operator bool() { return m_data && m_size && m_id; }
	};

	struct app_settings
	{
		uint32_t width = 0, height = 0;
		bool use_postprocess = true;
		bool vsync = true;
	};

	class system
	{
	public:
		static void init();
		static void terminate();

		static void rumble();
		static void error_msg(const std::string& msg);

		static void set_rumbler_active(bool setRumble) { s_rumbler_active = setRumble; }

		/* if a mouse is connected changes the cursor type, otherwise does nothing */
		static void set_cursor_type(cursor_type cursorType);

		static bool is_rumbler_active() { return s_rumbler_active; }
		static bool supports_rumbler()
		{ 
			#ifdef APP_ANDROID
			return true;
			#else
			return false;
			#endif
		}
		static bool supports_nonvsync_mode();

		static std::shared_ptr<app_settings> get_settings();
		static void serialize_settings(std::shared_ptr<app_settings> settings);
		static void serialize_data(const std::string& path, const void* data, size_t dataSize);

		static std::shared_ptr<gensou_file> deserialize_data(const std::string& path);

		/* not safe, use only when sure about the type */
		template<typename T>
		static std::unique_ptr<T> deserialize_data_as(const std::string& path)
		{
			return deserialize_data(path)->get_data_as<T>();
		}

		/* terminates application and exits */
		static void exit();

		/* returns the id from a disk file if it has already being loaded before, returns a 0-id otherwise */
		static uuid get_cached_id_from_file(const std::string& filePath);

		/* returns a copy of  */
		static const char* get_internal_data_path() { return s_internal_data_path.c_str(); }
		static std::string make_path_from_internal_data(const std::string& path);

		static void set_clear_value(const glm::vec4& color);

		static bool vsync();
		static void set_vsync(bool enabled);

		static uint32_t get_display_cutout_height();

		static std::shared_ptr<gensou_file> load_file(const std::string& path);

		/* loads a free file (not a gensou_file) from the application's internal data folder */
		static std::vector<byte> load_internal_file(const std::string& path);

		/* loads an asset file and checks its magic number to assert it's an spv shader file */
		static std::vector<byte> load_spv_file(const std::string& path);

		/* android_app pointer in case of android, HINSTANCE in case of windows */
		static void* get_platform_data() { return s_platform_data; }
		static void set_platform_data(void* data) { s_platform_data = data; }

		template<typename Functor>
		static std::future<void> run_on_loading_thread(Functor&& functor)
		{
			return s_loading_thread.submit(std::forward<Functor>(functor));
		}

		/* 
		 * only for regular tasks
		 * not suitable for vulkan commands as they do not have their own command pool
		 * for these, use the loading, renderer and main threads
		 */
		template<typename Functor>
		static std::future<void> run_async(Functor&& functor)
		{
			return s_thread_pool.submit(std::forward<Functor>(functor));
		}

		template<typename Functor>
		static void submit_render_cmd(uint32_t frame, Functor&& functor)
		{
			s_render_thread.submit(frame, std::forward<Functor>(functor));
		}

		static std::future<void> execute_render_cmds(uint32_t frame)
		{
			return s_render_thread.execute(frame);
		}
		
		static std::thread::id get_main_thread_id() { return s_main_thread_id; }
		static std::thread::id get_render_thread_id() { return s_render_thread_id; }
		static std::thread::id get_loading_thread_id() { return s_loading_thread_id; }

	private:
		static std::shared_ptr<gensou_file> deserialize_data_internal(const std::string& path);
		static std::shared_ptr<app_settings> deserialize_settings();
		
	private:
		static bool s_rumbler_active;
		static std::string s_internal_data_path;

		static std::shared_ptr<app_settings> s_app_settings;
		static void* s_platform_data;

	private:
		/*-----------------THREAD-RELATED--------------------------*/
		struct render_thread
		{
			operator bool() const { return id != 0; }

			std::array<cmd_queue, MAX_FRAMES_IN_FLIGHT> command_queues;
			uint32_t current_frame = 0; /* current frame of render thread (which is always one behind the app thread */

			std::thread thread;
			mutable std::mutex mutex;
			std::condition_variable mutex_condition;

			std::promise<void> promise;

			uint32_t id = 0;
			std::string thread_name;

			bool is_alive = true;
			bool active = false;

			template<typename Functor>
			void submit(uint32_t frame, Functor&& functor)
			{
				auto commandfn = [](void* functor_ptr)
				{
					auto pFunctorAsCmd = (Functor*)functor_ptr;
					(*pFunctorAsCmd)();
					pFunctorAsCmd->~Functor();
				};

				std::unique_lock<std::mutex> lock(mutex);
				auto cmdBuffer = command_queues[frame].enqueue(commandfn, sizeof(Functor));
				new(cmdBuffer) Functor(std::forward<Functor>(functor));
			}

			std::future<void> execute(uint32_t frame)
			{
				std::unique_lock<std::mutex> lock(mutex);
				active = true;
				current_frame = frame;
				mutex_condition.notify_one();

				return promise.get_future();
			}
		};

		struct loading_thread
		{
			struct task
			{
				task() = default;

				template<typename Functor>
				task(Functor&& f) : function(f) {}

				std::function<void(void)> function;
				std::promise<void> promise;
				bool free = false;
			};

			/* a simple ring buffer structure */
			/* only thread-safe if there is a single producer thread and single consumer thread */
			std::vector<task> task_queue;
			uint32_t head_ptr = 0, tail_ptr = 0;

			std::thread thread;
			mutable std::mutex mutex;
			std::condition_variable mutex_condition;

			uint32_t id = 0;
			std::string thread_name;
			bool is_alive = true;

			operator bool() const { return id; }

			bool has_work() const { return tail_ptr != head_ptr; }

			template<typename Functor>
			std::future<void> submit(Functor&& functor)
			{
				if (head_ptr == task_queue.size())
					head_ptr = 0;

				task_queue[head_ptr] = task(std::forward<Functor>(functor));
				auto oldtPtr = head_ptr++;

				std::unique_lock<std::mutex> lock(mutex);
				mutex_condition.notify_one();

				return task_queue[oldtPtr].promise.get_future();
			}

		};

		static class thread_pool
		{
		public:
			struct task
			{
				task() = default;

				template<typename Functor>
				task(Functor&& f) : function(f) {}

				std::function<void(void)> function;
				std::promise<void> promise;
			};

			static constexpr uint32_t thread_count = 4UL;

			std::vector<std::thread> threads;
			std::queue<task> tasks_queue;

			mutable std::mutex mutex;
			std::condition_variable mutex_condition;

			bool is_alive = true;

			/*------------------------------------------------------------------*/
			void terminate()
			{
				{
					std::unique_lock<std::mutex> lock(s_render_thread.mutex);
					is_alive = false;
					mutex_condition.notify_all();
				}
				
				for(auto& t : threads)
					t.join();

				threads.clear();
			}

			template<typename Functor>
			std::future<void> submit(Functor&& functor)
			{
				std::unique_lock<std::mutex> lock(mutex);
				auto& newTask = tasks_queue.emplace(std::forward<Functor>(functor));
				mutex_condition.notify_one();

				return newTask.promise.get_future();
			}

		} s_thread_pool;

		/* the render, loading and main threads can do operations that require command buffers */
		static render_thread s_render_thread;
		static loading_thread s_loading_thread;

		/* for selecting the right command pool when allocating command buffers */
		static std::thread::id s_main_thread_id, s_render_thread_id, s_loading_thread_id;
	};
}