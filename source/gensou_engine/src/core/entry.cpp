#include "core/core.h"
#include "core/log.h"
#include "core/system.h"
#include "core/gensou_app.h"


#if defined(APP_WINDOWS)
#define NOMINMAX 
#include <windows.h>

#if defined(APP_SHIPPING)

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	gs::system::set_platform_data(hInst);
	gs::gensou_app* gensouApp = gs::gensou_app::create();

	gensouApp->init();
	gensouApp->start();
	gensouApp->run();
	gensouApp->destroy();
	
	return 0;
}
#else
int main(int argc, char** argv)
{
	auto hInst = GetModuleHandle(nullptr);
	gs::system::set_platform_data(hInst);

	gs::gensou_app* gensouApp = gs::gensou_app::create();

	gensouApp->init();
	gensouApp->start();
	gensouApp->run();
	gensouApp->destroy();

	return 0;
}
#endif // APP_SHIPPING

#elif defined(APP_ANDROID)

#include "platform/android/android_platform.h"
#include "platform/android/android_ads.h"

/* since ANativeActivity_onCreate is called from Java, most compilers will strip it out of the static lib
 * the following code will make sure this does not happen
 */

extern "C" void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);
void (*ANativeActivity_onCreateFn)(ANativeActivity*, void*, size_t) = &ANativeActivity_onCreate;

void android_main(android_app* pState)
{
	LOG_ENGINE(trace, "ENTRY POINT | starting '%s'", GAME_NAME);
	gs::system::set_platform_data(pState);
	gs::gensou_app* gensouApp = gs::gensou_app::create();

	pState->userData = gensouApp;
	pState->onAppCmd = gs::android_window::handle_cmd;
	pState->onInputEvent = gs::android_window::handle_input;

	//gs::ads::load_all();

	gensouApp->run();
	gensouApp->destroy();

	gs::android_window::destroy_application();
}

#elif defined(APP_LINUX)

int main(int argc, char** argv)
{
	gs::gensou_app* gensouApp = gs::gensou_app::create();

	gensouApp->init();
	gensouApp->start();
	gensouApp->run();
	gensouApp->destroy();

	return 0;
}

#else

#error unsuported platform

#endif
