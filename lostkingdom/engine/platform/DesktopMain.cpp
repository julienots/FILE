// AGR Engine — headless desktop backend (Linux, EGL pbuffer on Mesa).
// Runs the real game code with a fixed time step for automated play-tests, screenshots
// (PNG) and audio captures (WAV). Not shipped in the APK.
#include "../core/App.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

namespace agr {
static std::string gAssetDir = "assets", gSaveDir = "build/desktop_save";
static std::vector<std::string> gScreenshotQueue;
static std::vector<std::string> gArgs;

namespace platform {
bool readAsset(const std::string& path, std::vector<uint8_t>& out) {
    std::string full = gAssetDir + "/" + path;
    FILE* f = fopen(full.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize((size_t)n);
    size_t r = fread(out.data(), 1, (size_t)n, f);
    fclose(f);
    return r == (size_t)n;
}
std::string writableDir() { return gSaveDir; }
void vibrate(int, int) {}
void requestQuit() {}
void requestScreenshot(const std::string& path) { gScreenshotQueue.push_back(path); }
const std::vector<std::string>& launchArgs() { return gArgs; }
bool isHeadlessTest() { return true; }
}  // namespace platform
}  // namespace agr

using namespace agr;

static void saveScreenshot(const std::string& path, int w, int h) {
    std::vector<uint8_t> px((size_t)w * h * 4), flip((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++) memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    for (size_t i = 3; i < flip.size(); i += 4) flip[i] = 255;
    stbi_write_png(path.c_str(), w, h, 4, flip.data(), w * 4);
    AGR_LOG("screenshot -> %s", path.c_str());
}

static void writeWav(const std::string& path, const std::vector<float>& s, int rate) {
    std::ofstream f(path, std::ios::binary);
    auto w32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
    auto w16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
    uint32_t bytes = (uint32_t)(s.size() * 2);
    f.write("RIFF", 4); w32(36 + bytes); f.write("WAVEfmt ", 8); w32(16); w16(1); w16(2); w32(rate); w32(rate * 4); w16(4); w16(16);
    f.write("data", 4); w32(bytes);
    for (float v : s) { int x = (int)(std::max(-1.f, std::min(1.f, v)) * 32767); w16((uint16_t)(int16_t)x); }
}

int main(int argc, char** argv) {
    int W = 1280, H = 720, frames = 300;
    float dt = 1.f / 30.f;
    std::string wav;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--size") { std::string v = next(); sscanf(v.c_str(), "%dx%d", &W, &H); }
        else if (a == "--frames") frames = atoi(next().c_str());
        else if (a == "--dt") dt = (float)atof(next().c_str());
        else if (a == "--assets") gAssetDir = next();
        else if (a == "--save") gSaveDir = next();
        else if (a == "--wav") wav = next();
        else gArgs.push_back(a);
    }
    mkdir(gSaveDir.c_str(), 0755);

    auto getPlat = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay dpy = getPlat ? getPlat(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr) : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint ma, mi;
    if (!eglInitialize(dpy, &ma, &mi)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint cfgAttr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n);
    if (!n) { fprintf(stderr, "no EGL config\n"); return 1; }
    EGLint pb[] = {EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE};
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
    EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "eglMakeCurrent failed\n"); return 1; }

    App* app = createApp();
    if (!app->init(W, H)) { fprintf(stderr, "app init failed\n"); return 2; }
    std::vector<float> audio;
    const int rate = 48000;
    std::vector<float> abuf;
    double frameMsTotal = 0, frameMsMax = 0;
    for (int f = 0; f < frames; f++) {
        double t0 = platform::timeSeconds();
        app->frame(dt);
        glFinish();
        double ms = (platform::timeSeconds() - t0) * 1000;
        frameMsTotal += ms; frameMsMax = std::max(frameMsMax, ms);
        for (auto& p : gScreenshotQueue) saveScreenshot(p, W, H);
        gScreenshotQueue.clear();
        if (!wav.empty()) {
            int nf = (int)(rate * dt);
            abuf.assign((size_t)nf * 2, 0.f);
            app->renderAudio(abuf.data(), nf, rate);
            audio.insert(audio.end(), abuf.begin(), abuf.end());
        }
        eglSwapBuffers(dpy, surf);
    }
    AGR_LOG("desktop run: %d frames, avg %.1f ms, max %.1f ms (software rasteriser)", frames, frameMsTotal / std::max(1, frames), frameMsMax);
    app->pause();
    app->shutdown();
    delete app;
    if (!wav.empty()) writeWav(wav, audio, rate);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surf);
    eglTerminate(dpy);
    return 0;
}
