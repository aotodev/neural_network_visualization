#pragma once

//#define APP_ANDROID
#ifdef APP_ANDROID

#include "core/window.h"

#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>

#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>


namespace gs {

	class swapchain;

	class android_window : public window
	{
	public:
		android_window(const std::string& name, uint32_t width, uint32_t height);

		android_window(const window_properties& props)
			:	m_name(props.name),
				m_extent(props.width, props.height),
				m_initial_aspect_ratio(props.aspect_ratio) {}

		~android_window();

		virtual void init() final override;

		virtual void* get() const final override;

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
		virtual bool focused() const final override { return m_focused && is_window_ready; };
		virtual bool should_close_window() const final override { return m_should_close_window; }
		virtual bool supports_nonvsync_mode() const final override;

		virtual void dialog_box(const std::string& message) override;
		virtual uint32_t get_display_cutout_height() override;

		static void handle_cmd(android_app* inApp, int32_t inCmd);
		static int32_t handle_input(android_app* inApp, AInputEvent* inEvent);

		static void destroy_application();

		void restore_transparent_bars();


	private:
		void set_window_close(bool set = true) { m_should_close_window = set; }
		void terminate();

		virtual void request_destroy() final override;

	private:
		std::shared_ptr<swapchain> m_swapchain;

		extent2d m_extent;
		float m_initial_aspect_ratio;
		bool m_focused = false;
		bool m_should_close_window = false, m_vsync = true;
		std::string m_name;

	public:
		bool is_window_ready = false;
	};

}

#endif /* APP_ANDROID */
