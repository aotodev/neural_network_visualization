#include "platform/android/android_google_services.h"
#include "platform/android/android_jni.h"

#include "core/log.h"
#include "core/event.h"

#ifdef APP_ANDROID

#include "core/system.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>

namespace gs::google_services {

    void display_leaderboard()
    {
        LOG_ENGINE(trace, "displaying leaderboard");
        jni::call_void_method("ShowLeaderboard");
    }

    void update_leaderboard_score(uint32_t score)
    {
        LOG_ENGINE(trace, "updating leaderboard with score %u", score);
        jni::call_void_method("UpdateLeaderboardScore", "(I)V", (int)score);
    }
}

#else

namespace gs::google_services {

    void display_leaderboard() {}
    void update_leaderboard_score(uint32_t score) {}
}

#endif