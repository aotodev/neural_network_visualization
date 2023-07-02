#pragma once

#ifdef APP_ANDROID

#include "core/system.h"
#include "core/log.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>

namespace gs::jni {

    /* this helper function will only work with primitive types (both parameters and return type)
     * for other types, such as strings, you'll need to do the JNI calls yourself */

    /* about jtypes:
     * jbyte    == int8_t
     * jboolean == uint8_t
     * jshort   == int16_t
     * jchar    == uint16_t // java uses Unicode!
     * jlong    == int64_t
     * jfloat   == float
     * jdouble  == double
     *
     * jobjects, jmethodID, etc are typedef pointers to opaque types much like vulkan's VkDescriptorSet, etc
     **/

    template<typename... Args>
    static inline void call_void_method(const std::string& methodName, const std::string& methodSignature, Args&&... args)
    {
        auto androidApp = (android_app*)system::get_platform_data();
        if (!androidApp)
        {
            LOG_ENGINE(error, "failed to retrieve a valid reference to the android_app");
            return;
        }

        JNIEnv* pJNIEnv;
        androidApp->activity->vm->AttachCurrentThread(&pJNIEnv, NULL);
        jclass clazz = pJNIEnv->GetObjectClass(androidApp->activity->clazz);
        jmethodID methodID = pJNIEnv->GetMethodID(clazz, methodName.c_str(), methodSignature.c_str());

        if (!methodID)
        {
            LOG_ENGINE(error, "failed to retrieve JNI method with name '%s'", methodName.c_str());
            return;
        }

        pJNIEnv->CallVoidMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);
        androidApp->activity->vm->DetachCurrentThread();
    }

    static inline void call_void_method(const std::string& methodName)
    {
        call_void_method(methodName, "()V");
    }

    template<typename T, typename... Args>
    static inline T call_method(const std::string& methodName, const std::string& methodSignature, Args&&...args)
    {
        auto androidApp = (android_app*)system::get_platform_data();
        if (!androidApp)
        {
            LOG_ENGINE(error, "failed to retrieve a valid reference to the android_app");
            return (T)0;
        }

        JNIEnv* pJNIEnv;
        androidApp->activity->vm->AttachCurrentThread(&pJNIEnv, NULL);
        jclass clazz = pJNIEnv->GetObjectClass(androidApp->activity->clazz);
        jmethodID methodID = pJNIEnv->GetMethodID(clazz, methodName.c_str(), methodSignature.c_str());

        if (!methodID)
        {
            LOG_ENGINE(error, "failed to retrieve JNI method with name '%s'", methodName.c_str());
            return (T)0;
        }

        if constexpr(std::is_void_v<T>)
        {
            pJNIEnv->CallVoidMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);
            androidApp->activity->vm->DetachCurrentThread();
            return;
        }

        T out = T(0);

        if constexpr(std::is_same_v<T, int8_t>)
            out = (int8_t)pJNIEnv->CallByteMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, uint8_t>)
            out = (uint8_t)pJNIEnv->CallBooleanMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, uint16_t>)
            out = (uint16_t)pJNIEnv->CallCharMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, int16_t>)
            out = (int16_t)pJNIEnv->CallShortMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, int> || std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>)
            out = (T)pJNIEnv->CallIntMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, long long> || std::is_same_v<T, int64_t> || std::is_same_v<T, uint64_t>)
            out = (T)pJNIEnv->CallLongMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, float>)
            out = (float)pJNIEnv->CallFloatMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        if constexpr(std::is_same_v<T, double>)
            out = (double)pJNIEnv->CallDoubleMethod(androidApp->activity->clazz, methodID, std::forward<decltype(args)>(args)...);

        androidApp->activity->vm->DetachCurrentThread();

        return out;
    }

}

#endif