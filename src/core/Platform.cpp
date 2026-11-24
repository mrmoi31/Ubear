#include "Platform.hpp"

static platform::FocusCallback g_focusCb = nullptr;

void platform::setFocusCallback(FocusCallback cb) { g_focusCb = cb; }

extern "C" void UbearOnFocusChange(int focused)
{
    if (g_focusCb) g_focusCb(focused != 0);
}

#if defined(PLATFORM_ANDROID)

#include <android_native_app_glue.h>
#include <jni.h>

extern "C" struct android_app* GetAndroidApp(void);

namespace {
struct Vibrator {
    bool      ready = false, failed = false;
    JNIEnv*   env = nullptr;
    jobject   vibrator = nullptr;     // référence globale
    jmethodID vibrateLegacy = nullptr; // vibrate(long)
    jmethodID vibrateEffect = nullptr; // vibrate(VibrationEffect)
    jclass    effectClass = nullptr;   // référence globale
    jmethodID createOneShot = nullptr;

    bool clearException()
    {
        if (env->ExceptionCheck()) { env->ExceptionClear(); return true; }
        return false;
    }

    void init()
    {
        android_app* app = GetAndroidApp();
        if (!app || !app->activity) { failed = true; return; }
        JavaVM* vm = app->activity->vm;
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) { failed = true; return; }
        }
        jobject activity = app->activity->clazz;
        jclass ctx = env->FindClass("android/content/Context");
        if (clearException() || !ctx) { failed = true; return; }
        jfieldID fid = env->GetStaticFieldID(ctx, "VIBRATOR_SERVICE", "Ljava/lang/String;");
        if (clearException() || !fid) { failed = true; return; }
        jobject name = env->GetStaticObjectField(ctx, fid);
        jmethodID getSvc = env->GetMethodID(ctx, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        if (clearException() || !getSvc) { failed = true; return; }
        jobject vib = env->CallObjectMethod(activity, getSvc, name);
        if (clearException() || !vib) { failed = true; return; }
        vibrator = env->NewGlobalRef(vib);

        jclass vibCls = env->GetObjectClass(vib);
        jclass eff = env->FindClass("android/os/VibrationEffect");   // API 26+
        if (!clearException() && eff) {
            createOneShot = env->GetStaticMethodID(eff, "createOneShot", "(JI)Landroid/os/VibrationEffect;");
            vibrateEffect = env->GetMethodID(vibCls, "vibrate", "(Landroid/os/VibrationEffect;)V");
            if (clearException() || !createOneShot || !vibrateEffect) { createOneShot = nullptr; vibrateEffect = nullptr; }
            else effectClass = (jclass)env->NewGlobalRef(eff);
        }
        if (!vibrateEffect) {
            vibrateLegacy = env->GetMethodID(vibCls, "vibrate", "(J)V");
            if (clearException() || !vibrateLegacy) { failed = true; return; }
        }
        ready = true;
    }

    void vibrate(int ms)
    {
        if (failed) return;
        if (!ready) init();
        if (!ready) return;
        if (vibrateEffect) {
            jobject effect = env->CallStaticObjectMethod(effectClass, createOneShot, (jlong)ms, (jint)-1);
            if (!clearException() && effect) {
                env->CallVoidMethod(vibrator, vibrateEffect, effect);
                clearException();
                env->DeleteLocalRef(effect);
            }
        } else {
            env->CallVoidMethod(vibrator, vibrateLegacy, (jlong)ms);
            clearException();
        }
    }

    void release()
    {
        if (!ready) return;
        if (vibrator) env->DeleteGlobalRef(vibrator);
        if (effectClass) env->DeleteGlobalRef(effectClass);
        vibrator = nullptr; effectClass = nullptr; ready = false;
        android_app* app = GetAndroidApp();
        if (app && app->activity) app->activity->vm->DetachCurrentThread();
    }
};
Vibrator g_vibrator;
} // namespace

void platform::vibrate(int ms) { g_vibrator.vibrate(ms); }
void platform::shutdown() { g_vibrator.release(); }

namespace {
jclass audioClass = nullptr;
jmethodID audioAllows = nullptr;

JNIEnv* attachEnv(bool& attached)
{
    attached = false;
    android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) return nullptr;
    JNIEnv* env = nullptr;
    jint st = app->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (st == JNI_OK) return env;
    if (app->activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return nullptr;
    attached = true;
    return env;
}

void detachEnv(bool attached)
{
    if (!attached) return;
    android_app* app = GetAndroidApp();
    if (app && app->activity && app->activity->vm) app->activity->vm->DetachCurrentThread();
}

// FindClass ne voit pas les classes de l'appli depuis le thread natif : on passe par le ClassLoader.
jclass loadAppClass(JNIEnv* env, jobject activity, const char* cls)
{
    jclass activityCls = env->GetObjectClass(activity);
    jmethodID getLoader = env->GetMethodID(activityCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = env->CallObjectMethod(activity, getLoader);
    jclass loaderCls = env->FindClass("java/lang/ClassLoader");
    jmethodID loadClass = env->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring name = env->NewStringUTF(cls);
    jclass local = (jclass)env->CallObjectMethod(loader, loadClass, name);
    if (env->ExceptionCheck() || !local) { env->ExceptionClear(); return nullptr; }
    return (jclass)env->NewGlobalRef(local);
}

jclass notifierClass = nullptr;

// Appelle f(env, activity) avec la classe Notifier chargée.
template <class F> void withNotifier(F f)
{
    bool attached = false;
    JNIEnv* env = attachEnv(attached);
    if (!env) return;
    jobject activity = GetAndroidApp()->activity->clazz;
    if (!notifierClass) notifierClass = loadAppClass(env, activity, "com.ubear.game.Notifier");
    if (notifierClass) f(env, activity);
    if (env->ExceptionCheck()) env->ExceptionClear();
    detachEnv(attached);
}
}

void platform::requestNotifications()
{
    withNotifier([](JNIEnv* env, jobject activity) {
        jmethodID m = env->GetStaticMethodID(notifierClass, "requestPermission", "(Landroid/app/Activity;)V");
        if (m) env->CallStaticVoidMethod(notifierClass, m, activity);
    });
}

void platform::scheduleNotification(int id, long long delaySec, const char* title, const char* text)
{
    withNotifier([&](JNIEnv* env, jobject activity) {
        jmethodID m = env->GetStaticMethodID(notifierClass, "schedule",
                                             "(Landroid/content/Context;IJLjava/lang/String;Ljava/lang/String;)V");
        if (!m) return;
        jstring jt = env->NewStringUTF(title), jx = env->NewStringUTF(text);
        env->CallStaticVoidMethod(notifierClass, m, activity, (jint)id, (jlong)delaySec, jt, jx);
        env->DeleteLocalRef(jt);
        env->DeleteLocalRef(jx);
    });
}

void platform::cancelNotifications()
{
    withNotifier([](JNIEnv* env, jobject activity) {
        jmethodID m = env->GetStaticMethodID(notifierClass, "cancelAll", "(Landroid/content/Context;)V");
        if (m) env->CallStaticVoidMethod(notifierClass, m, activity);
    });
}

void platform::initAudioFocus()
{
    bool attached = false;
    JNIEnv* env = attachEnv(attached);
    if (!env) return;
    android_app* app = GetAndroidApp();
    jobject activity = app->activity->clazz;

    audioClass = loadAppClass(env, activity, "com.ubear.game.AudioFocus");
    if (!audioClass) {
        detachEnv(attached);
        return;
    }
    jmethodID init = env->GetStaticMethodID(audioClass, "init", "(Landroid/content/Context;)V");
    audioAllows = env->GetStaticMethodID(audioClass, "allowsMusic", "()Z");
    if (init) env->CallStaticVoidMethod(audioClass, init, activity);
    if (env->ExceptionCheck()) env->ExceptionClear();
    detachEnv(attached);
}

bool platform::allowGameMusic()
{
    if (!audioClass || !audioAllows) return true;
    bool attached = false;
    JNIEnv* env = attachEnv(attached);
    if (!env) return true;
    jboolean ok = env->CallStaticBooleanMethod(audioClass, audioAllows);
    if (env->ExceptionCheck()) { env->ExceptionClear(); detachEnv(attached); return true; }
    detachEnv(attached);
    return ok == JNI_TRUE;
}

#elif defined(PLATFORM_IOS)

extern "C" int IosOtherMusicPlaying(void);
extern "C" void IosHaptic(int ms);   // rcore_ios.m
extern "C" void IosNotifyRequest(void);
extern "C" void IosNotifySchedule(int id, long long delaySec, const char* title, const char* text);
extern "C" void IosNotifyCancelAll(void);

void platform::vibrate(int ms) { IosHaptic(ms); }
void platform::requestNotifications() { IosNotifyRequest(); }
void platform::scheduleNotification(int id, long long delaySec, const char* title, const char* text)
{
    IosNotifySchedule(id, delaySec, title, text);
}
void platform::cancelNotifications() { IosNotifyCancelAll(); }
void platform::shutdown() {}
void platform::initAudioFocus() {}
bool platform::allowGameMusic() { return IosOtherMusicPlaying() == 0; }

#else

void platform::requestNotifications() {}
void platform::scheduleNotification(int, long long, const char*, const char*) {}
void platform::cancelNotifications() {}
void platform::vibrate(int) {}
void platform::shutdown() {}
void platform::initAudioFocus() {}
bool platform::allowGameMusic() { return true; }

#endif
