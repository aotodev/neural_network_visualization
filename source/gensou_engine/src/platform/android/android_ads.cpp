#include "platform/android/android_ads.h"
#include "platform/android/android_jni.h"

#include "core/log.h"
#include "core/event.h"


static gs::ads::ad_state s_rewarded_video_ad_state = gs::ads::ad_state::not_init;
static gs::ads::ad_reward_fn s_reward_action = nullptr;

gs::ads::ad_state gs::ads::get_rewarded_video_ad_state() { return s_rewarded_video_ad_state; }

void gs::ads::set_on_reward_action(gs::ads::ad_reward_fn action)
{
	s_reward_action = action;
}

void gs::ads::clear_on_reward_action()
{
	s_reward_action = nullptr;
}

static void on_video_ad_reward(bool receivedReward)
{
	if(s_reward_action)
		s_reward_action(receivedReward);
}

#ifdef APP_ANDROID

#include "core/system.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>

#define CALL_VOID_JNI_METHOD(name)										\
auto androidApp = (android_app*)system::get_platform_data();			\
if (!androidApp)														\
return;																	\
																		\
JNIEnv* pJNIEnv;														\
androidApp->activity->vm->AttachCurrentThread(&pJNIEnv, NULL);			\
jclass clazz = pJNIEnv->GetObjectClass(androidApp->activity->clazz);	\
jmethodID methodID = pJNIEnv->GetMethodID(clazz, #name, "()V");		    \
																		\
if (!methodID)															\
return;																	\
																		\
pJNIEnv->CallVoidMethod(androidApp->activity->clazz, methodID);			\
androidApp->activity->vm->DetachCurrentThread();						\

extern "C" {
    JNIEXPORT void JNICALL Java_art_acquarelli_engine_GameActivity_SetAdState(JNIEnv *env, jobject thiz, int state)
    {
		LOG_ENGINE(warn, "calling native SetAdState");
		assert(state > 0 && state < 4);
		s_rewarded_video_ad_state = (gs::ads::ad_state)state;
    }

    JNIEXPORT void JNICALL Java_art_acquarelli_engine_GameActivity_CallOnVideoAdReward(JNIEnv *env, jobject thiz, jboolean receivedReward)
    {
		on_video_ad_reward(receivedReward);
    }
}

namespace gs::ads {

	void load_banner_ad()
	{
		/* not implemented */
	}

	void show_banner_ad()
	{
		/* not implemented */
	}

	void load_interstitial_ad()
	{
		/* not implemented */
	}

	void show_interstitial_ad()
	{
		/* not implemented */
	}

	void load_rewarded_video_ad()
	{
        jni::call_void_method("LoadRewardedVideoAd");
	}
	
	void show_rewarded_video_ad()
	{
        jni::call_void_method("ShowRewardedVideoAd");
	}

	void load_all()
	{
		load_banner_ad();
		load_interstitial_ad();
		load_rewarded_video_ad();
	}

}

#else
namespace gs::ads {

	void load_all() {}

	void load_banner_ad() {}
	void show_banner_ad() {}

	void load_interstitial_ad() {}
	void show_interstitial_ad() {}

	void load_rewarded_video_ad() { LOG_ENGINE(trace, "loading rewarded ad"); }
	void show_rewarded_video_ad() { LOG_ENGINE(info, "showing rewarded ad"); }

}

#endif /* APP_ANDROID */