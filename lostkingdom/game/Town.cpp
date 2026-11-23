// LOST KINGDOM — 3.9.0 Aldrenbourg, the market town of Greenhaven. Once the Grande Foire is open (quest
// al_fair), the square is dressed for the feast: masts round the fountain, garlands of pennants slung between them
// and across the square, fluttering in the wind.
#include "Game.h"

namespace lk {

void Game::renderFairBunting() {
    if (dungeon.active || !questsDone.count("al_fair")) return;
    const Poi* p = world.poiById("aldrenbourg");
    if (!p || length(p->pos - camFocus.xz()) > 220.f) return;
    static std::shared_ptr<Mesh> m;
    static vec3 origin;
    if (!m) {
        float gy0 = world.groundHeight(p->pos.x, p->pos.y);
        origin = vec3(p->pos.x, gy0, p->pos.y);
        const World& W = world;
        vec2 c = p->pos;
        m = cachedMesh("fair_bunting", [&](MeshBuilder& b) {
            const int NP = 10; const float R = 15.5f, H = 6.2f;
            Mat pole(hexColor(0x6a4a2e), 0.85f), rope(hexColor(0xd8c8a0), 0.9f), gold(hexColor(0xc9a45c), 0.35f, 0.9f);
            vec3 cols[] = {hexColor(0xc0392b), hexColor(0xf1c40f), hexColor(0x2a5fa8), hexColor(0xf4f0e6), hexColor(0x27ae60)};
            std::vector<vec3> tops;
            for (int i = 0; i < NP; i++) {
                float a = (i + 0.5f) * TAU / NP;
                vec2 q = c + vec2(std::cos(a), std::sin(a)) * R;
                float gy = W.groundHeight(q.x, q.y) - gy0;
                vec3 base{q.x - c.x, gy, q.y - c.y};
                b.tube(base - vec3(0, 0.3f, 0), base + vec3(0, H, 0), 0.09f, 0.07f, pole, 7, true);
                b.ellipsoid(base + vec3(0, H + 0.12f, 0), vec3(0.13f), gold, 6, 4);
                // a pennant on top of each mast
                b.triP(base + vec3(0, H - 0.05f, 0), base + vec3(0, H - 0.75f, 0), base + vec3(0.9f * std::cos(a + 1.f), H - 0.4f, 0.9f * std::sin(a + 1.f)), Mat(cols[i % 5], 0.8f, 0, 0, 0.8f));
                tops.push_back(base + vec3(0, H - 0.2f, 0));
            }
            // garlands: round the ring, and across the square (over the fountain)
            auto garland = [&](vec3 A, vec3 B, int seed) {
                float L = length(B - A); int n = std::max(6, (int)(L / 0.55f)); float sag = L * 0.09f;
                vec3 prev = A;
                for (int k = 1; k <= n; k++) {
                    float t = (float)k / n;
                    vec3 q = lerp(A, B, t) - vec3(0, sag * 4.f * t * (1 - t), 0);
                    b.tube(prev, q, 0.012f, 0.012f, rope, 3, false);
                    // a triangular pennant hanging under the rope
                    vec3 mid = (prev + q) * 0.5f, d = normalize(q - prev);
                    vec3 c0 = mid - d * 0.2f, c1 = mid + d * 0.2f, tip = mid - vec3(0, 0.42f, 0);
                    Mat f(cols[(k + seed) % 5], 0.8f, 0, 0, 0.9f);
                    b.triP(c0, c1, tip, f); b.triP(c1, c0, tip, f);
                    prev = q;
                }
            };
            for (int i = 0; i < NP; i++) garland(tops[i], tops[(i + 1) % NP], i);
            for (int i = 0; i < NP / 2; i++) garland(tops[i], tops[(i + NP / 2) % NP] + vec3(0, 0.6f, 0), i * 3);
        });
    }
    DrawItem d; d.mesh = m.get(); d.model = mat4::translate(origin); renderer.submit(d);
}

}  // namespace lk
