#pragma once

#include "core/window.h"
#include "core/input_codes.h"
#include "core/input.h"
#include "scene/game_instance.h"

#include <vulkan/vulkan.h>

namespace gs {

	class scene;

	class gensou_app
	{
		friend class scene;

	public:
		~gensou_app() = default;

		void run();
		static gensou_app* create();
		static void destroy();
		static gensou_app* get();

		static void show_msg(const std::string& msg);

		void init();
		void start();
		void update();
		void terminate();

		window* get_window() const noexcept { return m_window.get(); }

		void handle_vulkan_error(VkResult result, const std::string& message);

	private:
		gensou_app();

	private:
		static gensou_app* s_instance;
		std::unique_ptr<window> m_window;

		game_instance* m_game_instance = nullptr;
	};

}