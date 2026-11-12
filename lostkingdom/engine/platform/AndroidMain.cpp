// AGR Engine — Android backend: NativeActivity (no Java code), EGL / OpenGL ES 3, AAudio, JNI haptics.
#include "../core/App.h"
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/window.h>
#include <aaudio/AAudio.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <atomic>
#include <thread>
#include <algorithm>
#include <string>
#include <vector>

namespace agr {
static android_app* gApp = nullptr;
static std::string gDataDir;
static std::vector<std::string> gNoArgs;
static std::atomic<bool> gQuit{false};

namespace platform {
bool readAsset(const std::string& path, std::vector<uint8_t>& out) {
    if (!gApp) return false;
    AAsset* a = AAssetManager_open(gApp->activity->assetManager, path.c_str(), AASSET_MODE_BUFFER);
    if (!a) return false;
    size_t n = (size_t)AAsset_getLength(a);
    out.resize(n);
    int r = AAsset_read(a, out.data(), n);
    AAsset_close(a);
    return r == (int)n;
}
std::string writableDir() { return gDataDir; }
void requestQuit() { gQuit = true; if (gApp) ANativeActivity_finish(gApp->activity); }
void requestScreenshot(const std::string&) {}
const std::vector<std::string>& launchArgs() { return gNoArgs; }
bool isHeadlessTest() { return false; }

void vibrate(int ms, int amplitude) {
    if (!gApp) return;
    JavaVM* vm = gApp->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    jobject activity = gApp->activity->clazz;
    jclass ctxCls = env->FindClass("android/content/Context");
    jfieldID fid = env->GetStaticFieldID(ctxCls, "VIBRATOR_SERVICE", "Ljava/lang/String;");
    jobject svcName = env->GetStaticObjectField(ctxCls, fid);
    jclass actCls = env->GetObjectClass(activity);
    jmethodID getSvc = env->GetMethodID(actCls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject vib = env->CallObjectMethod(activity, getSvc, svcName);
    if (vib) {
        jclass vibCls = env->GetObjectClass(vib);
        jclass effCls = env->FindClass("android/os/VibrationEffect");
        jmethodID create = effCls ? env->GetStaticMethodID(effCls, "createOneShot", "(JI)Landroid/os/VibrationEffect;") : nullptr;
        if (create) {
            jobject eff = env->CallStaticObjectMethod(effCls, create, (jlong)ms, (jint)std::max(1, std::min(255, amplitude)));
            jmethodID vm2 = env->GetMethodID(vibCls, "vibrate", "(Landroid/os/VibrationEffect;)V");
            if (vm2 && eff) env->CallVoidMethod(vib, vm2, eff);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (attached) vm->DetachCurrentThread();
}
}  // namespace platform
}  // namespace agr

using namespace agr;

struct Engine {
    android_app* app = nullptr;
    App* game = nullptr;
    EGLDisplay dpy = EGL_NO_DISPLAY;
    EGLSurface surf = EGL_NO_SURFACE;
    EGLContext ctx = EGL_NO_CONTEXT;
    EGLConfig cfg = nullptr;
    int w = 0, h = 0;
    bool gameInit = false, focused = false, resumed = false, hasWindow = false;
    AAudioStream* audio = nullptr;
    double lastTime = 0;
};

static void setImmersive(android_app* app) {
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);
    jobject act = app->activity->clazz;
    jclass actCls = env->GetObjectClass(act);
    jmethodID getWindow = env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(act, getWindow);
    jclass winCls = env->GetObjectClass(window);
    jmethodID getDecor = env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;");
    jobject decor = env->CallObjectMethod(window, getDecor);
    jclass viewCls = env->GetObjectClass(decor);
    jmethodID setVis = env->GetMethodID(viewCls, "setSystemUiVisibility", "(I)V");
    // IMMERSIVE_STICKY | FULLSCREEN | HIDE_NAVIGATION | LAYOUT_* flags
    int flags = 0x00001000 | 0x00000004 | 0x00000002 | 0x00000100 | 0x00000200 | 0x00000400;
    if (setVis) env->CallVoidMethod(decor, setVis, flags);
    if (env->ExceptionCheck()) env->ExceptionClear();
    app->activity->vm->DetachCurrentThread();
}

static aaudio_data_callback_result_t audioCb(AAudioStream* s, void* user, void* data, int32_t frames) {
    Engine* e = (Engine*)user;
    float* out = (float*)data;
    if (e->game && e->gameInit) e->game->renderAudio(out, frames, AAudioStream_getSampleRate(s));
    else for (int i = 0; i < frames * 2; i++) out[i] = 0;
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void audioError(AAudioStream*, void* user, aaudio_result_t err);

static void startAudio(Engine* e) {
    if (e->audio) return;
    AAudioStreamBuilder* b = nullptr;
    if (AAudio_createStreamBuilder(&b) != AAUDIO_OK) return;
    AAudioStreamBuilder_setFormat(b, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(b, 2);
    AAudioStreamBuilder_setSampleRate(b, 48000);
    AAudioStreamBuilder_setPerformanceMode(b, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(b, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setDataCallback(b, audioCb, e);
    AAudioStreamBuilder_setErrorCallback(b, audioError, e);
    if (AAudioStreamBuilder_openStream(b, &e->audio) == AAUDIO_OK) AAudioStream_requestStart(e->audio);
    else e->audio = nullptr;
    AAudioStreamBuilder_delete(b);
}

static void stopAudio(Engine* e) {
    if (!e->audio) return;
    AAudioStream_requestStop(e->audio);
    AAudioStream_close(e->audio);
    e->audio = nullptr;
}

static void audioError(AAudioStream*, void* user, aaudio_result_t err) {
    // Device disconnected (e.g. headphones unplugged): restart on a fresh stream from another thread.
    AGR_WARN("AAudio error %d, restarting stream", err);
    Engine* e = (Engine*)user;
    std::thread([e]() { stopAudio(e); startAudio(e); }).detach();
}

static bool initDisplay(Engine* e) {
    if (e->dpy == EGL_NO_DISPLAY) {
        e->dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(e->dpy, nullptr, nullptr);
        const EGLint attrs[] = {EGL_RENDERABLE_TYPE, 0x0040 /*EGL_OPENGL_ES3_BIT*/, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                                EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_NONE};
        EGLint n = 0;
        eglChooseConfig(e->dpy, attrs, &e->cfg, 1, &n);
        if (!n) { AGR_ERR("no EGL config"); return false; }
    }
    if (e->ctx == EGL_NO_CONTEXT) {
        const EGLint ca[] = {0x3098 /*EGL_CONTEXT_CLIENT_VERSION*/, 3, EGL_NONE};
        e->ctx = eglCreateContext(e->dpy, e->cfg, EGL_NO_CONTEXT, ca);
        if (e->ctx == EGL_NO_CONTEXT) { AGR_ERR("eglCreateContext failed"); return false; }
        if (e->gameInit) { e->game->contextLost(); e->gameInit = false; }
    }
    e->surf = eglCreateWindowSurface(e->dpy, e->cfg, e->app->window, nullptr);
    if (!eglMakeCurrent(e->dpy, e->surf, e->surf, e->ctx)) {
        EGLint err = eglGetError();
        AGR_ERR("eglMakeCurrent failed 0x%x", err);
        if (err == EGL_CONTEXT_LOST) {
            eglDestroyContext(e->dpy, e->ctx); e->ctx = EGL_NO_CONTEXT;
            if (e->gameInit) { e->game->contextLost(); e->gameInit = false; }
            return initDisplay(e);
        }
        return false;
    }
    eglSwapInterval(e->dpy, 1);
    eglQuerySurface(e->dpy, e->surf, EGL_WIDTH, &e->w);
    eglQuerySurface(e->dpy, e->surf, EGL_HEIGHT, &e->h);
    if (!e->gameInit) {
        e->gameInit = e->game->init(e->w, e->h);
        if (!e->gameInit) AGR_ERR("game init failed");
    } else e->game->resize(e->w, e->h);
    return e->gameInit;
}

static void termSurface(Engine* e) {
    if (e->dpy == EGL_NO_DISPLAY) return;
    eglMakeCurrent(e->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, e->ctx);  // keep the context (and all GPU resources) alive
    if (e->surf != EGL_NO_SURFACE) eglDestroySurface(e->dpy, e->surf);
    e->surf = EGL_NO_SURFACE;
}

static void handleCmd(android_app* app, int32_t cmd) {
    Engine* e = (Engine*)app->userData;
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) { e->hasWindow = true; initDisplay(e); setImmersive(app); }
        break;
    case APP_CMD_TERM_WINDOW:
        e->hasWindow = false;
        if (e->gameInit) e->game->pause();
        termSurface(e);
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONTENT_RECT_CHANGED:
    case APP_CMD_CONFIG_CHANGED:
        if (e->surf != EGL_NO_SURFACE) {
            eglQuerySurface(e->dpy, e->surf, EGL_WIDTH, &e->w);
            eglQuerySurface(e->dpy, e->surf, EGL_HEIGHT, &e->h);
            if (e->gameInit) e->game->resize(e->w, e->h);
        }
        break;
    case APP_CMD_GAINED_FOCUS: e->focused = true; if (e->gameInit) e->game->resume(); startAudio(e); setImmersive(app); break;
    case APP_CMD_LOST_FOCUS: e->focused = false; if (e->gameInit) e->game->pause(); break;
    case APP_CMD_PAUSE: e->resumed = false; if (e->gameInit) e->game->pause(); stopAudio(e); break;
    case APP_CMD_RESUME: e->resumed = true; break;
    case APP_CMD_SAVE_STATE: if (e->gameInit) e->game->pause(); break;  // the game autosaves on pause
    case APP_CMD_LOW_MEMORY: if (e->gameInit) e->game->lowMemory(); break;
    case APP_CMD_DESTROY: break;
    }
}

static int32_t handleInput(android_app* app, AInputEvent* ev) {
    Engine* e = (Engine*)app->userData;
    if (!e->gameInit) return 0;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getKeyCode(ev) == AKEYCODE_BACK) {
            if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) {
                if (!e->game->back()) return 0;  // let the system handle it (moves task to back)
            }
            return 1;
        }
        return 0;
    }
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t action = AMotionEvent_getAction(ev);
    int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pidx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    size_t count = AMotionEvent_getPointerCount(ev);
    auto send = [&](TouchEvent::Type t, size_t i) {
        TouchEvent te{t, (int)AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i)};
        e->game->touch(te);
    };
    switch (masked) {
    case AMOTION_EVENT_ACTION_DOWN: send(TouchEvent::Down, 0); break;
    case AMOTION_EVENT_ACTION_POINTER_DOWN: send(TouchEvent::Down, pidx); break;
    case AMOTION_EVENT_ACTION_UP: send(TouchEvent::Up, 0); break;
    case AMOTION_EVENT_ACTION_POINTER_UP: send(TouchEvent::Up, pidx); break;
    case AMOTION_EVENT_ACTION_CANCEL: for (size_t i = 0; i < count; i++) send(TouchEvent::Cancel, i); break;
    case AMOTION_EVENT_ACTION_MOVE: for (size_t i = 0; i < count; i++) send(TouchEvent::Move, i); break;
    }
    return 1;
}

void android_main(android_app* app) {
    Engine e;
    e.app = app;
    gApp = app;
    gDataDir = app->activity->internalDataPath ? app->activity->internalDataPath : "/data/local/tmp";
    mkdir(gDataDir.c_str(), 0700);
    app->userData = &e;
    app->onAppCmd = handleCmd;
    app->onInputEvent = handleInput;
    ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    e.game = createApp();
    e.lastTime = platform::timeSeconds();

    while (!app->destroyRequested) {
        int events;
        android_poll_source* src;
        bool active = e.focused && e.hasWindow && e.surf != EGL_NO_SURFACE;
        while (ALooper_pollOnce(active ? 0 : 250, nullptr, &events, (void**)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) break;
            active = e.focused && e.hasWindow && e.surf != EGL_NO_SURFACE;
        }
        if (app->destroyRequested) break;
        if (!(e.focused && e.hasWindow && e.surf != EGL_NO_SURFACE && e.gameInit)) { e.lastTime = platform::timeSeconds(); continue; }
        double now = platform::timeSeconds();
        float dt = (float)std::min(0.1, now - e.lastTime);
        // frame pacing for 30 FPS profiles (vsync gives 60)
        int fps = e.game->targetFps();
        if (fps < 50 && now - e.lastTime < 1.0 / fps - 0.002) {
            struct timespec ts{0, (long)((1.0 / fps - (now - e.lastTime)) * 1e9 * 0.9)};
            nanosleep(&ts, nullptr);
            continue;
        }
        e.lastTime = now;
        // The window can change size without a reliable APP_CMD_WINDOW_RESIZED (immersive mode hiding the
        // navigation bar after start-up, 180° rotation, split screen). A stale size breaks the UI scale and
        // makes touches land beside the buttons, so check every frame (cheap).
        {
            EGLint w = 0, h = 0;
            eglQuerySurface(e.dpy, e.surf, EGL_WIDTH, &w);
            eglQuerySurface(e.dpy, e.surf, EGL_HEIGHT, &h);
            int ww = app->window ? ANativeWindow_getWidth(app->window) : w, wh = app->window ? ANativeWindow_getHeight(app->window) : h;
            if (ww > 0 && wh > 0 && (ww != w || wh != h)) { w = ww; h = wh; }
            if (w > 0 && h > 0 && (w != e.w || h != e.h)) {
                AGR_LOG("surface resized %dx%d -> %dx%d", e.w, e.h, w, h);
                e.w = w; e.h = h;
                e.game->resize(w, h);
            }
        }
        e.game->frame(dt);
        if (!eglSwapBuffers(e.dpy, e.surf)) {
            EGLint err = eglGetError();
            if (err == EGL_BAD_SURFACE) { termSurface(&e); if (app->window) initDisplay(&e); }
            else if (err == EGL_CONTEXT_LOST) { e.game->contextLost(); e.gameInit = false; eglDestroyContext(e.dpy, e.ctx); e.ctx = EGL_NO_CONTEXT; termSurface(&e); if (app->window) initDisplay(&e); }
        }
    }
    if (e.gameInit) { e.game->pause(); e.game->shutdown(); }
    stopAudio(&e);
    delete e.game;
    if (e.dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(e.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (e.ctx != EGL_NO_CONTEXT) eglDestroyContext(e.dpy, e.ctx);
        if (e.surf != EGL_NO_SURFACE) eglDestroySurface(e.dpy, e.surf);
        eglTerminate(e.dpy);
    }
}
