#include "platform/android/android_platform.h"

#ifdef APP_ANDROID

#include "core/core.h"
#include "core/log.h"
#include "core/system.h"
#include "core/gensou_app.h"
#include "core/runtime.h"
#include "core/input.h"
#include "core/engine_events.h"

#include "renderer/device.h"
#include "renderer/swapchain.h"
#include "renderer/renderer.h"


#include <android/set_abort_message.h>

#include <stb_image.h>
#include <glm/glm.hpp>

/* JNI native functions (to be called by java code) */
static bool s_splash_video_finished = false;

extern "C" {
    JNIEXPORT void JNICALL Java_art_acquarelli_engine_GameActivity_SetVideoSplashFinished(JNIEnv *env, jobject thiz)
    {
        s_splash_video_finished = true;
        if (auto app = gs::gensou_app::get())
        {
            LOG_ENGINE(trace, "calling native method after video ended");
			static_cast<gs::android_window*>(app->get_window())->is_window_ready = true;
        }
    }

	JNIEXPORT void JNICALL Java_art_acquarelli_engine_GameActivity_NativeRequestDestroy(JNIEnv *env, jobject thiz)
	{
		LOG_ENGINE(warn, "calling native setVideoSpashFinished");
		s_splash_video_finished = true;
		if (auto app = gs::gensou_app::get())
		{
			LOG(trace, "calling native request_destroy");
			gs::system::exit();
		}
	}

	JNIEXPORT void JNICALL Java_art_acquarelli_engine_GameActivity_NativeOnPinchScale(JNIEnv *env, jobject thiz, float scale)
	{
        LOG_ENGINE(info, "native pinch_scale broadcasting");
		gs::engine_events::pinch_scale.broadcast(scale);
	}
}

//---------------------------------------------------------------------------------------------
// EXTERN FUNCTIONS
//---------------------------------------------------------------------------------------------

VkSurfaceKHR create_vulkan_surface(void* window)
{
	VkSurfaceKHR surface;

	VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;

	surfaceCreateInfo.window = (ANativeWindow*)window;

	VkResult result = vkCreateAndroidSurfaceKHR(gs::device::get_instance(), &surfaceCreateInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		gs::engine_events::vulkan_result_error.broadcast(result, "could not create an Android surface for presenting");

	return surface;
}

extern const char* get_platform_surface_ext()
{
	return "VK_KHR_android_surface"; //VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
}

namespace gs {

	bool input::is_pressed(key_code key)
	{
		return false;
	}

	void system::error_msg(const std::string& message)
	{
		if (android_app* app = static_cast<android_app*>(system::get_platform_data()))
		{
			JNIEnv* jni;
			app->activity->vm->AttachCurrentThread(&jni, NULL);

			/* memory allocation */
			jstring jmessage = jni->NewStringUTF(message.c_str());

			jclass clazz = jni->GetObjectClass(app->activity->clazz);

			/* Signature has to match java implementation (arguments) */
			jmethodID methodID = jni->GetMethodID(clazz, "ShowMessage", "(Ljava/lang/String;)V");
			jni->CallVoidMethod(app->activity->clazz, methodID, jmessage);

			/* dealocate any dynamically allocated memory */
			jni->DeleteLocalRef(jmessage);

			app->activity->vm->DetachCurrentThread();
			return;
		}
	}

	void system::rumble()
	{
		if (!s_rumbler_active)
			return;

		if (android_app* app = static_cast<android_app*>(system::get_platform_data()))
		{
			JNIEnv* pJNIEnv = nullptr;
			app->activity->vm->AttachCurrentThread(&pJNIEnv, NULL);

			if (pJNIEnv == nullptr)
			{
				LOG_ENGINE(error, "could not get JNIEnv");
				assert(false);
			}

			jclass clazz = pJNIEnv->GetObjectClass(app->activity->clazz);
			jmethodID methodID = pJNIEnv->GetMethodID(clazz, "Vibrate", "()V");
			if (!methodID) return;
			pJNIEnv->CallVoidMethod(app->activity->clazz, methodID);
			app->activity->vm->DetachCurrentThread();
		}
	}


	android_window::android_window(const std::string& name, uint32_t width, uint32_t height)
		: m_name(name), m_extent(width, height)
	{
		LOG_ENGINE(trace, "Calling android_window constructor");
	}

	android_window::~android_window()
	{
		destroy_swapchain();

		LOG_ENGINE(trace, "destroyed android_window");
	}

	window* window::create(const window_properties& properties)
	{
		return new android_window(properties);
	}

    void* android_window::get() const
    {
        if(auto androidApp = (android_app*)system::get_platform_data())
        {
            return androidApp->window;
        }
        else
        {
            LOG_ENGINE(error, "android_window::get() | failed to get android_app");
            return nullptr;
        }
    }

	void android_window::init()
	{
		m_focused = true;
		m_should_close_window = false;
		m_vsync = true;
		is_window_ready = false;

		if (auto androidApp = (android_app*)system::get_platform_data())
		{
            if(!androidApp->window)
            {
                LOG_ENGINE(error, "android_app->window was NULL, failed to init window");
                return;
            }

			m_extent.width = ANativeWindow_getWidth(androidApp->window);
			m_extent.height = ANativeWindow_getHeight(androidApp->window);

			if(m_extent != runtime::viewport())
			{
				LOG_ENGINE(info, "window size from init ([%u, %u]) not the same as the current viewport([%u, %u]), firing resize event", m_extent.width, m_extent.height, runtime::viewport().width, runtime::viewport().height);
				runtime::set_viewport(m_extent.width, m_extent.height);

				engine_events::window_resize.broadcast(m_extent.width, m_extent.height);
				engine_events::viewport_resize.broadcast(m_extent.width, m_extent.height);

				runtime::restart_counter();
			}
		}
		else
		{
			LOG_ENGINE(warn, "android_app was null while attempting to create a android window");
		}

		LOG_ENGINE(info, "successfully completed android_window::Init()");
	}

	void android_window::resize(uint32_t width, uint32_t height)
	{
		if ((width + height == 0) /* || (extent2d(width, height) == runtime::viewport())*/)
		{
			runtime::restart_counter();
			return;
		}

		m_extent.width = width;
		m_extent.height = height;

		runtime::set_viewport(width, height);

		if (m_swapchain)
			m_swapchain->on_resize(width, height);

		engine_events::window_resize.broadcast(width, height);
		engine_events::viewport_resize.broadcast(width, height);

		runtime::restart_counter();
	}

	void android_window::poll_events()
	{
		auto androidApp = (android_app*)system::get_platform_data();

		if (!androidApp)
			return;

		int events;
		struct android_poll_source* source = nullptr;

		while ((ALooper_pollAll(m_focused ? 0 : -1, NULL, &events, (void**)&source)) >= 0)
		{
			if (source != nullptr)
				source->process(androidApp, source);

			if (androidApp->destroyRequested != 0)
			{
				LOG_ENGINE(info, "Android app destroy requested");
				m_should_close_window = true;
				break;
			}
		}

		if (m_should_close_window)
			ANativeActivity_finish(androidApp->activity);
	}

	void android_window::request_destroy()
	{
        auto androidApp = (android_app*)system::get_platform_data();

		m_should_close_window = true;
		ANativeActivity_finish(androidApp->activity);
	}

	void android_window::update()
	{
	}

	void android_window::swap_buffers()
	{
		m_swapchain->present(runtime::s_current_frame);
	}

	void android_window::create_swapchain(bool useVSync)
	{
		auto androidApp = (android_app*)system::get_platform_data();
		if(!androidApp)
		{
			LOG_ENGINE(error, "android_app was NULL, could not create swapchain");
			return;
		}

        if(!androidApp->window)
		{
			LOG_ENGINE(error, "android_app->window was NULL, could not create swapchain");
			return;
		}
		
		if (!m_swapchain)
			m_swapchain = std::make_shared<swapchain>();

		swapchain_properties properties;
		properties.extent = m_extent;
		properties.vsync = useVSync;
		properties.use_depth = false;
		properties.prefer_mailbox_mode = true; // may improve FPS at the expense of increase battery drain
		properties.desired_surface_format = VK_FORMAT_R8G8B8A8_SRGB; // the only sRGB format garanteed on android
		properties.desired_composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; // the only one availabel on android

		m_swapchain->create_surface(androidApp->window, properties);
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

	void android_window::destroy_swapchain()
	{
		if (m_swapchain)
		{
			m_swapchain->wait_for_cmds();
			m_swapchain->terminate();
			m_swapchain.reset();

            LOG_ENGINE(info, "destroyed swapchain");
		}
	}

	void android_window::set_vsync(bool enable)
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

	bool android_window::supports_nonvsync_mode() const
	{
		return m_swapchain->supports_nonvsync_mode();
	}

	/*--ANDROID-CALLBACKS---------------------------------------------------------*/

	void android_window::handle_cmd(android_app* inApp, int32_t inCmd)
	{
		assert(inApp->userData);
		gensou_app* gensou = (gensou_app*)inApp->userData;

		switch (inCmd)
		{
			case APP_CMD_SAVE_STATE:
			{
				/* APP_CMD_SAVE_STATE command is fired when the app loses focus */
				LOG_ENGINE(info, "APP_CMD_SAVE_STATE");
                engine_events::save_state.broadcast();

				if (inApp->savedState) { /* ... */ }
				break;
			}

			case APP_CMD_INIT_WINDOW:
			{
				system::set_platform_data(inApp);

				if (inApp->window != nullptr)
				{
					LOG_ENGINE(warn, "APP_CMD_INIT_WINDOW running application");
					
					auto pWindow = dynamic_cast<android_window*>(gensou->get_window());
					//if(pWindow)
						//pWindow->init();

					gensou->init();
					gensou->start();

					if (s_splash_video_finished)
					{
						if (pWindow)
							pWindow->is_window_ready = true;
					}

					runtime::set_focused(true);

					LOG_ENGINE(warn, "APP_CMD_INIT_WINDOW finished application");
				}
				else
				{
					LOG_ENGINE(error, "APP_CMD_INIT_WINDOW Native window was null");
				}
				break;
			}

			case APP_CMD_WINDOW_RESIZED:
			{
				uint32_t x = ANativeWindow_getWidth(inApp->window);
				uint32_t y = ANativeWindow_getHeight(inApp->window);

				LOG_ENGINE(trace, "resize callback with value [%u, %u] | past values [%u, %u]", x, y, rt::viewport().width, rt::viewport().height);

				if (auto pWindow = dynamic_cast<android_window*>(gensou->get_window()))
					pWindow->resize(x, y);

				break;
			}

            case APP_CMD_WINDOW_REDRAW_NEEDED:
            {
                LOG_ENGINE(warn, "APP_CMD_WINDOW_REDRAW_NEEDED");
                break;
            }

            case APP_CMD_CONTENT_RECT_CHANGED:
            {
				uint32_t x = std::abs(inApp->contentRect.right - inApp->contentRect.left);
				uint32_t y = std::abs(inApp->contentRect.bottom - inApp->contentRect.top);

				LOG_ENGINE(trace, "APP_CMD_CONTENT_RECT_CHANGED %u x %u", x, y);
				LOG_ENGINE(trace, "past values ==  %u x %u", runtime::s_viewport.width, runtime::s_viewport.height);

				break;
            }

			case APP_CMD_LOST_FOCUS:
			{
				LOG_ENGINE(trace, "Application lost focus");
                runtime::set_focused(false);
                static_cast<android_window*>(gensou->get_window())->set_focused(false);
                engine_events::change_focus.broadcast(false);
                break;
			}

			case APP_CMD_GAINED_FOCUS:
			{
				LOG_ENGINE(trace, "application gained focus");
                runtime::set_focused(true);
                static_cast<android_window*>(gensou->get_window())->set_focused(true);
                engine_events::change_focus.broadcast(true);
				runtime::restart_counter();
                break;
			}

			case APP_CMD_START:
			{
				LOG_ENGINE(info , "APP_CMD_START");
				break;
			}

			case APP_CMD_PAUSE:
			{
				LOG_ENGINE(info, "APP_CMD_PAUSE");
				engine_events::window_minimize.broadcast(true);
				break;
			}

			case APP_CMD_RESUME:
			{
				LOG_ENGINE(info, "APP_CMD_RESUME");
				engine_events::window_minimize.broadcast(false);
				/* done in the java side */
				//static_cast<android_window*>(gensou->get_window())->restore_transparent_bars();
				break;
			}

			case APP_CMD_STOP:
			{
				break;
			}

			case APP_CMD_TERM_WINDOW:
			{
				runtime::set_focused(false);

				if (android_window* pWindow = dynamic_cast<android_window*>(gensou->get_window()))
				{
					pWindow->is_window_ready = false;
				}
				break;
			}

			case APP_CMD_DESTROY:
			{
				if (android_window* pWindow = dynamic_cast<android_window*>(gensou->get_window()))
				{
					pWindow->set_window_close();
				}
				break;
			}

			case APP_CMD_LOW_MEMORY:
			{
				break;
			}
		}
	}

	int32_t android_window::handle_input(android_app* inApp, AInputEvent* inEvent)
	{
		if (AInputEvent_getType(inEvent) == AINPUT_EVENT_TYPE_MOTION)
		{
			int32_t eventSource = AInputEvent_getSource(inEvent);
			switch (eventSource)
			{
				case AINPUT_SOURCE_TOUCHSCREEN:
				{
					int32_t action = AMotionEvent_getAction(inEvent);
					size_t pointerCount = AMotionEvent_getPointerCount(inEvent);

					/* more complicated gestures will be handled in the java side */
					if(pointerCount > 1)
						return 0;

					switch (action & AMOTION_EVENT_ACTION_MASK)
					{
						case AMOTION_EVENT_ACTION_POINTER_DOWN:
						{
							int32_t iIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
							float x = AMotionEvent_getX(inEvent, iIndex); // default 0
							float y = AMotionEvent_getY(inEvent, iIndex);

							CONVERT_TO_VIEWPORT(x, y);

							input::s_active_input_type = input_type::touch;
							input::s_touch_position = { x, y };
							input::s_position_on_last_touchDown = { x, y };

							engine_events::touch_down.broadcast(x, y);
							break;
						}

						case AMOTION_EVENT_ACTION_DOWN:
						{
							float x = AMotionEvent_getX(inEvent, 0);
							float y = AMotionEvent_getY(inEvent, 0);

							CONVERT_TO_VIEWPORT(x, y);

							input::s_active_input_type = input_type::touch;
							input::s_touch_position = { x, y };
							input::s_position_on_last_touchDown = { x, y };

							engine_events::touch_down.broadcast(x, y);
							break;
						}

						case AMOTION_EVENT_ACTION_UP:
						{
							float x = AMotionEvent_getX(inEvent, 0);
							float y = AMotionEvent_getY(inEvent, 0);

							if (input::s_active_input_type == input_type::touch)
								input::s_active_input_type = input_type::none;

							CONVERT_TO_VIEWPORT(x, y);
							engine_events::touch_up.broadcast(x, y);
							break;
						}

						case AMOTION_EVENT_ACTION_POINTER_UP:
						{
							//float x = AMotionEvent_getX(inEvent, pointerCount - 1);
							//float y = AMotionEvent_getY(inEvent, pointerCount - 1);
							//CONVERT_TO_VIEWPORT(x, y);
							//engine_events::touch_up.broadcast(x, y);
							break;
						}

						case AMOTION_EVENT_ACTION_MOVE:
						{
							float x = AMotionEvent_getX(inEvent, pointerCount - 1);
							float y = AMotionEvent_getY(inEvent, pointerCount - 1);

							CONVERT_TO_VIEWPORT(x, y);
							input::s_touch_position = { x, y };

							engine_events::touch_move.broadcast(x, y);
							break;
						}

						default:
							break;
					}
					break;
				}

				case AINPUT_SOURCE_JOYSTICK:
				{
					break;
				}
				default:
					break;
			}
			// was handled
			return 1;
		}

		/* was not handled */
		return 0;
	}

	/*--JNI-METHODS---------------------------------------------------------------*/

	void android_window::dialog_box(const std::string& message)
	{
		auto androidApp = (android_app*)system::get_platform_data();

		if (!androidApp)
			return;

		JNIEnv* jni;
		androidApp->activity->vm->AttachCurrentThread(&jni, NULL);

		/* memory allocation */
		jstring jmessage = jni->NewStringUTF(message.c_str());

		jclass clazz = jni->GetObjectClass(androidApp->activity->clazz);

		/* Signature has to match java implementation (arguments) */
		jmethodID methodID = jni->GetMethodID(clazz, "ShowMessage", "(Ljava/lang/String;)V");
		jni->CallVoidMethod(androidApp->activity->clazz, methodID, jmessage);

		/* dealocate any dynamically allocated memory */
		jni->DeleteLocalRef(jmessage);

		androidApp->activity->vm->DetachCurrentThread();
		return;
	}

    uint32_t android_window::get_display_cutout_height()
    {
        auto androidApp = (android_app*)system::get_platform_data();

        if (!androidApp)
        {
            LOG_ENGINE(error, "androidApp was NULL. Returning 0");
            return 0;
        }

        JNIEnv* jni;
        androidApp->activity->vm->AttachCurrentThread(&jni, NULL);

        jclass clazz = jni->GetObjectClass(androidApp->activity->clazz);
        jmethodID methodID = jni->GetMethodID(clazz, "GetDisplayCutoutHeight", "()I");

        if(!methodID)
        {
            LOG_ENGINE(error, "failed to get GetDisplayCutoutHeight JNI method");
            androidApp->activity->vm->DetachCurrentThread();
            return 0;
        }

        int cutout = jni->CallIntMethod(androidApp->activity->clazz, methodID);

        androidApp->activity->vm->DetachCurrentThread();

		return uint32_t(cutout);
    }

	void android_window::restore_transparent_bars()
	{
		auto androidApp = (android_app*)system::get_platform_data();

		if (!androidApp)
			return;

		JNIEnv* jni;
		androidApp->activity->vm->AttachCurrentThread(&jni, NULL);

		jclass clazz = jni->GetObjectClass(androidApp->activity->clazz);
		jmethodID methodID = jni->GetMethodID(clazz, "RestoreTransparentBars", "()V");
		jni->CallVoidMethod(androidApp->activity->clazz, methodID);

		androidApp->activity->vm->DetachCurrentThread();
		return;
	}

	void android_window::destroy_application()
	{
		auto androidApp = (android_app*)system::get_platform_data();

		if (!androidApp)
			return;

		JNIEnv* jni;
		androidApp->activity->vm->AttachCurrentThread(&jni, NULL);

		jclass clazz = jni->GetObjectClass(androidApp->activity->clazz);
		jmethodID methodID = jni->GetMethodID(clazz, "DestroyApplication", "()V");
		jni->CallVoidMethod(androidApp->activity->clazz, methodID);

		androidApp->activity->vm->DetachCurrentThread();
	}

    window_properties window_properties::get_default(uint32_t width, uint32_t height)
    {
        window_properties outProperties{
            GAME_NAME,
            width, height,
            float(ASPECT_RATIO_NUM) / float(ASPECT_RATIO_DEN),
            0, 0,
            "engine_res/textures/logo_small.png"
        };

		if(width > 0 && height > 0)
		{
			outProperties.aspect_ratio = float(width) / float(height);
			return outProperties;
		}

        auto androidApp = (android_app*)system::get_platform_data();

        if (!androidApp)
        {
            LOG_ENGINE(error, "androidApp was NULL. Returning default window properties");
            return outProperties;
        }

        JNIEnv* jni;
        androidApp->activity->vm->AttachCurrentThread(&jni, NULL);

        jclass clazz = jni->GetObjectClass(androidApp->activity->clazz);
        jmethodID widthMethodID = jni->GetMethodID(clazz, "GetDisplayWidth", "()I");
        jmethodID heightMethodID = jni->GetMethodID(clazz, "GetDisplayHeight", "()I");

        if(!widthMethodID || !heightMethodID)
        {
            LOG_ENGINE(error, "failed to get GetDisplay_ JNI methods");
            androidApp->activity->vm->DetachCurrentThread();
            return outProperties;
        }

        int displayWidth = jni->CallIntMethod(androidApp->activity->clazz, widthMethodID);
        int displayHeight = jni->CallIntMethod(androidApp->activity->clazz, heightMethodID);

        androidApp->activity->vm->DetachCurrentThread();

        if(displayWidth <= 0 || displayHeight <= 0)
        {
            LOG_ENGINE(error, "display width and/or height retrieved from JNI were invalid (values == [%d, %d]). returning default window properties", displayWidth, displayHeight);
            return outProperties;
        }

        outProperties.width = (uint32_t)displayWidth;
        outProperties.height = (uint32_t)displayHeight;
        outProperties.aspect_ratio = float(displayWidth) / float(displayHeight);

        return outProperties;
    }

}

#endif /*APP_ANDROID */