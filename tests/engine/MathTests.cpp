#include <cmath>
#include <map>

#include <gtest/gtest.h>

#include "mathModule/Forward.h"
#include "mathModule/Utils.h"

// Pure math: no engine state, checked by invariants and hand-computed values rather than another library.
namespace SFE::MathTests {
	using namespace SFE::Math;

	constexpr float kEps = 1e-4f;

	template <typename T, size_t N>
	void expectNear(const Vector<T, N>& actual, const Vector<T, N>& expected, T eps = static_cast<T>(kEps)) {
		for (size_t i = 0; i < N; ++i) {
			EXPECT_NEAR(actual[i], expected[i], eps) << "component " << i;
		}
	}

	template <typename T, size_t R, size_t C>
	void expectNear(const Matrix<T, R, C>& actual, const Matrix<T, R, C>& expected, T eps = static_cast<T>(kEps)) {
		for (size_t c = 0; c < C; ++c) {
			for (size_t r = 0; r < R; ++r) {
				EXPECT_NEAR(actual[c][r], expected[c][r], eps) << "column " << c << " row " << r;
			}
		}
	}

	// q and -q are the same rotation
	template <typename T>
	void expectSameRotation(const Quaternion<T>& a, const Quaternion<T>& b, T eps = static_cast<T>(kEps)) {
		const T d = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
		EXPECT_NEAR(std::abs(d), T{1}, eps);
	}

	template <typename T>
	Quaternion<T> axisAngle(const Vector<T, 3>& axis, T angle) {
		return { std::cos(angle / 2), normalize(axis) * std::sin(angle / 2) };
	}

	const Mat4 kTrs = translate(Mat4{1.f}, Vec3{1.f, -2.f, 3.f}) * axisAngle(Vec3{1.f, 2.f, 3.f}, 0.7f).toMat4() * scale(Mat4{1.f}, Vec3{2.f, 0.5f, 3.f});

	TEST(MathVector, Arithmetic) {
		Vec3 a{1.f, 2.f, 3.f};
		const Vec3 b{4.f, 5.f, 6.f};
		expectNear(a + b, {5.f, 7.f, 9.f});
		expectNear(a - b, {-3.f, -3.f, -3.f});
		expectNear(a * b, {4.f, 10.f, 18.f});
		expectNear(b / a, {4.f, 2.5f, 2.f});
		expectNear(a * 2.f, {2.f, 4.f, 6.f});
		expectNear(-a, {-1.f, -2.f, -3.f});
		EXPECT_FLOAT_EQ(dot(a, b), 32.f);
		expectNear(cross(Vec3{1.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}), {0.f, 0.f, 1.f});
		EXPECT_FLOAT_EQ(length(Vec3{3.f, 4.f, 0.f}), 5.f);
		EXPECT_FLOAT_EQ(distance(a, b), std::sqrt(27.f));
	}

	TEST(MathVector, CompoundAssignmentReturnsTheVector) {
		Vec3 a{1.f};
		EXPECT_EQ(&(a += Vec3{1.f}), &a);
		EXPECT_EQ(&(a *= 2.f), &a);
		(a -= 1.f) /= 3.f;
		expectNear(a, {1.f, 1.f, 1.f});
	}

	TEST(MathVector, NormalizeZeroStaysZero) {
		expectNear(normalize(Vec3{}), Vec3{});
		expectNear(normalize(Vec3{0.f, 3.f, 4.f}), {0.f, 0.6f, 0.8f});
	}

	TEST(MathVector, ConversionsKeepComponents) {
		const Vec4 v4{Vec3{1.f, 2.f, 3.f}, 4};
		expectNear(v4, {1.f, 2.f, 3.f, 4.f});
		expectNear(Vec3{v4}, {1.f, 2.f, 3.f});
		expectNear(v4.xyz, {1.f, 2.f, 3.f});
		expectNear(Vec2{DVec3{1.5, 2.5, 3.5}}, {1.5f, 2.5f});
		EXPECT_EQ((IVec2{Vec2{1.9f, -1.9f}}), (IVec2{1, -1}));
	}

	TEST(MathVector, OrderingIsStrictAndConsistent) {
		const IVec3 a{1, 5, 0};
		const IVec3 b{2, 0, 0};
		EXPECT_TRUE(a < b);
		EXPECT_TRUE(b > a);
		EXPECT_FALSE(a > b);
		EXPECT_FALSE(a < a);
		EXPECT_FALSE(a > a);
		EXPECT_TRUE((IVec3{1, 2, 3}) < (IVec3{1, 2, 4}));

		std::map<IVec3, int> keyed{{a, 1}, {b, 2}, {IVec3{1, 5, 1}, 3}};
		EXPECT_EQ(keyed.size(), 3u);
		EXPECT_EQ(keyed.begin()->second, 1);
	}

	TEST(MathVector, RotateAroundAxis) {
		expectNear(rotate(Vec3{1.f, 0.f, 0.f}, pi<float>() / 2, Vec3{0.f, 0.f, 1.f}), {0.f, 1.f, 0.f});
		const Vec3 axis = normalize(Vec3{1.f, 1.f, 1.f});
		// a third of a turn around the diagonal cycles the basis
		expectNear(rotate(Vec3{1.f, 0.f, 0.f}, twoPi<float>() / 3, axis), {0.f, 1.f, 0.f});
	}

	TEST(MathScalar, AngleConversions) {
		static_assert(pi<double>() > 3.14159 && pi<double>() < 3.1416);
		static_assert(radians(180.0) - pi<double>() < 1e-12 && pi<double>() - radians(180.0) < 1e-12);
		EXPECT_NEAR(degrees(radians(37.f)), 37.f, kEps);
		expectNear(degrees(Vec3{pi<float>()}), Vec3{180.f});
	}

	TEST(MathMatrix, MultiplySquareMatchesDefinition) {
		const Mat4 a = kTrs;
		const Mat4 b = perspectiveRH_NO(1.f, 1.5f, 0.1f, 100.f);
		const Mat4 product = a * b;
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				float expected = 0.f;
				for (size_t k = 0; k < 4; ++k) {
					expected += a[k][r] * b[c][k];
				}
				EXPECT_NEAR(product[c][r], expected, kEps);
			}
		}
		expectNear(Mat4{1.f} * a, a);
		expectNear(Mat3{a} * Mat3{1.f}, Mat3{a});
	}

	TEST(MathMatrix, NonSquareMultiplyAndTranspose) {
		// 2 rows x 3 columns, stored by column
		const Matrix<float, 2, 3> a{{Vec2{1.f, 4.f}, Vec2{2.f, 5.f}, Vec2{3.f, 6.f}}};
		const Matrix<float, 3, 2> b = transpose(a);
		expectNear(b[0], Vec3{1.f, 2.f, 3.f});
		expectNear(b[1], Vec3{4.f, 5.f, 6.f});

		const Matrix<float, 2, 2> ab = a * b;
		expectNear(ab[0], Vec2{14.f, 32.f});
		expectNear(ab[1], Vec2{32.f, 77.f});

		const Matrix<float, 3, 3> ba = b * a;
		expectNear(ba[0], Vec3{17.f, 22.f, 27.f});
		expectNear(ba[2], Vec3{27.f, 36.f, 45.f});

		expectNear(a * Vec3{1.f, 0.f, -1.f}, Vec2{-2.f, -2.f});
	}

	TEST(MathMatrix, InverseUndoesTheMatrix) {
		expectNear(inverse(kTrs) * kTrs, Mat4{1.f});
		expectNear(kTrs * inverse(kTrs), Mat4{1.f});

		const Mat4 projection = perspectiveRH_NO(1.f, 1.5f, 0.1f, 100.f);
		expectNear(inverse(projection) * projection, Mat4{1.f});

		const DMat3 m{DVec3{2., 0., 1.}, DVec3{1., 3., 0.}, DVec3{0., 1., 4.}};
		expectNear(inverse(m) * m, DMat3{1.}, 1e-9);
	}

	TEST(MathMatrix, TransformsPoints) {
		const Vec3 p{1.f, 0.f, 0.f};
		expectNear(translate(Mat4{1.f}, Vec3{1.f, 2.f, 3.f}) * p, {2.f, 2.f, 3.f});
		expectNear(scale(Mat4{1.f}, Vec3{2.f, 3.f, 4.f}) * Vec3{1.f}, {2.f, 3.f, 4.f});
		expectNear(rotate(Mat4{1.f}, pi<float>() / 2, Vec3{0.f, 0.f, 1.f}) * p, {0.f, 1.f, 0.f});
		// Mat4 * Vec3 treats it as a point, Mat4 * Vec4 honours w
		expectNear(kTrs * p, Vec3{kTrs * Vec4{p, 1.f}});
		expectNear(Vec3{kTrs * Vec4{p, 0.f}} + Vec3{kTrs[3]}, kTrs * p);
	}

	TEST(MathMatrix, Transforms2DMatch3D) {
		const Mat3 m2 = scale(rotate(translate(Mat3{1.f}, Vec2{3.f, -1.f}), 0.6f), Vec2{2.f, 0.5f});
		const Mat4 m3 = scale(rotate(translate(Mat4{1.f}, Vec3{3.f, -1.f, 0.f}), 0.6f, Vec3{0.f, 0.f, 1.f}), Vec3{2.f, 0.5f, 1.f});
		const Vec2 p{0.3f, -0.7f};
		expectNear(m2 * p, Vec2{m3 * Vec3{p, 0.f}});
		expectNear(rotate(Mat3{1.f}, pi<float>() / 2) * Vec2{1.f, 0.f}, Vec2{0.f, 1.f});
	}

	TEST(MathMatrix, ProjectionsMapNearAndFar) {
		const float n = 0.5f;
		const float f = 50.f;
		const auto depth = [](const Mat4& projection, float viewZ) {
			const Vec4 clip = projection * Vec4{0.f, 0.f, viewZ, 1.f};
			return clip.z / clip.w;
		};
		const Mat4 perspectiveNO = perspectiveRH_NO(1.f, 1.f, n, f);
		EXPECT_NEAR(depth(perspectiveNO, -n), -1.f, kEps);
		EXPECT_NEAR(depth(perspectiveNO, -f), 1.f, kEps);
		const Mat4 perspectiveZO = perspectiveRH_ZO(1.f, 1.f, n, f);
		EXPECT_NEAR(depth(perspectiveZO, -n), 0.f, kEps);
		EXPECT_NEAR(depth(perspectiveZO, -f), 1.f, kEps);
		EXPECT_NEAR(depth(orthoRH_NO(-1.f, 1.f, -1.f, 1.f, n, f), -f), 1.f, kEps);
		EXPECT_NEAR(depth(orthoRH_ZO(-1.f, 1.f, -1.f, 1.f, n, f), -n), 0.f, kEps);
	}

	TEST(MathQuaternion, MatrixMatchesAxisAngle) {
		const Vec3 axis{1.f, 2.f, 3.f};
		const Quat q = axisAngle(axis, 0.7f);
		expectNear(q.toMat4(), rotate(Mat4{1.f}, 0.7f, axis));
		expectNear(q.toMat4()[3], Vec4{0.f, 0.f, 0.f, 1.f});
		expectNear(Mat3{q.toMat4()}, q.toRotateMatrix3());
	}

	TEST(MathQuaternion, RotateVectorMatchesMatrix) {
		const Quat q = axisAngle(Vec3{-1.f, 0.5f, 2.f}, 2.3f);
		const Vec3 v{0.3f, -4.f, 1.5f};
		expectNear(q.rotateVector(v), q.toRotateMatrix3() * v);
		expectNear(q.globalToLocal(q.rotateVector(v)), v);

		const Quat r = axisAngle(Vec3{0.f, 1.f, 0.f}, -1.1f);
		expectNear((q * r).rotateVector(v), q.rotateVector(r.rotateVector(v)));
	}

	TEST(MathQuaternion, MatrixRoundTripsInEveryBranch) {
		// small angle hits the trace branch; half turns hit the largest-diagonal branches
		const Vec3 axes[] = {{1.f, 2.f, 3.f}, {1.f, 0.1f, 0.2f}, {0.1f, 1.f, 0.2f}, {0.1f, 0.2f, 1.f}};
		for (const auto& axis : axes) {
			for (float angle : {0.4f, 3.0f}) {
				const Quat q = axisAngle(axis, angle);
				Quat back;
				back.matrixToQuaternion(q.toRotateMatrix3());
				expectSameRotation(back, q);
			}
		}
	}

	TEST(MathQuaternion, EulerRoundTrip) {
		const Vec3 radiansIn{0.3f, -0.8f, 1.2f};
		const Quat q(radiansIn.x, radiansIn.y, radiansIn.z);
		expectNear(q.toEuler(), radiansIn);

		Quat fromDegrees;
		fromDegrees.eulerToQuaternion(degrees(radiansIn));
		expectSameRotation(fromDegrees, q);
	}

	TEST(MathQuaternion, EulerAtGimbalLockIsFinite) {
		const Quat q = axisAngle(Vec3{0.f, 1.f, 0.f}, pi<float>() / 2);
		const Vec3 euler = q.toEuler();
		EXPECT_TRUE(std::isfinite(euler.x) && std::isfinite(euler.y) && std::isfinite(euler.z));
		EXPECT_NEAR(euler.y, pi<float>() / 2, 1e-3f);
	}

	TEST(MathQuaternion, FromToRotation) {
		const Vec3 from = normalize(Vec3{1.f, 2.f, -1.f});
		const Vec3 to = normalize(Vec3{-3.f, 0.5f, 2.f});
		expectNear(Quat(to, from).rotateVector(from), to);
		expectSameRotation(Quat(from, from), Quat{});

		const Quat halfTurn(-from, from);
		EXPECT_NEAR(halfTurn.magnitude(), 1.f, kEps);
		expectNear(halfTurn.rotateVector(from), -from);
	}

	TEST(MathQuaternion, Slerp) {
		const Quat a = axisAngle(Vec3{0.f, 0.f, 1.f}, 0.2f);
		const Quat b = axisAngle(Vec3{0.f, 0.f, 1.f}, 1.4f);
		expectSameRotation(slerp(a, b, 0.f), a);
		expectSameRotation(slerp(a, b, 1.f), b);
		expectSameRotation(slerp(a, b, 0.5f), axisAngle(Vec3{0.f, 0.f, 1.f}, 0.8f));

		// takes the short way even when b is stored negated
		const Quat negated{-b.w, -b.x, -b.y, -b.z};
		expectSameRotation(slerp(a, negated, 0.5f), axisAngle(Vec3{0.f, 0.f, 1.f}, 0.8f));

		const Quaternion<double> da{1., 0., 0., 0.};
		const Quaternion<double> db = axisAngle(DVec3{1., 0., 0.}, 1.);
		expectSameRotation(slerp(da, db, 0.5), axisAngle(DVec3{1., 0., 0.}, 0.5), 1e-9);
	}

	TEST(MathQuaternion, AxisOfRotation) {
		expectNear(axisAngle(Vec3{0.f, 3.f, 4.f}, 1.f).getAxis(), {0.f, 0.6f, 0.8f});
	}
}
