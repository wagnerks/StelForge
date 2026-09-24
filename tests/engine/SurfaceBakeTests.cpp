#include <cmath>

#include <gtest/gtest.h>

#include "assetsModule/modelModule/SurfaceBake.h"

// Surfaces unrolled round a sphere (AssetsModule::bakeSurface): no device, no files -- meshes made here, without
// textures, so each is drawn in its material's colour.
namespace SFE::SurfaceBakeTests {
	using namespace SFE::Math;

	// A square facing +z, `half` across, `z` out, in one colour.
	MeshObject3D square(const float z, const float half, const Vec3& colour) {
		MeshObject3D object;
		for (const auto& [x, y] : { std::pair{ -half, -half }, std::pair{ half, -half }, std::pair{ half, half }, std::pair{ -half, half } }) {
			Vertex3D vertex;
			vertex.position = { x, y, z };
			vertex.normal = { 0.f, 0.f, 1.f };
			object.mesh.vertices.push_back(vertex);
		}
		object.mesh.indices = { 0, 1, 2, 0, 2, 3 };
		object.material.baseColor = colour;
		return object;
	}

	AssetsModule::SphereUnwrap aheadOfCentre() {
		AssetsModule::SphereUnwrap unwrap;
		unwrap.azimuth[0] = -20.f;
		unwrap.azimuth[1] = 20.f;
		unwrap.polar[0] = 70.f;
		unwrap.polar[1] = 110.f;
		return unwrap;
	}

	TEST(SurfaceBake, TheOuterOfTwoOverlappingSurfacesIsKept) {
		const auto inner = square(1.f, 0.1f, { 1.f, 0.f, 0.f });
		const auto outer = square(1.1f, 0.05f, { 0.f, 1.f, 0.f });
		// Outer first: the inner one drawn after it must not cover it.
		const auto baked = AssetsModule::bakeSurface({ &outer, &inner }, aheadOfCentre(), 40, 40, false);
		const auto at = [&](const int x, const int y) { return baked.colour.data() + (static_cast<size_t>(y) * 40 + x) * 4; };
		EXPECT_EQ(at(20, 20)[1], 255) << "the middle is the outer, green square";
		EXPECT_EQ(at(20, 20)[0], 0);
		EXPECT_NEAR(baked.radius[20 * 40 + 20], 1.1f, 0.01f);
		// The inner square reaches about 5.7 degrees out, the outer only 2.6: at 4.5 degrees only the inner one is.
		EXPECT_EQ(at(24, 20)[0], 255) << "beside the outer square, the inner, red one";
		EXPECT_EQ(at(0, 0)[3], 0) << "nothing in the corner";
		EXPECT_GT(baked.covered, 0.f);
		EXPECT_LT(baked.covered, 0.2f);
	}

	TEST(SurfaceBake, ANormalFacingOutIsStraightUpInTheMap) {
		const auto facing = square(1.f, 0.1f, { 1.f, 1.f, 1.f });
		const auto baked = AssetsModule::bakeSurface({ &facing }, aheadOfCentre(), 40, 40, false);
		const unsigned char* n = baked.normal.data() + (static_cast<size_t>(20) * 40 + 20) * 4;
		EXPECT_NEAR(n[0], 128, 2);
		EXPECT_NEAR(n[1], 128, 2);
		EXPECT_NEAR(n[2], 255, 2);
	}

	TEST(SurfaceBake, TheSlopeOfTheHeightTiltsTheNormal) {
		// A plane is further from the centre toward its edges, so from its height a texel right of the middle slopes
		// up to the right, and its normal leans back toward the middle: -x in the map.
		const auto facing = square(1.f, 0.2f, { 1.f, 1.f, 1.f });
		const auto baked = AssetsModule::bakeSurface({ &facing }, aheadOfCentre(), 40, 40, true);
		const unsigned char* middle = baked.normal.data() + (static_cast<size_t>(20) * 40 + 20) * 4;
		const unsigned char* right = baked.normal.data() + (static_cast<size_t>(20) * 40 + 30) * 4;
		EXPECT_NEAR(middle[0], 128, 3);
		EXPECT_LT(right[0], 120);
	}

	TEST(SurfaceBake, TheFittedSphereIsTheOneThePointsLieOn) {
		MeshObject3D ball;
		for (int i = 0; i < 200; ++i) {
			const float a = static_cast<float>(i) * 0.37f;
			const float b = static_cast<float>(i) * 0.61f;
			Vertex3D vertex;
			vertex.position = Vec3{ 1.f, 2.f, 3.f } + Vec3{ std::sin(a) * std::cos(b), std::cos(a), std::sin(a) * std::sin(b) } * 0.5f;
			ball.mesh.vertices.push_back(vertex);
		}
		const auto sphere = AssetsModule::fitSphere({ &ball });
		EXPECT_NEAR(sphere.x, 1.f, 1e-3f);
		EXPECT_NEAR(sphere.y, 2.f, 1e-3f);
		EXPECT_NEAR(sphere.z, 3.f, 1e-3f);
		EXPECT_NEAR(sphere.w, 0.5f, 1e-3f);
	}
}
