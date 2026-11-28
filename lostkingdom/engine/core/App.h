// AGR Engine — application interface implemented by the game and driven by a platform backend.
#pragma once
#include "Platform.h"

namespace agr {

class App {
public:
    virtual ~App() = default;
    virtual bool init(int width, int height) = 0;     // GL context is current
    virtual void frame(float dt) = 0;                  // update + render one frame
    virtual void resize(int width, int height) = 0;
    virtual void touch(const TouchEvent& e) = 0;
    virtual bool back() = 0;                           // Android back button; return true if handled
    virtual void pause() = 0;                          // app goes to background: save, mute
    virtual void resume() = 0;
    virtual void contextLost() = 0;                    // GL objects are gone, recreate on next init
    virtual void lowMemory() {}
    virtual void shutdown() = 0;
    virtual int targetFps() const { return 60; }
    // Audio callback (called from the audio thread): fill interleaved stereo float frames.
    virtual void renderAudio(float* out, int frames, int sampleRate) = 0;
};

App* createApp();

}  // namespace agr
