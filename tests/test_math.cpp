#include <doctest/doctest.h>

#include "agr/math/Geometry.hpp"
#include "agr/math/Math.hpp"

using namespace agr::math;

namespace {
bool matNear(const Mat4& a, const Mat4& b, float eps = 1e-4f) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!nearlyEqual(a[c][r], b[c][r], eps)) return false;
    return true;
}
}  // namespace

TEST_SUITE("math") {

TEST_CASE("vector basics") {
    CHECK(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}) == 32.0f);
    CHECK(cross(Vec3::right(), Vec3::up()) == Vec3{0, 0, 1});
    CHECK(nearlyEqual(length(normalize(Vec3{3, 4, 12})), 1.0f));
    CHECK(normalize(Vec3{}) == Vec3{});  // no NaN on zero vector
    CHECK(cross(Vec2{1, 0}, Vec2{0, 1}) == 1.0f);
}

TEST_CASE("quaternion rotates like its matrix") {
    const Quat q = Quat::fromAxisAngle({0.3f, 1.0f, -0.2f}, 1.234f);
    const Vec3 v{1.5f, -2.0f, 0.75f};
    CHECK(nearlyEqual(q * v, Mat4::rotate(q).transformVector(v)));
    const Quat y90 = Quat::fromAxisAngle({0, 1, 0}, radians(90));
    CHECK(nearlyEqual(y90 * Vec3{1, 0, 0}, Vec3{0, 0, -1}));  // right-handed
    CHECK(nearlyEqual((q * q.conjugate()), Quat::identity()));
}

TEST_CASE("euler round trip (YXZ)") {
    for (Vec3 e : {Vec3{0.3f, -1.2f, 0.4f}, Vec3{-0.7f, 2.5f, -1.1f}, Vec3{0, 0, 0}, Vec3{1.2f, 0.1f, 3.0f}}) {
        const Quat q = Quat::fromEuler(e);
        CHECK(nearlyEqual(Quat::fromEuler(q.toEuler()), q));
    }
}

TEST_CASE("slerp endpoints and midpoint") {
    const Quat a = Quat::identity();
    const Quat b = Quat::fromAxisAngle({0, 0, 1}, radians(90));
    CHECK(nearlyEqual(slerp(a, b, 0.0f), a));
    CHECK(nearlyEqual(slerp(a, b, 1.0f), b));
    CHECK(nearlyEqual(slerp(a, b, 0.5f), Quat::fromAxisAngle({0, 0, 1}, radians(45))));
}

TEST_CASE("inverse and decompose") {
    const Quat r = Quat::fromEuler({0.4f, -0.9f, 0.2f});
    const Mat4 m = Mat4::trs({3, -2, 5}, r, {2, 0.5f, 1.5f});
    CHECK(matNear(m * inverse(m), Mat4::identity()));
    Vec3 t, s;
    Quat q;
    decompose(m, t, q, s);
    CHECK(nearlyEqual(t, Vec3{3, -2, 5}));
    CHECK(nearlyEqual(s, Vec3{2, 0.5f, 1.5f}));
    CHECK(nearlyEqual(q, r));
    CHECK(matNear(Mat4::trs(t, q, s), m));
}

TEST_CASE("perspective maps near / far to the selected depth range") {
    const Mat4 vk = Mat4::perspective(radians(60), 16.0f / 9.0f, 0.1f, 100.0f);
    CHECK(nearlyEqual(vk.transformPoint({0, 0, -0.1f}).z, 0.0f));
    CHECK(nearlyEqual(vk.transformPoint({0, 0, -100.0f}).z, 1.0f));
    const Mat4 gl = Mat4::perspective(radians(60), 1.0f, 0.1f, 100.0f, Mat4::DepthRange::NegativeOneToOne);
    CHECK(nearlyEqual(gl.transformPoint({0, 0, -0.1f}).z, -1.0f));
    CHECK(nearlyEqual(gl.transformPoint({0, 0, -100.0f}).z, 1.0f));
}

TEST_CASE("orthographic maps the box to clip space") {
    const Mat4 o = Mat4::orthographic(0, 800, 0, 600, 0, 10);
    CHECK(nearlyEqual(o.transformPoint({0, 0, 0}), Vec3{-1, -1, 0}));
    CHECK(nearlyEqual(o.transformPoint({800, 600, -10}), Vec3{1, 1, 1}));
}

TEST_CASE("lookAt places the target in front of the camera") {
    const Mat4 v = Mat4::lookAt({0, 0, 5}, {0, 0, 0}, Vec3::up());
    CHECK(nearlyEqual(v.transformPoint({0, 0, 0}), Vec3{0, 0, -5}));
    const Mat4 v2 = Mat4::lookAt({10, 0, 0}, {0, 0, 0}, Vec3::up());
    CHECK(nearlyEqual(v2.transformPoint({0, 0, 0}), Vec3{0, 0, -10}));
}

TEST_CASE("frustum culling") {
    const Mat4 vp = Mat4::perspective(radians(60), 1.0f, 0.1f, 50.0f) * Mat4::lookAt({0, 0, 0}, {0, 0, -1}, Vec3::up());
    const Frustum f = Frustum::fromMatrix(vp);
    CHECK(f.intersects(Aabb::fromCenterExtents({0, 0, -10}, Vec3{1})));
    CHECK_FALSE(f.intersects(Aabb::fromCenterExtents({0, 0, 10}, Vec3{1})));    // behind
    CHECK_FALSE(f.intersects(Aabb::fromCenterExtents({0, 0, -80}, Vec3{1})));   // beyond far
    CHECK_FALSE(f.intersects(Aabb::fromCenterExtents({50, 0, -10}, Vec3{1})));  // far right
    CHECK(f.intersects(Aabb::fromCenterExtents({0, 0, -50.5f}, Vec3{1})));      // straddles far
}

TEST_CASE("ray / box intersection and box transform") {
    const Aabb box = Aabb::fromCenterExtents({0, 0, -10}, Vec3{1});
    CHECK(nearlyEqual(intersect(Ray{{0, 0, 0}, {0, 0, -1}}, box), 9.0f));
    CHECK(intersect(Ray{{0, 5, 0}, {0, 0, -1}}, box) < 0.0f);
    const Aabb moved = box.transformed(Mat4::trs({1, 0, 0}, Quat::fromAxisAngle({0, 1, 0}, radians(45)), Vec3{1}));
    CHECK(moved.valid());
    CHECK(nearlyEqual(moved.extents().x, std::sqrt(2.0f), 1e-3f));
    CHECK(moved.contains(Mat4::trs({1, 0, 0}, Quat::fromAxisAngle({0, 1, 0}, radians(45)), Vec3{1}).transformPoint(box.center())));
}

}  // TEST_SUITE
