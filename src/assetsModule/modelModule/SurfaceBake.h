#pragma once
#include <cstdint>
#include <vector>

#include "Mesh.h"
#include "mathModule/Forward.h"

namespace AssetsModule {
	// A surface laid flat by a sphere's angles round `centre`: across, a point's angle about `up` from `front`, growing
	// to the right as the surface is seen from outside (toward up x front); down, its angle from `up`. The picture
	// covers `azimuth` and `polar`, degrees: its top row polar[0], its left column azimuth[0].
	struct SphereUnwrap {
		SFE::Math::Vec3 centre{ 0.f };
		SFE::Math::Vec3 up{ 0.f, 1.f, 0.f };
		SFE::Math::Vec3 front{ 0.f, 0.f, 1.f };
		float azimuth[2]{ -180.f, 180.f };
		float polar[2]{ 0.f, 180.f };
	};

	// What bakeSurface draws, row by row from the top.
	struct BakedSurface {
		int width = 0;
		int height = 0;
		std::vector<uint8_t> colour;   // RGBA8: the texture's colour times the material's, alpha 255 where anything is
		std::vector<uint8_t> normal;   // RGBA8: tangent space (+x across, +y down the picture, +z out), alpha the height
		std::vector<float> radius;     // how far from the centre the outermost surface is; 0 where nothing is
		float radiusMin = 0.f;         // the range the normal's alpha spans
		float radiusMax = 0.f;
		float covered = 0.f;           // the share of the picture anything was drawn in
	};

	// Everything of `meshes` drawn into the unwrap, on the CPU -- the same on every backend, and without a device. Each
	// triangle with its material's albedo texture (read from its file) and cut out where the texture's alpha is under
	// the material's cut-off (a half, if it has none); where two overlap, the one further from the centre is kept,
	// as it is the one seen from outside. Positions are the meshes' own (a skinned mesh's at rest).
	//
	// The normals are the meshes' own, or, with `normalsFromHeight`, the slope of the outermost surface: what
	// separates one card of hair from the one it lies over even when the cards' normals were all smoothed alike.
	BakedSurface bakeSurface(const std::vector<const SFE::MeshObject3D*>& meshes, const SphereUnwrap& unwrap, int width, int height,
		bool normalsFromHeight);

	// The sphere nearest the vertices of `meshes` (least squares): a centre for an unwrap round them. Its radius in w.
	SFE::Math::Vec4 fitSphere(const std::vector<const SFE::MeshObject3D*>& meshes);
}
