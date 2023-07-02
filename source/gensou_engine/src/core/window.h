#pragma once

#include "core/core.h"

namespace gs {

	class swapchain;
	class system;
	enum class cursor_type;

	/* these are relevant only for desktop */
	struct window_properties
	{
		std::string name = "window";

		/* if width, height and aspect_ratio are all 0,
		 * then a window will be created with the monitor working area as extent
		 */

		/* width/height of zero means the monitor working width/height */
		uint32_t width = 0, height = 0;

		/* a aspect ratio of 0.0f is ignored */
		float aspect_ratio = 0.0f;

		/* will be ignored if 0 or if the logo_path string is not empty */
		int8_t embedded_logo_start = 0, embedded_logo_end = 0;

		/* will be ignored if empty() or if embedded_logo... is 0 */
		std::string logo_path;

		static window_properties get_default(uint32_t width = 0, uint32_t height = 0);
	};

	class window
	{
	public:
		virtual ~window() = default;

		virtual void init() = 0;

		virtual void* get() const = 0;
		virtual void* get_native_window() { return nullptr; }

		virtual const uint32_t get_width() const = 0;
		virtual const uint32_t get_height() const = 0;
		virtual const extent2d get_extent() const = 0;

		virtual void resize(uint32_t width, uint32_t height) = 0;

		virtual void poll_events() = 0;
		virtual void update() = 0;
		virtual void swap_buffers() = 0;

		virtual void create_swapchain(bool use_vsync) = 0;
		virtual void destroy_swapchain() = 0;
		virtual std::shared_ptr<swapchain> get_swapchain() = 0;

		virtual void set_vsync(bool enabled) = 0;
		virtual bool is_vsync() const = 0;
		virtual void set_focused(bool isFocused) = 0;
		virtual bool focused() const = 0;
		virtual bool should_close_window() const = 0;
		virtual bool supports_nonvsync_mode() const = 0;

		/* desktop enviroment specifc */
		virtual std::string open_file(const char* filter) { return std::string(); }
		virtual std::string save_file(const char* filter) { return std::string(); }

		virtual void dialog_box(const std::string& message) {}

		virtual void set_cursor_type(cursor_type type) {}

		/* only relevant for phones and tablets */
		virtual uint32_t get_display_cutout_height() { return 0; }

		/* factory */
		[[nodiscard]] static window* create(const window_properties& properties);

		virtual void request_minimize() {}
		virtual void request_restore() {}
		virtual void request_destroy() {}

	protected:
		virtual void set_icon(const int8_t* binaryStart, const int8_t* binaryEnd, bool flip = true) {}
		virtual void set_icon(const std::string& path, bool flip = true) {}

	private:
		friend class system;
	};

}