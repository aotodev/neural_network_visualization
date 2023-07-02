#ifdef APP_WINDOWS

#include "platform/windows/windows_platform.h"

#include "core/core.h"
#include "core/system.h"
#include "core/log.h"
#include "core/runtime.h"
#include "core/gensou_app.h"

#include "core/engine_events.h"
#include "core/input.h"

#include "renderer/swapchain.h"
#include "renderer/device.h"
#include "renderer/renderer.h"

#include <stb_image.h>

//---------------------------------------------------------------------------------------------
// EXTERN FUNCTIONS
//---------------------------------------------------------------------------------------------

VkSurfaceKHR create_vulkan_surface(void* window)
{
	VkSurfaceKHR surface;

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;

	surfaceCreateInfo.hwnd = glfwGetWin32Window((GLFWwindow*)window);;
	surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

	VkResult result = vkCreateWin32SurfaceKHR(gs::device::get_instance(), &surfaceCreateInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		gs::engine_events::vulkan_result_error.broadcast(result, "Could not create a Windows surface for presenting");

	return surface;
}

extern const char* get_platform_surface_ext()
{
	return "VK_KHR_win32_surface"; //VK_KHR_WIN32_SURFACE_EXTENSION_NAME
}

namespace gs {

	void system::error_msg(const std::string& message)
	{
		/* from WinUser.h
		 * MessageBox(NULL, "An error has occurred!", "Title!", MB_ICONERROR | MB_OK);
		 * MessageBox(NULL, message.c_str(), "Error", MB_ICONERROR | MB_OK);
		 */

		MessageBox(NULL, message.c_str(), "Error", MB_ICONERROR | MB_OK);
	}

	void system::rumble()
	{

	}

	bool input::is_pressed(key_code key)
	{
		return ::GetKeyState((int)key) & 0x8000;
		//return ::GetAsyncKeyState((int)key) & 0x8000;
	}

	window_properties window_properties::get_default(uint32_t width, uint32_t height)
	{
		return window_properties{ 
			GAME_NAME, 
			width, height,
			float(ASPECT_RATIO_NUM) / float(ASPECT_RATIO_DEN), 
			0, 0, 
			"engine_res/textures/logo_small.png" 
		};
	}

	window* window::create(const window_properties& properties)
	{
		return new windows_window(properties);
	}

	//---------------------------------------------------------------------------------------------
	// MEMBER FUNCTIONS
	//---------------------------------------------------------------------------------------------

	windows_window::~windows_window()
	{
		destroy_swapchain();

		glfwDestroyWindow(m_window);
		glfwTerminate();

		LOG_ENGINE(trace, "Destroyed window");
	}

	//==OVERRIDES================================================================

	void windows_window::init()
	{
		glfwSetErrorCallback([](int code, const char* description)
		{
			LOG_ENGINE(error, "GLFW ERROR, with code %d and message %s", code, description);
		});

		if (glfwInit() != GLFW_TRUE)
			LOG_ENGINE(critical, "could not initialize glfw");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); /* GLFW_FALSE */

		auto pMonitor = glfwGetPrimaryMonitor();
		int posX, posY, monitorWidth, monitorHeight;
		glfwGetMonitorWorkarea(pMonitor, &posX, &posY, &monitorWidth, &monitorHeight);

		monitorWidth = (int)((float)monitorWidth * 0.9f);
		monitorHeight = (int)((float)monitorHeight * 0.9f);

		if(m_extent == extent2d(0, 0))
		{
			constexpr float aspectRatio = float(ASPECT_RATIO_NUM) / float(ASPECT_RATIO_DEN);

			if constexpr(ASPECT_RATIO_NUM == ASPECT_RATIO_DEN)
			{
				uint32_t side = std::min(monitorWidth, monitorHeight);

				m_extent.width = side;
				m_extent.height = side;
			}
			else if constexpr (aspectRatio > 1)
			{
				m_extent.height = std::min(float(monitorHeight), monitorWidth / aspectRatio);
				m_extent.width = m_extent.height * aspectRatio;
			}
			else
			{
				m_extent.width = std::min(float(monitorWidth), monitorHeight * aspectRatio);
				m_extent.height = m_extent.width / aspectRatio;
			}
		}

		if(m_extent.width == 0)
			m_extent.width = (uint32_t)monitorWidth;

		if(m_extent.height == 0)
			m_extent.height = (uint32_t)monitorHeight;

		glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

		m_window = glfwCreateWindow(m_extent.width, m_extent.height, m_name.c_str(), nullptr, nullptr);

		if (!m_window)
			LOG_ENGINE(critical, "could not create glfw window");

		glfwSetWindowSizeLimits(m_window, std::min(monitorWidth / 4, 8), std::min(monitorHeight / 4, 8), GLFW_DONT_CARE, GLFW_DONT_CARE);
		glfwSetWindowAspectRatio(m_window, ASPECT_RATIO_NUM, ASPECT_RATIO_DEN);

		/* make sure we stored the right resolution */
		{
			int winWidth, winHeight;
			glfwGetWindowSize(m_window, &winWidth, &winHeight);

			m_extent.width = (uint32_t)winWidth;
			m_extent.width = (uint32_t)winHeight;
		}

		if (m_logo_start + m_logo_end)
		{
			set_icon(&m_logo_start, &m_logo_end, false);
		}
		else if (!m_logo_path.empty())
		{
			set_icon(m_logo_path, false);
		}

		m_vsync = true;

		/* set window callbacks */
		{
			glfwSetWindowUserPointer(m_window, this);
			glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* glfwWindow, int width, int height)
			{
				LOG_ENGINE(trace, "resize callback | width: %d, height: %d", width, height);

				auto pWindow = static_cast<windows_window*>(glfwGetWindowUserPointer(glfwWindow));
				pWindow->resize(width, height);

				runtime::restart_counter();
			});

			glfwSetWindowCloseCallback(m_window, [](GLFWwindow* glfwWindow)
			{
				static_cast<windows_window*>(glfwGetWindowUserPointer(glfwWindow))->set_window_close();
			});

			glfwSetKeyCallback(m_window, [](GLFWwindow* glfwWindow, int key, int scancode, int action, int mods)
			{
				switch (action)
				{
					case GLFW_PRESS:
					{
						input::s_held_key_count++;
						input::s_active_input_type = input_type::key;

						engine_events::key_pressed.broadcast((key_code)key, 0);
						break;
					}
					case GLFW_RELEASE:
					{
						input::s_held_key_count--;

						if (input::s_held_key_count == 0UL && input::s_active_input_type == input_type::key)
							input::s_active_input_type = input_type::none;

						engine_events::key_released.broadcast((key_code)key);

						break;
					}
				}

				engine_events::key.broadcast((key_code)key, (input_state)action);
			});


			glfwSetMouseButtonCallback(m_window, [](GLFWwindow* glfwWindow, int button, int action, int mods)
			{
				engine_events::mouse_button_action.broadcast((mouse_button)button, (input_state)action);

				switch (action)
				{
					case GLFW_PRESS:
					{
						input::s_active_input_type = input_type::mouse_button;
						input::s_mouse_position_last_click = input::s_mouse_position;
						engine_events::mouse_button_pressed.broadcast((mouse_button)button);
						break;
					}
					case GLFW_RELEASE:
					{
						if (input::s_active_input_type == input_type::mouse_button)
							input::s_active_input_type = input_type::none;

						engine_events::mouse_button_released.broadcast((mouse_button)button);
						break;
					}
				}
			});

			glfwSetScrollCallback(m_window, [](GLFWwindow* glfwWindow, double xOffset, double yOffset)
			{
				engine_events::mouse_scrolled.broadcast(yOffset);
			});

			glfwSetCursorPosCallback(m_window, [](GLFWwindow* glfwWindow, double xPos, double yPos)
			{
				float x = (float)xPos; float y = (float)yPos;
				CONVERT_TO_VIEWPORT(x, y);
				input::s_mouse_position = { x, y };

				engine_events::mouse_moved.broadcast(x, y);
			});

			glfwSetWindowPosCallback(m_window, [](GLFWwindow* glfwWindow, int xpos, int ypos)
			{ 
				runtime::restart_counter();
				LOG_ENGINE(info, "Window pos changed [%d, %d]", xpos, ypos);
			});

			glfwSetWindowFocusCallback(m_window, [](GLFWwindow* glfwWindow, int focus)
			{ 
				auto pWindow = static_cast<linux_window*>(glfwGetWindowUserPointer(glfwWindow));
				pWindow->set_focused(focus);

				engine_events::change_focus.broadcast(focus);

				if(focus)
					runtime::restart_counter();

				LOG_ENGINE(info, "focus == %d", focus);
			});

			glfwSetWindowIconifyCallback(m_window, [](GLFWwindow* glfwWindow, int minimized)
			{
				auto pWindow = static_cast<linux_window*>(glfwGetWindowUserPointer(glfwWindow));
				pWindow->set_focused(bool(minimized));
				pWindow->set_minimized(bool(minimized));

				engine_events::window_minimize.broadcast(bool(minimized));

				if(minimized)
					runtime::restart_counter();
			});
		}
	}

	void windows_window::resize(uint32_t width, uint32_t height)
	{
		if (width + height == 0)
		{
			m_minimized = true;
			runtime::restart_counter();

			return;
		}

		if (extent2d(width, height) == runtime::viewport())
		{
			if (m_minimized)
			{
				/* fire maximized event */
			}

			m_minimized = false;
			runtime::restart_counter();
			return;
		}

		m_minimized = false;
		runtime::set_viewport(width, height);

		m_extent = { (uint32_t)width, (uint32_t)height };

		if (m_swapchain)
			m_swapchain->on_resize(width, height);

		engine_events::window_resize.broadcast(width, height);
		engine_events::viewport_resize.broadcast(width, height);

		runtime::restart_counter();
	}

	void windows_window::poll_events()
	{
		glfwPollEvents();
	}

	void windows_window::update()
	{
	}

	void windows_window::swap_buffers()
	{
		m_swapchain->present(runtime::s_current_frame);
	}

	void windows_window::create_swapchain(bool useVSync)
	{
		if (!m_swapchain)
			m_swapchain = std::make_shared<swapchain>();

		swapchain_properties properties;
		properties.extent = m_extent;
		properties.vsync = useVSync;
		properties.use_depth = false;
		properties.prefer_mailbox_mode = false;
		/* the most common sRGB format on desktop */
		properties.desired_surface_format = VK_FORMAT_B8G8R8A8_SRGB;
		/* garanteed on desktop */
		properties.desired_composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		m_swapchain->create_surface(m_window, properties);
		m_swapchain->create(m_extent, useVSync);

		m_vsync = useVSync;

		if (!m_vsync)
		{	
			if (!m_swapchain->supports_nonvsync_mode())
			{
				m_vsync = true;
				LOG_ENGINE(warn, "non vsync mode asked but not supported");
			}
		}
	}

	void windows_window::destroy_swapchain()
	{
		if (m_swapchain)
		{
			m_swapchain->wait_for_cmds();
			m_swapchain->terminate();
			m_swapchain.reset();

			LOG_ENGINE(info, "destroyed swapchain");
		}
	}

	void windows_window::set_vsync(bool enable)
	{
		if (m_vsync == enable)
			return;

		if (!enable)
		{
			if (!m_swapchain->supports_nonvsync_mode())
			{
				LOG_ENGINE(warn, "non vsync mode asked but not supported");
				assert(m_vsync);
				return;
			}
		}

		m_vsync = enable;

		m_swapchain->wait_for_cmds();
		create_swapchain(m_vsync);
	}

	bool windows_window::supports_nonvsync_mode() const
	{
		return m_swapchain->supports_nonvsync_mode();
	}

	/*-------------logo--------------------------------------------------------*/

	void windows_window::set_icon(const int8_t* binaryStart, const int8_t* binaryEnd, bool flip /*=false*/)
	{
		int32_t width, height, bpp;
		stbi_set_flip_vertically_on_load(flip);
		uint8_t* icon_bytes_array = stbi_load_from_memory((uint8_t*)binaryStart, (binaryEnd - binaryStart), &width, &height, &bpp, 4);

		GLFWimage icon = { width, height, icon_bytes_array };
		glfwSetWindowIcon(m_window, 1, &icon);

		stbi_image_free(icon_bytes_array);
	}

	void windows_window::set_icon(const std::string& path, bool flip /*=false*/)
	{
		int32_t width, height, bpp;
		stbi_set_flip_vertically_on_load(flip);
		uint8_t* icon_bytes_array = stbi_load(path.c_str(), &width, &height, &bpp, 4);

		GLFWimage icon = { width, height, icon_bytes_array };
		glfwSetWindowIcon(m_window, 1, &icon);

		stbi_image_free(icon_bytes_array);

	}

	/*-------------private-----------------------------------------------------*/

	void windows_window::request_restore()
	{
		if (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED))
		{
			glfwRestoreWindow(m_window);
		}
		else
		{
			glfwMaximizeWindow(m_window);
		}
		//auto hwnd = glfwGetWin32Window(m_Window);
		//ShowWindow(hwnd, SW_RESTORE);
	}

	void windows_window::request_minimize()
	{
		auto hwnd = glfwGetWin32Window(m_window);
		ShowWindow(hwnd, SW_SHOWMINIMIZED);
	}


	void windows_window::set_cursor_type(cursor_type type)
	{
		::SetCursor(LoadCursor(NULL, (LPCSTR)(uint32_t)type));
	}

	std::string windows_window::open_file(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(m_window);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::string();
	}

	std::string windows_window::save_file(const char* filter)
	{
		OPENFILENAMEA ofn;
		CHAR szFile[260] = { 0 };
		CHAR currentDir[256] = { 0 };
		ZeroMemory(&ofn, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = glfwGetWin32Window(m_window);
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		if (GetCurrentDirectoryA(256, currentDir))
			ofn.lpstrInitialDir = currentDir;
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		// Sets the default extension by extracting it from the filter
		ofn.lpstrDefExt = strchr(filter, '\0') + 1;

		if (GetSaveFileNameA(&ofn) == TRUE)
			return ofn.lpstrFile;

		return std::string();
	}

}

#endif /* APP_WINDOWS */