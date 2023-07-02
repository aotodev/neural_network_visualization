#pragma once

#if defined(APP_WINDOWS)
#include "platform/windows/windows_platform.h"
#elif defined(APP_ANDROID)
#include "platform/android/android_platform.h"
#include "platform/android/android_ads.h"
#endif