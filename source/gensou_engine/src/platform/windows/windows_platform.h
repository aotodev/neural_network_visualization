#pragma once

#ifdef APP_WINDOWS

#include "core/window.h"

/* NOMINMAX avois naming conflits with std::max/min */
#define NOMINMAX 

//with this macro vulkan.h will include <windows.h> & "vulkan_win32.h"
#define VK_USE_PLATFORM_WIN32_KHR

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>

namespace gs {

	class swapchain;

	class windows_window : public window
	{
	public:
		windows_window(const window_properties& props)
			: 	m_name(props.name),
				m_extent(props.width, props.height),
				m_initial_aspect_ratio(props.aspect_ratio),
				m_logo_start(props.embedded_logo_start),
				m_logo_end(props.embedded_logo_end),
				m_logo_path(props.logo_path) {}

		~windows_window();

		virtual void init() final override;

		virtual void* get() const final override { return m_window; }

		virtual const extent2d get_extent() const final override { return m_extent; }
		virtual const uint32_t get_width() const final override { return m_extent.width; }
		virtual const uint32_t get_height() const final override { return m_extent.height; }

		virtual void resize(uint32_t width, uint32_t height) override;

		virtual void poll_events() final override;
		virtual void update() final override;
		virtual void swap_buffers() final override;

		virtual void create_swapchain(bool useVSync) override;
		virtual void destroy_swapchain() override;
		virtual std::shared_ptr<swapchain> get_swapchain() override { return m_swapchain; }

		virtual void set_vsync(bool enabled) final override;
		virtual bool is_vsync() const final override { return m_vsync; }
		virtual void set_focused(bool isFocused) final override { m_focused = isFocused; }

		virtual bool focused() const final override { return !m_minimized; };
		void set_minimized(bool minimized) { m_minimized = minimized; }

		virtual bool should_close_window() const final override { return m_should_close_window; }
		virtual bool supports_nonvsync_mode() const final override;

		virtual void set_cursor_type(cursor_type type) override;

		virtual std::string open_file(const char* filter) final override;
		virtual std::string save_file(const char* filter) final override;

	protected:
		virtual void set_icon(const int8_t* binaryStart, const int8_t* binaryEnd, bool flip = true) final override;
		virtual void set_icon(const std::string& path, bool flip = true) final override;

	private:
		void set_window_close() { m_should_close_window = true; }

		virtual void request_minimize() final override;
		virtual void request_restore() final override;
		virtual void request_destroy() final override { m_should_close_window = true; }

	private:
		GLFWwindow* m_window = nullptr;
		std::shared_ptr<swapchain> m_swapchain;

		extent2d m_extent;
		float m_initial_aspect_ratio;
		bool m_focused = false, m_minimized = false;
		bool m_should_close_window = false, m_vsync = true;
		std::string m_name;

		/* from path */
		std::string m_logo_path = "";

		/* from embedded resource */
		int8_t m_logo_start = 0, m_logo_end = 0;
	};

}

#endif /* APP_WINDOWS */