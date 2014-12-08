#include <cstdint>
#include <cinttypes>

#include <string>
#include <locale>
#include <codecvt>
#include <array>
#include <vector>

#include <jni.h>

#include <android/native_window_jni.h>

#include <coffeecatch/coffeejni.h>

#include <mbgl/android/jni.hpp>
#include <mbgl/android/native_map_view.hpp>
#include <mbgl/map/map.hpp>
#include <mbgl/platform/android/log_android.hpp>
#include <mbgl/platform/event.hpp>
#include <mbgl/platform/log.hpp>

#pragma clang diagnostic ignored "-Wunused-parameter"

#define CPP_TRY_JNI(ENV, CODE)                          \
    try {                                               \
        CODE;                                           \
    } catch(std::exception e) {                         \
        std::string msg("Unhandled C++ exception: ");   \
        msg.append(e.what());                            \
        throw_error(ENV, msg.c_str());                  \
    }


namespace mbgl {
namespace android {

std::string cachePath;
std::string dataPath;
std::string apkPath;

jmethodID onMapChangedId = nullptr;
jmethodID onFpsChangedId = nullptr;

jclass lonLatClass = nullptr;
jmethodID lonLatConstructorId = nullptr;
jfieldID lonLatLonId = nullptr;
jfieldID lonLatLatId = nullptr;

jclass lonLatZoomClass = nullptr;
jmethodID lonLatZoomConstructorId = nullptr;
jfieldID lonLatZoomLonId = nullptr;
jfieldID lonLatZoomLatId = nullptr;
jfieldID lonLatZoomZoomId = nullptr;

jclass runtimeExceptionClass = nullptr;
jclass nullPointerExceptionClass = nullptr;

jmethodID listToArrayId = nullptr;

jclass arrayListClass = nullptr;
jmethodID arrayListConstructorId = nullptr;
jmethodID arrayListAddId = nullptr;

bool throw_error(JNIEnv* env, const char* msg) {
    if (env->ThrowNew(runtimeExceptionClass, msg) < 0) {
        env->ExceptionDescribe();
        return false;
    }

    return true;
}

std::string std_string_from_jstring(JNIEnv* env, jstring jstr) {
    std::string str;

    if (jstr == nullptr) {
        if (env->ThrowNew(nullPointerExceptionClass, "String cannot be null.") < 0) {
            env->ExceptionDescribe();
            return str;
        }

        return str;
    }

    jsize len = env->GetStringLength(jstr);
    if (len < 0) {
        env->ExceptionDescribe();
        return str;
    }

    const jchar* chars = env->GetStringChars(jstr, nullptr);
    if (chars == nullptr) {
        env->ExceptionDescribe();
        return str;
    }

    std::u16string ustr(reinterpret_cast<const char16_t*>(chars), len);
    env->ReleaseStringChars(jstr, chars);
    chars = nullptr;
    str = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes(ustr);
    return str;
}

jstring std_string_to_jstring(JNIEnv* env, std::string str) {
    std::u16string ustr = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().from_bytes(str);

    jstring jstr = env->NewString(reinterpret_cast<const jchar*>(ustr.c_str()), ustr.size());
    if (jstr == nullptr) {
        env->ExceptionDescribe();
        return nullptr;
    }

    return jstr;
}

std::vector<std::string> std_vector_string_from_jobject(JNIEnv* env, jobject jlist) {
    std::vector<std::string> vector;

    if (jlist == nullptr) {
        if (env->ThrowNew(nullPointerExceptionClass, "List cannot be null.") < 0) {
            env->ExceptionDescribe();
            return vector;
        }

        return vector;
    }

    jobjectArray array = reinterpret_cast<jobjectArray>(env->CallObjectMethod(jlist, listToArrayId));
    if (env->ExceptionCheck() || (array == nullptr)) {
        env->ExceptionDescribe();
        return vector;
    }

    jsize len = env->GetArrayLength(array);
    if (len < 0) {
        env->ExceptionDescribe();
        return vector;
    }

    for (jsize i = 0; i < len; i++) {
        jstring jstr = reinterpret_cast<jstring>(env->GetObjectArrayElement(array, i));
        if (jstr == nullptr) {
            env->ExceptionDescribe();
            return vector;
        }

        vector.push_back(std_string_from_jstring(env, jstr));
    }

    return vector;
}

jobject std_vector_string_to_jobject(JNIEnv* env, std::vector<std::string> vector)  {
    jobject jlist = env->NewObject(arrayListClass, arrayListConstructorId);
    if (jlist == nullptr) {
        env->ExceptionDescribe();
        return nullptr;
    }

    for (std::string str : vector) {
        env->CallBooleanMethod(jlist, arrayListAddId, std_string_to_jstring(env, str));
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            return nullptr;
        }
    }

    return jlist;
}

}
}

namespace {

using namespace mbgl::android;

jlong JNICALL nativeCreate(JNIEnv* env, jobject obj, jstring cachePath_, jstring dataPath_, jstring apkPath_) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeCreate");
    cachePath = std_string_from_jstring(env, cachePath_);
    dataPath = std_string_from_jstring(env, dataPath_);
    apkPath = std_string_from_jstring(env, apkPath_);
    NativeMapView* nativeMapView = nullptr;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView = new NativeMapView(env, obj)));
    if (nativeMapView == nullptr) {
        throw_error(env, "Unable to create NativeMapView.");
        return 0;
    }
    jlong mapViewPtr = reinterpret_cast<jlong>(nativeMapView);
    return mapViewPtr;
}

void JNICALL nativeDestroy(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeDestroy");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    delete nativeMapView;
    nativeMapView = nullptr;
}

void JNICALL nativeInitializeDisplay(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeInitializeDisplay");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    bool ret = false;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->initializeDisplay()));
    if (!ret) {
        throw_error(env, "Unable to initialize GL display.");
    }
}

void JNICALL nativeTerminateDisplay(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeTerminateDisplay");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->terminateDisplay()));
}

void JNICALL nativeInitializeContext(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeInitializeContext");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    bool ret = false;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->initializeContext()));
    if (!ret) {
        throw_error(env, "Unable to initialize GL context.");
    }
}

void JNICALL nativeTerminateContext(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeTerminateContext");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->terminateContext()));
}

void JNICALL nativeCreateSurface(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jobject surface) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeCreateSurface");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    bool ret = false;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->createSurface(ANativeWindow_fromSurface(env, surface))));
    if (!ret) {
        throw_error(env, "Unable to create GL surface.");
    }
}

void JNICALL nativeDestroySurface(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeDestroySurface");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->destroySurface()));
}

void JNICALL nativeStart(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStart");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->start()));
}

void JNICALL nativeStop(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStop");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->stop()));
}

void JNICALL nativePause(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativePause");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->pause()));
}

void JNICALL nativeResume(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResume");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->resume()));
}

void JNICALL nativeRun(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeRun");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().run()));
}

void JNICALL nativeRerender(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeRerender");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().rerender()));
}

void JNICALL nativeUpdate(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeUpdate");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().update()));
}

void JNICALL nativeTerminate(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeTerminate");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().terminate()));
}

jboolean JNICALL nativeNeedsSwap(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeNeedsSwap");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jboolean ret = false;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().needsSwap()));
    return ret;
}

void JNICALL nativeSwapped(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSwapped");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, (nativeMapView->getMap().swapped())));
}

void JNICALL nativeResize(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jint width, jint height, jfloat ratio) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResize");
    assert(nativeMapViewPtr != 0);
    assert(width >= 0);
    assert(height >= 0);
    assert(width <= UINT16_MAX);
    assert(height <= UINT16_MAX);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().resize(width, height, ratio)));
}

void JNICALL nativeResize(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jint width, jint height, jfloat ratio, jint fbWidth, jint fbHeight) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResize");
    assert(nativeMapViewPtr != 0);
    assert(width >= 0);
    assert(height >= 0);
    assert(width <= UINT16_MAX);
    assert(height <= UINT16_MAX);
    assert(fbWidth >= 0);
    assert(fbHeight >= 0);
    assert(fbWidth <= UINT16_MAX);
    assert(fbHeight <= UINT16_MAX);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().resize(width, height, ratio, fbWidth, fbHeight)));
}

void JNICALL nativeSetAppliedClasses(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jobject classes) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetAppliedClasses");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setAppliedClasses(std_vector_string_from_jobject(env, classes))));
}

jobject JNICALL nativeGetAppliedClasses(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetAppliedClasses");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jobject ret = nullptr;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = std_vector_string_to_jobject(env, nativeMapView->getMap().getAppliedClasses())));
    return ret;
}

void JNICALL nativeSetDefaultTransitionDuration(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jlong milliseconds) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetDefaultTransitionDuration");
    assert(nativeMapViewPtr != 0);
    assert(milliseconds >= 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setDefaultTransitionDuration(milliseconds)));
}

jlong JNICALL nativeGetDefaultTransitionDuration(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetDefaultTransitionDuration");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jlong ret = 0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getDefaultTransitionDuration()));
    return ret;
}

void JNICALL nativeSetStyleURL(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jstring url) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetStyleURL");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setStyleURL(std_string_from_jstring(env, url))));
}

void JNICALL nativeSetStyleJSON(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jstring newStyleJson, jstring base) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetStyleJSON");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setStyleJSON(std_string_from_jstring(env, newStyleJson), std_string_from_jstring(env, base))));
}

jstring JNICALL nativeGetStyleJSON(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetStyleJSON");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jstring ret = nullptr;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = std_string_to_jstring(env, nativeMapView->getMap().getStyleJSON())));
    return ret;
}

void JNICALL nativeSetAccessToken(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jstring accessToken) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetAccessToken");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getFileSource().setAccessToken(std_string_from_jstring(env, accessToken))));
}

jstring JNICALL nativeGetAccessToken(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetAccessToken");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jstring ret = nullptr;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = std_string_to_jstring(env, nativeMapView->getFileSource().getAccessToken())));
    return ret;
}

void JNICALL nativeCancelTransitions(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeCancelTransitions");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().cancelTransitions()));
}

void JNICALL nativeMoveBy(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble dx, jdouble dy, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeMoveBy");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().moveBy(dx, dy, duration)));
}

void JNICALL nativeSetLonLat(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jobject lonLat, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetLonLat");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);

    double lon = env->GetDoubleField(lonLat, lonLatLonId);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return;
    }

    double lat = env->GetDoubleField(lonLat, lonLatLatId);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return;
    }

    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setLonLat(lon, lat, duration)));
}

jobject JNICALL nativeGetLonLat(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetLonLat");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    double lon = 0.0, lat = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().getLonLat(lon, lat)));

    jobject ret = env->NewObject(lonLatClass, lonLatConstructorId, lon, lat);
    if (ret == nullptr) {
        env->ExceptionDescribe();
        return nullptr;
    }

    return ret;
}

void JNICALL nativeStartPanning(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStartPanning");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().startPanning()));
}

void JNICALL nativeStopPanning(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStopPanning");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().stopPanning()));
}

void JNICALL nativeResetPosition(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResetPosition");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().resetPosition()));
}

void JNICALL nativeScaleBy(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble ds, jdouble cx, jdouble cy, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeScaleBy");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().scaleBy(ds, cx, cy, duration)));
}

void JNICALL nativeSetScale(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble scale, jdouble cx, jdouble cy, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetScale");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setScale(scale, cx, cy, duration)));
}

jdouble JNICALL nativeGetScale(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetScale");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jdouble ret = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getScale()));
    return ret;
}

void JNICALL nativeSetZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble zoom, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setZoom(zoom, duration)));
}

jdouble JNICALL nativeGetZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jdouble ret = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getZoom()));
    return ret;
}

void JNICALL nativeSetLonLatZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jobject lonLatZoom, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetLonLatZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);

    double lon = env->GetDoubleField(lonLatZoom, lonLatZoomLonId);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return;
    }

    double lat = env->GetDoubleField(lonLatZoom, lonLatZoomLatId);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return;
    }

    double zoom = env->GetDoubleField(lonLatZoom, lonLatZoomZoomId);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return;
    }

    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setLonLatZoom(lon, lat, zoom, duration)));
}

jobject JNICALL nativeGetLonLatZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetLonLatZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    double lon = 0.0, lat = 0.0, zoom = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().getLonLatZoom(lon, lat, zoom)));

    jobject ret = env->NewObject(lonLatZoomClass, lonLatZoomConstructorId, lon, lat, zoom);
    if (ret == nullptr) {
        env->ExceptionDescribe();
        return nullptr;
    }

    return ret;
}

void JNICALL nativeResetZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResetZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().resetZoom()));
}

void JNICALL nativeStartScaling(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStartScaling");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().startScaling()));
}

void JNICALL nativeStopScaling(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStopScaling");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().stopScaling()));
}

jdouble JNICALL nativeGetMinZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetMinZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jdouble ret = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getMinZoom()));
    return ret;
}

jdouble JNICALL nativeGetMaxZoom(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetMaxZoom");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jdouble ret = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getMaxZoom()));
    return ret;
}

void JNICALL nativeRotateBy(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble sx, jdouble sy, jdouble ex, jdouble ey, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeRotateBy");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().rotateBy(sx, sy, ex, ey, duration)));
}

void JNICALL nativeSetBearing(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble degrees, jdouble duration) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetBearing");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setBearing(degrees, duration)));
}

void JNICALL nativeSetBearing(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jdouble degrees, jdouble cx, jdouble cy) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetBearing");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setBearing(degrees, cx, cy)));
}

jdouble JNICALL nativeGetBearing(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetBearing");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jdouble ret = 0.0;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getBearing()));
    return ret;
}

void JNICALL nativeResetNorth(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeResetNorth");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().resetNorth()));
}

void JNICALL nativeStartRotating(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStartRotating");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().startRotating()));
}

void JNICALL nativeStopRotating(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeStopRotating");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().stopRotating()));
}

void JNICALL nativeSetDebug(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jboolean debug) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetDebug");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().setDebug(debug); nativeMapView->enableFps(debug)));
}

void JNICALL nativeToggleDebug(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeToggleDebug");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getMap().toggleDebug(); nativeMapView->enableFps(nativeMapView->getMap().getDebug())));
}

jboolean JNICALL nativeGetDebug(JNIEnv* env, jobject obj, jlong nativeMapViewPtr) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeGetDebug");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    jboolean ret = false;
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, ret = nativeMapView->getMap().getDebug()));
    return ret;
}

void JNICALL nativeSetReachability(JNIEnv* env, jobject obj, jlong nativeMapViewPtr, jboolean status) {
    mbgl::Log::Debug(mbgl::Event::JNI, "nativeSetReachability");
    assert(nativeMapViewPtr != 0);
    NativeMapView* nativeMapView = reinterpret_cast<NativeMapView*>(nativeMapViewPtr);
    COFFEE_TRY_JNI(env, CPP_TRY_JNI(env, nativeMapView->getFileSource().setReachability(status)));
}

}

extern "C" {

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    mbgl::Log::Set<mbgl::AndroidLogBackend>();

    mbgl::Log::Debug(mbgl::Event::JNI, "JNI_OnLoad");

    JNIEnv* env = nullptr;
    jint ret = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (ret != JNI_OK) {
        mbgl::Log::Error(mbgl::Event::JNI, "GetEnv() failed with %i", ret);
        return JNI_ERR;
    }

    lonLatClass = env->FindClass("com/mapbox/mapboxgl/lib/LonLat");
    if (lonLatClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatConstructorId = env->GetMethodID(lonLatClass, "<init>", "(DD)V");
    if (lonLatConstructorId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatLonId = env->GetFieldID(lonLatClass, "lon", "D");
    if (lonLatLonId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatLatId = env->GetFieldID(lonLatClass, "lat", "D");
    if (lonLatLatId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomClass = env->FindClass("com/mapbox/mapboxgl/lib/LonLatZoom");
    if (lonLatClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomConstructorId = env->GetMethodID(lonLatZoomClass, "<init>", "(DDD)V");
    if (lonLatZoomConstructorId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomLonId = env->GetFieldID(lonLatZoomClass, "lon", "D");
    if (lonLatZoomLonId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomLatId = env->GetFieldID(lonLatZoomClass, "lat", "D");
    if (lonLatZoomLatId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomZoomId = env->GetFieldID(lonLatZoomClass, "zoom", "D");
    if (lonLatZoomZoomId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    jclass nativeMapViewClass = env->FindClass("com/mapbox/mapboxgl/lib/NativeMapView");
    if (nativeMapViewClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    onMapChangedId = env->GetMethodID(nativeMapViewClass, "onMapChanged", "()V");
    if (onMapChangedId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    onFpsChangedId = env->GetMethodID(nativeMapViewClass, "onFpsChanged", "(D)V");
    if (onFpsChangedId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    runtimeExceptionClass = env->FindClass("java/lang/RuntimeException");
    if (runtimeExceptionClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    nullPointerExceptionClass = env->FindClass("java/lang/NullPointerException");
    if (nullPointerExceptionClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    jclass listClass = env->FindClass("java/util/List");
    if (listClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    listToArrayId = env->GetMethodID(listClass, "toArray", "()[Ljava/lang/Object;");
    if (listToArrayId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    arrayListClass = env->FindClass("java/util/ArrayList");
    if (arrayListClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    arrayListConstructorId = env->GetMethodID(arrayListClass, "<init>", "()V");
    if (arrayListConstructorId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    arrayListAddId = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    if (arrayListAddId == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    // NOTE: if you get java.lang.UnsatisfiedLinkError you likely forgot to set the size of the array correctly (too large)
    std::array<JNINativeMethod, 59> methods = {{ // Can remove the extra brace in C++14
        { "nativeCreate", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J", reinterpret_cast<void*>(&nativeCreate) },
        { "nativeDestroy", "(J)V", reinterpret_cast<void*>(&nativeDestroy) },
        { "nativeInitializeDisplay", "(J)V", reinterpret_cast<void*>(&nativeInitializeDisplay) },
        { "nativeTerminateDisplay", "(J)V", reinterpret_cast<void*>(&nativeTerminateDisplay) },
        { "nativeInitializeContext", "(J)V", reinterpret_cast<void*>(&nativeInitializeContext) },
        { "nativeTerminateContext", "(J)V", reinterpret_cast<void*>(&nativeTerminateContext) },
        { "nativeCreateSurface", "(JLandroid/view/Surface;)V", reinterpret_cast<void*>(&nativeCreateSurface) },
        { "nativeDestroySurface", "(J)V", reinterpret_cast<void*>(&nativeDestroySurface) },
        { "nativeStart", "(J)V", reinterpret_cast<void*>(&nativeStart) },
        { "nativeStop", "(J)V", reinterpret_cast<void*>(&nativeStop) },
        { "nativePause", "(J)V", reinterpret_cast<void*>(&nativePause) },
        { "nativeResume", "(J)V", reinterpret_cast<void*>(&nativeResume) },
        { "nativeRun", "(J)V", reinterpret_cast<void*>(&nativeRun) },
        { "nativeRerender", "(J)V", reinterpret_cast<void*>(&nativeRerender) },
        { "nativeUpdate", "(J)V", reinterpret_cast<void*>(&nativeUpdate) },
        { "nativeTerminate", "(J)V", reinterpret_cast<void*>(&nativeTerminate) },
        { "nativeNeedsSwap", "(J)Z", reinterpret_cast<void*>(&nativeNeedsSwap) },
        { "nativeSwapped", "(J)V", reinterpret_cast<void*>(&nativeSwapped) },
        { "nativeResize", "(JIIF)V", reinterpret_cast<void*>(static_cast<void JNICALL(*)(JNIEnv*,jobject,jlong,jint,jint,jfloat)>(&nativeResize)) },
        { "nativeResize", "(JIIFII)V", reinterpret_cast<void*>(static_cast<void JNICALL(*)(JNIEnv*,jobject,jlong,jint,jint,jfloat,jint,jint)>(&nativeResize)) },
        { "nativeSetAppliedClasses", "(JLjava/util/List;)V", reinterpret_cast<void*>(&nativeSetAppliedClasses) },
        { "nativeGetAppliedClasses", "(J)Ljava/util/List;", reinterpret_cast<void*>(&nativeGetAppliedClasses) },
        { "nativeSetDefaultTransitionDuration", "(JJ)V", reinterpret_cast<void*>(&nativeSetDefaultTransitionDuration) },
        { "nativeGetDefaultTransitionDuration", "(J)J", reinterpret_cast<void*>(&nativeGetDefaultTransitionDuration) },
        { "nativeSetStyleUrl", "(JLjava/lang/String;)V", reinterpret_cast<void*>(&nativeSetStyleURL) },
        { "nativeSetStyleJson", "(JLjava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeSetStyleJSON) },
        { "nativeGetStyleJson", "(J)Ljava/lang/String;", reinterpret_cast<void*>(&nativeGetStyleJSON) },
        { "nativeSetAccessToken", "(JLjava/lang/String;)V", reinterpret_cast<void*>(&nativeSetAccessToken) },
        { "nativeGetAccessToken", "(J)Ljava/lang/String;", reinterpret_cast<void*>(&nativeGetAccessToken) },
        { "nativeCancelTransitions", "(J)V", reinterpret_cast<void*>(&nativeCancelTransitions) },
        { "nativeMoveBy", "(JDDD)V", reinterpret_cast<void*>(&nativeMoveBy) },
        { "nativeSetLonLat", "(JLcom/mapbox/mapboxgl/lib/LonLat;D)V", reinterpret_cast<void*>(&nativeSetLonLat) },
        { "nativeGetLonLat", "(J)Lcom/mapbox/mapboxgl/lib/LonLat;", reinterpret_cast<void*>(&nativeGetLonLat) },
        { "nativeStartPanning", "(J)V", reinterpret_cast<void*>(&nativeStartPanning) },
        { "nativeStopPanning", "(J)V", reinterpret_cast<void*>(&nativeStopPanning) },
        { "nativeResetPosition", "(J)V", reinterpret_cast<void*>(&nativeResetPosition) },
        { "nativeScaleBy", "(JDDDD)V", reinterpret_cast<void*>(&nativeScaleBy) },
        { "nativeSetScale", "(JDDDD)V", reinterpret_cast<void*>(&nativeSetScale) },
        { "nativeGetScale", "(J)D", reinterpret_cast<void*>(&nativeGetScale) },
        { "nativeSetZoom", "(JDD)V", reinterpret_cast<void*>(&nativeSetZoom) },
        { "nativeGetZoom", "(J)D", reinterpret_cast<void*>(&nativeGetZoom) },
        { "nativeSetLonLatZoom", "(JLcom/mapbox/mapboxgl/lib/LonLatZoom;D)V", reinterpret_cast<void*>(&nativeSetLonLatZoom) },
        { "nativeGetLonLatZoom", "(J)Lcom/mapbox/mapboxgl/lib/LonLatZoom;", reinterpret_cast<void*>(&nativeGetLonLatZoom) },
        { "nativeResetZoom", "(J)V", reinterpret_cast<void*>(&nativeResetZoom) },
        { "nativeStartPanning", "(J)V", reinterpret_cast<void*>(&nativeStartScaling) },
        { "nativeStopPanning", "(J)V", reinterpret_cast<void*>(&nativeStopScaling) },
        { "nativeGetMinZoom", "(J)D", reinterpret_cast<void*>(&nativeGetMinZoom) },
        { "nativeGetMaxZoom", "(J)D", reinterpret_cast<void*>(&nativeGetMaxZoom) },
        { "nativeRotateBy", "(JDDDDD)V", reinterpret_cast<void*>(&nativeRotateBy) },
        { "nativeSetBearing", "(JDD)V", reinterpret_cast<void*>(static_cast<void JNICALL(*)(JNIEnv*,jobject,jlong,jdouble,jdouble)>(&nativeSetBearing)) },
        { "nativeSetBearing", "(JDDD)V", reinterpret_cast<void*>(static_cast<void JNICALL(*)(JNIEnv*,jobject,jlong,jdouble,jdouble,jdouble)>(&nativeSetBearing)) },
        { "nativeGetBearing", "(J)D", reinterpret_cast<void*>(&nativeGetBearing) },
        { "nativeResetNorth", "(J)V", reinterpret_cast<void*>(&nativeResetNorth) },
        { "nativeStartRotating", "(J)V", reinterpret_cast<void*>(&nativeStartRotating) },
        { "nativeStopRotating", "(J)V", reinterpret_cast<void*>(&nativeStopRotating) },
        { "nativeSetDebug", "(JZ)V", reinterpret_cast<void*>(&nativeSetDebug) },
        { "nativeToggleDebug", "(J)V", reinterpret_cast<void*>(&nativeToggleDebug) },
        { "nativeGetDebug", "(J)Z", reinterpret_cast<void*>(&nativeGetDebug) },
        { "nativeSetReachability", "(JZ)V", reinterpret_cast<void*>(&nativeSetReachability) }
    }};

    if (env->RegisterNatives(nativeMapViewClass, methods.data(), methods.size()) < 0) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatClass = reinterpret_cast<jclass>(env->NewGlobalRef(lonLatClass));
    if (lonLatClass == nullptr) {
        env->ExceptionDescribe();
        return JNI_ERR;
    }

    lonLatZoomClass = reinterpret_cast<jclass>(env->NewGlobalRef(lonLatZoomClass));
    if (lonLatZoomClass == nullptr) {
        env->ExceptionDescribe();
        env->DeleteGlobalRef(lonLatClass);
        return JNI_ERR;
    }

    runtimeExceptionClass = reinterpret_cast<jclass>(env->NewGlobalRef(runtimeExceptionClass));
    if (runtimeExceptionClass == nullptr) {
        env->ExceptionDescribe();
        env->DeleteGlobalRef(lonLatClass);
        env->DeleteGlobalRef(lonLatZoomClass);
        return JNI_ERR;
    }

    nullPointerExceptionClass = reinterpret_cast<jclass>(env->NewGlobalRef(nullPointerExceptionClass));
    if (nullPointerExceptionClass == nullptr) {
        env->ExceptionDescribe();
        env->DeleteGlobalRef(lonLatClass);
        env->DeleteGlobalRef(lonLatZoomClass);
        env->DeleteGlobalRef(runtimeExceptionClass);
        return JNI_ERR;
    }

    arrayListClass = reinterpret_cast<jclass>(env->NewGlobalRef(arrayListClass));
    if (arrayListClass == nullptr) {
        env->ExceptionDescribe();
        env->DeleteGlobalRef(lonLatClass);
        env->DeleteGlobalRef(lonLatZoomClass);
        env->DeleteGlobalRef(runtimeExceptionClass);
        env->DeleteGlobalRef(nullPointerExceptionClass);
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    mbgl::Log::Debug(mbgl::Event::JNI, "JNI_OnUnload");

    JNIEnv* env = nullptr;
    jint ret = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (ret != JNI_OK) {
        mbgl::Log::Error(mbgl::Event::JNI, "GetEnv() failed with %i", ret);
        return;
    }

    env->DeleteGlobalRef(lonLatClass);
    lonLatClass = nullptr;
    lonLatConstructorId = nullptr;
    lonLatLonId = nullptr;
    lonLatLatId = nullptr;

    env->DeleteGlobalRef(lonLatZoomClass);
    lonLatZoomClass = nullptr;
    lonLatZoomConstructorId = nullptr;
    lonLatZoomLonId = nullptr;
    lonLatZoomLatId = nullptr;
    lonLatZoomZoomId = nullptr;

    onMapChangedId = nullptr;
    onFpsChangedId = nullptr;

    env->DeleteGlobalRef(runtimeExceptionClass);
    runtimeExceptionClass = nullptr;

    env->DeleteGlobalRef(nullPointerExceptionClass);
    nullPointerExceptionClass = nullptr;

    listToArrayId = nullptr;

    env->DeleteGlobalRef(arrayListClass);
    arrayListClass = nullptr;
    arrayListConstructorId = nullptr;
    arrayListAddId = nullptr;
}

}
