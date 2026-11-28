// AGR Engine — particle system: CPU simulation, GPU instanced camera-facing quads.
#pragma once
#include "GL.h"
#include "../core/Noise.h"
#include <vector>
#include <functional>

namespace agr {

enum PFrame { PF_DOT = 0, PF_SMOKE, PF_SPARK, PF_FLAME, PF_LEAF, PF_SNOW, PF_STAR, PF_RING, PF_RUNE, PF_RAIN, PF_EMBER, PF_SHARD, PF_CROSS, PF_BIRD, PF_DUST, PF_BOLT };

struct Particle {
    vec3 pos, vel;
    float life = 0, maxLife = 1;
    float size0 = 0.2f, size1 = 0.f;
    vec4 c0{1, 1, 1, 1}, c1{1, 1, 1, 0};
    float rot = 0, rotVel = 0;
    float drag = 0.f, gravity = 0.f;
    float stretch = 0.f;      // >0 = velocity aligned streak
    float turbulence = 0.f;   // curl-ish wobble amplitude
    float groundKill = -1e9f; // die (or bounce) below this height
    uint8_t frame = PF_DOT;
    bool additive = true;
    bool bounce = false;
};

class ParticleSystem {
public:
    void init(int maxParticles);
    void shutdown();
    void setBudget(float scale) { budgetScale = scale; }
    void spawn(const Particle& p);
    void update(float dt);
    // Returns number of draw calls issued (0 or 1). Shader must already be bound.
    int draw(bool additive, const Mesh& quad);
    void clear() { parts.clear(); }
    size_t count() const { return parts.size(); }
    Rng rng{1234};

    // --- presets ---------------------------------------------------------------------------
    void fire(vec3 p, float scale, float dt);            // continuous: call every frame
    void smoke(vec3 p, float scale, float dt, vec3 tint = {0.35f, 0.33f, 0.3f});
    void sparks(vec3 p, vec3 dir, int n, vec3 color = {1.f, 0.75f, 0.35f});
    void impact(vec3 p, vec3 normal, vec3 color, float scale);
    void shockwave(vec3 p, vec3 color, float radius);
    void dust(vec3 p, float scale, vec3 color = {0.55f, 0.48f, 0.38f});
    void magicBurst(vec3 p, vec3 color, float scale, int n = 24);
    void magicTrail(vec3 p, vec3 color, float scale);
    void heal(vec3 p, vec3 color = {0.4f, 1.f, 0.5f});
    void explosion(vec3 p, float scale, vec3 color = {1.f, 0.55f, 0.2f});
    void splash(vec3 p, float scale);
    void iceShards(vec3 p, int n);
    void lightningArc(vec3 a, vec3 b, vec3 color);
    void levelUp(vec3 p);
    void dissolveBurst(vec3 p, vec3 color, float radius);

private:
    std::vector<Particle> parts;
    std::vector<float> inst;
    GLuint vao = 0, ivbo = 0;
    int capacity = 0, maxParts = 4000;
    float budgetScale = 1.f;
    float accum[4] = {0, 0, 0, 0};
};

}  // namespace agr
