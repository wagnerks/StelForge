#include "SurfaceBake.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>

#include "assetsModule/TextureHandler.h"
#include "mathModule/VectorOperations.h"

namespace AssetsModule {
	namespace {
		using SFE::Math::Vec2;
		using SFE::Math::Vec3;
		using SFE::Math::Vec4;

		constexpr float kDegrees = 57.2957795f;

		struct Picture {
			std::vector<unsigned char> pixels;
			int width = 0;
			int height = 0;
		};

		// Bilinear, repeating: as the texture is drawn.
		Vec4 sample(const Picture& picture, const Vec2& uv) {
			const float x = uv.x * static_cast<float>(picture.width) - 0.5f;
			const float y = uv.y * static_cast<float>(picture.height) - 0.5f;
			const float fx = std::floor(x);
			const float fy = std::floor(y);
			const auto texel = [&](int tx, int ty) {
				tx = ((tx % picture.width) + picture.width) % picture.width;
				ty = ((ty % picture.height) + picture.height) % picture.height;
				const unsigned char* p = picture.pixels.data() + (static_cast<size_t>(ty) * picture.width + tx) * 4;
				return Vec4{ p[0] / 255.f, p[1] / 255.f, p[2] / 255.f, p[3] / 255.f };
			};
			const int x0 = static_cast<int>(fx);
			const int y0 = static_cast<int>(fy);
			const float ax = x - fx;
			const float ay = y - fy;
			return (texel(x0, y0) * (1.f - ax) + texel(x0 + 1, y0) * ax) * (1.f - ay)
				+ (texel(x0, y0 + 1) * (1.f - ax) + texel(x0 + 1, y0 + 1) * ax) * ay;
		}

		struct Frame {
			Vec3 centre;
			Vec3 up;
			Vec3 front;
			Vec3 right;
		};

		// A point's azimuth and polar angle, degrees, and its distance from the centre.
		Vec3 anglesOf(const Frame& frame, const Vec3& point) {
			const Vec3 d = point - frame.centre;
			const float r = SFE::Math::length(d);
			if (r < 1e-9f) {
				return { 0.f, 0.f, 0.f };
			}
			const float azimuth = std::atan2(SFE::Math::dot(d, frame.right), SFE::Math::dot(d, frame.front)) * kDegrees;
			const float polar = std::acos(std::clamp(SFE::Math::dot(d, frame.up) / r, -1.f, 1.f)) * kDegrees;
			return { azimuth, polar, r };
		}

		// Out from the centre at these angles, and the way the azimuth and the polar angle grow there.
		void directionAt(const Frame& frame, const float azimuth, const float polar, Vec3& out, Vec3& across, Vec3& down) {
			const float a = azimuth / kDegrees;
			const float p = polar / kDegrees;
			out = frame.up * std::cos(p) + (frame.front * std::cos(a) + frame.right * std::sin(a)) * std::sin(p);
			across = frame.right * std::cos(a) - frame.front * std::sin(a);
			down = SFE::Math::cross(across, out);
		}

		float edge(const Vec2& a, const Vec2& b, const Vec2& p) {
			return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
		}
	}

	BakedSurface bakeSurface(const std::vector<const SFE::MeshObject3D*>& meshes, const SphereUnwrap& unwrap, const int width, const int height,
		const bool normalsFromHeight) {
		BakedSurface baked;
		baked.width = width;
		baked.height = height;
		const size_t texels = static_cast<size_t>(width) * height;
		baked.colour.assign(texels * 4, 0);
		baked.normal.assign(texels * 4, 0);
		baked.radius.assign(texels, 0.f);
		std::vector<Vec3> normals(texels, Vec3{ 0.f });
		if (width <= 0 || height <= 0) {
			return baked;
		}

		Frame frame;
		frame.centre = unwrap.centre;
		frame.up = SFE::Math::normalize(unwrap.up);
		frame.front = SFE::Math::normalize(unwrap.front - frame.up * SFE::Math::dot(unwrap.front, frame.up));
		frame.right = SFE::Math::cross(frame.up, frame.front);
		const float azimuthSpan = unwrap.azimuth[1] - unwrap.azimuth[0];
		const float polarSpan = unwrap.polar[1] - unwrap.polar[0];
		const auto toPixel = [&](const float azimuth, const float polar) {
			return Vec2{ (azimuth - unwrap.azimuth[0]) / azimuthSpan * static_cast<float>(width),
				(polar - unwrap.polar[0]) / polarSpan * static_cast<float>(height) };
		};

		std::map<std::string, Picture> pictures;
		for (const auto* mesh : meshes) {
			if (!mesh) {
				continue;
			}
			const auto& material = mesh->material;
			const Picture* picture = nullptr;
			int uvSet = 0;
			if (const auto it = material.materialTextures.find(SFE::DIFFUSE); it != material.materialTextures.end() && !it->second.path.empty()) {
				auto [found, made] = pictures.try_emplace(it->second.path);
				if (made) {
					found->second.pixels = TextureHandler::readPixels(it->second.path, found->second.width, found->second.height);
				}
				if (!found->second.pixels.empty()) {
					picture = &found->second;
				}
				uvSet = it->second.uvSet;
			}
			const float cutoff = material.alphaCutoff > 0.f ? material.alphaCutoff : 0.5f;
			const auto& vertices = mesh->mesh.vertices;
			const auto& indices = mesh->mesh.indices;
			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				const std::array<const SFE::Vertex3D*, 3> corner{ &vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]] };
				std::array<Vec3, 3> angle;
				for (int k = 0; k < 3; ++k) {
					angle[k] = anglesOf(frame, corner[k]->position);
				}
				// One that crosses the seam behind (azimuth +-180) is taken whole on one side of it, and drawn again a
				// turn round, for a picture that reaches past the seam.
				const float least = (std::min)({ angle[0].x, angle[1].x, angle[2].x });
				const float most = (std::max)({ angle[0].x, angle[1].x, angle[2].x });
				if (most - least > 180.f) {
					for (auto& a : angle) {
						if (a.x < 0.f) {
							a.x += 360.f;
						}
					}
				}
				for (const float turn : { 0.f, 360.f, -360.f }) {
					std::array<Vec2, 3> at;
					for (int k = 0; k < 3; ++k) {
						at[k] = toPixel(angle[k].x + turn, angle[k].y);
					}
					const float area = edge(at[0], at[1], at[2]);
					if (std::abs(area) < 1e-12f) {
						continue;
					}
					const int x0 = (std::max)(0, static_cast<int>(std::floor((std::min)({ at[0].x, at[1].x, at[2].x }))));
					const int x1 = (std::min)(width - 1, static_cast<int>(std::ceil((std::max)({ at[0].x, at[1].x, at[2].x }))));
					const int y0 = (std::max)(0, static_cast<int>(std::floor((std::min)({ at[0].y, at[1].y, at[2].y }))));
					const int y1 = (std::min)(height - 1, static_cast<int>(std::ceil((std::max)({ at[0].y, at[1].y, at[2].y }))));
					for (int y = y0; y <= y1; ++y) {
						for (int x = x0; x <= x1; ++x) {
							const Vec2 p{ static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f };
							const float w0 = edge(at[1], at[2], p) / area;
							const float w1 = edge(at[2], at[0], p) / area;
							const float w2 = 1.f - w0 - w1;
							if (w0 < 0.f || w1 < 0.f || w2 < 0.f) {
								continue;
							}
							// The point itself, not its distance, taken between the corners: a flat triangle is further out
							// toward its edges than at its corners' average.
							const float r = SFE::Math::length(corner[0]->position * w0 + corner[1]->position * w1 + corner[2]->position * w2 - frame.centre);
							const size_t texel = static_cast<size_t>(y) * width + x;
							if (r <= baked.radius[texel]) {
								continue;
							}
							Vec4 colour{ 1.f, 1.f, 1.f, 1.f };
							if (picture) {
								const auto uvOf = [&](const SFE::Vertex3D& v) { return uvSet == 1 ? v.texCoords1 : v.texCoords; };
								const Vec2 uv = uvOf(*corner[0]) * w0 + uvOf(*corner[1]) * w1 + uvOf(*corner[2]) * w2;
								colour = sample(*picture, uv);
								if (colour.w < cutoff) {
									continue;
								}
							}
							baked.radius[texel] = r;
							unsigned char* out = baked.colour.data() + texel * 4;
							out[0] = static_cast<unsigned char>(std::clamp(colour.x * material.baseColor.x, 0.f, 1.f) * 255.f + 0.5f);
							out[1] = static_cast<unsigned char>(std::clamp(colour.y * material.baseColor.y, 0.f, 1.f) * 255.f + 0.5f);
							out[2] = static_cast<unsigned char>(std::clamp(colour.z * material.baseColor.z, 0.f, 1.f) * 255.f + 0.5f);
							out[3] = 255;
							normals[texel] = corner[0]->normal * w0 + corner[1]->normal * w1 + corner[2]->normal * w2;
						}
					}
				}
			}
		}

		// The normals, into the unwrap's own frame at each texel; the heights, into the normal map's alpha.
		size_t drawn = 0;
		baked.radiusMin = 1e30f;
		baked.radiusMax = 0.f;
		for (const float r : baked.radius) {
			if (r > 0.f) {
				++drawn;
				baked.radiusMin = (std::min)(baked.radiusMin, r);
				baked.radiusMax = (std::max)(baked.radiusMax, r);
			}
		}
		baked.covered = static_cast<float>(drawn) / static_cast<float>(texels);
		if (!drawn) {
			baked.radiusMin = 0.f;
			return baked;
		}
		const float perColumn = azimuthSpan / static_cast<float>(width) / kDegrees;
		const float perRow = polarSpan / static_cast<float>(height) / kDegrees;
		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				const size_t texel = static_cast<size_t>(y) * width + x;
				const float r = baked.radius[texel];
				if (r <= 0.f) {
					continue;
				}
				const float azimuth = unwrap.azimuth[0] + (static_cast<float>(x) + 0.5f) * azimuthSpan / static_cast<float>(width);
				const float polar = unwrap.polar[0] + (static_cast<float>(y) + 0.5f) * polarSpan / static_cast<float>(height);
				Vec3 tangentSpace{ 0.f, 0.f, 1.f };
				if (normalsFromHeight) {
					// The slope of the outermost surface, where the texels either side have one.
					const auto radiusAt = [&](const int tx, const int ty) {
						return tx < 0 || ty < 0 || tx >= width || ty >= height ? 0.f : baked.radius[static_cast<size_t>(ty) * width + tx];
					};
					const auto slope = [&](const float before, const float after, const float step) {
						if (before > 0.f && after > 0.f) {
							return (after - before) / (2.f * step);
						}
						if (after > 0.f) {
							return (after - r) / step;
						}
						return before > 0.f ? (r - before) / step : 0.f;
					};
					const float across = slope(radiusAt(x - 1, y), radiusAt(x + 1, y), (std::max)(r * std::sin(polar / kDegrees) * perColumn, 1e-6f));
					const float down = slope(radiusAt(x, y - 1), radiusAt(x, y + 1), (std::max)(r * perRow, 1e-6f));
					tangentSpace = SFE::Math::normalize(Vec3{ -across, -down, 1.f });
				}
				else if (SFE::Math::length(normals[texel]) > 1e-6f) {
					Vec3 out;
					Vec3 acrossDir;
					Vec3 downDir;
					directionAt(frame, azimuth, polar, out, acrossDir, downDir);
					const Vec3 n = SFE::Math::normalize(normals[texel]);
					tangentSpace = SFE::Math::normalize(Vec3{ SFE::Math::dot(n, acrossDir), SFE::Math::dot(n, downDir), SFE::Math::dot(n, out) });
				}
				unsigned char* out = baked.normal.data() + texel * 4;
				out[0] = static_cast<unsigned char>((tangentSpace.x * 0.5f + 0.5f) * 255.f + 0.5f);
				out[1] = static_cast<unsigned char>((tangentSpace.y * 0.5f + 0.5f) * 255.f + 0.5f);
				out[2] = static_cast<unsigned char>((tangentSpace.z * 0.5f + 0.5f) * 255.f + 0.5f);
				const float span = baked.radiusMax - baked.radiusMin;
				out[3] = static_cast<unsigned char>(span > 0.f ? 1.f + (r - baked.radiusMin) / span * 254.f + 0.5f : 255.f);
			}
		}
		return baked;
	}

	SFE::Math::Vec4 fitSphere(const std::vector<const SFE::MeshObject3D*>& meshes) {
		// |p|^2 = 2 c.p + k over every vertex, least squares: the normal equations, four by four.
		double a[4][5]{};
		for (const auto* mesh : meshes) {
			if (!mesh) {
				continue;
			}
			for (const auto& vertex : mesh->mesh.vertices) {
				const double row[4]{ 2.0 * vertex.position.x, 2.0 * vertex.position.y, 2.0 * vertex.position.z, 1.0 };
				const double rhs = static_cast<double>(SFE::Math::dot(vertex.position, vertex.position));
				for (int i = 0; i < 4; ++i) {
					for (int j = 0; j < 4; ++j) {
						a[i][j] += row[i] * row[j];
					}
					a[i][4] += row[i] * rhs;
				}
			}
		}
		for (int col = 0; col < 4; ++col) {
			int pivot = col;
			for (int r = col + 1; r < 4; ++r) {
				if (std::abs(a[r][col]) > std::abs(a[pivot][col])) {
					pivot = r;
				}
			}
			if (std::abs(a[pivot][col]) < 1e-12) {
				return SFE::Math::Vec4{ 0.f };
			}
			std::swap(a[col], a[pivot]);
			for (int r = 0; r < 4; ++r) {
				if (r != col) {
					const double f = a[r][col] / a[col][col];
					for (int c = col; c < 5; ++c) {
						a[r][c] -= f * a[col][c];
					}
				}
			}
		}
		const double cx = a[0][4] / a[0][0];
		const double cy = a[1][4] / a[1][1];
		const double cz = a[2][4] / a[2][2];
		const double k = a[3][4] / a[3][3];
		return { static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz),
			static_cast<float>(std::sqrt((std::max)(0.0, k + cx * cx + cy * cy + cz * cz))) };
	}
}
