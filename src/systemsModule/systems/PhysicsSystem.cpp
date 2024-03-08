#include "PhysicsSystem.h"

#include "componentsModule/TransformComponent.h"
#include "Jolt/Renderer/DebugRenderer.h"
#include "renderModule/Utils.h"

namespace SFE::SystemsModule {

	using namespace JPH;

	void Physics::update(float dt) {
		auto& bodyInterface = physics_system->GetBodyInterface();
		for (const auto& [entity, physicsComp, transform] : ECSHandler::registry().forEach<PhysicsComponent, TransformComponent>()) {
			if (!transform) {
				continue;
			}
			const auto& pos = transform->getPos(true);
			if (pos != physicsComp->lastPos || transform->getRotate() != physicsComp->lastRotate) {
				bodyInterface.SetPositionAndRotationWhenChanged(physicsComp->mBodyID, toVec3(pos), toQuat(transform->getQuaternion()), EActivation::Activate);

				physicsComp->lastPos = pos;
			}

			if (bodyInterface.IsActive(physicsComp->mBodyID) == physicsComp->isSleeping) {
				continue;
			}

			physicsComp->lastPos = toVec3(bodyInterface.GetCenterOfMassPosition(physicsComp->mBodyID));
			transform->setPos(physicsComp->lastPos);

			physicsComp->lastRotate = Math::degrees(toVec3(bodyInterface.GetRotation(physicsComp->mBodyID).GetEulerAngles()));
			transform->setRotate(physicsComp->lastRotate);
		}

		// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
		//dt *= 0.5f;
		const int cCollisionSteps = 20 * static_cast<int>(std::ceil(dt * 60.f));
		if (!cCollisionSteps) {
			return;
		}
		// Step the world
		physics_system->Update(dt, cCollisionSteps, temp_allocator, job_system);
	}

	void Physics::debugUpdate(float dt) {
		/*class renderer : public DebugRenderer {
		public:
			renderer() {
				Initialize();
			}
			void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override {
				Render::Utils::renderLine(toVec3(inFrom), toVec3(inTo), Math::Vec4{ (float)inColor.r, (float)inColor.g, (float)inColor.b, (float)inColor.a});
			};

			void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow) override {
				Render::Utils::Triangle triangle;
				triangle.A.position = toVec3(inV1);
				triangle.B.position = toVec3(inV2);
				triangle.C.position = toVec3(inV3);
				Render::Utils::renderTriangle(triangle, Math::Vec4{ (float)inColor.r, (float)inColor.g, (float)inColor.b, (float)inColor.a});
			};

			Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override {
				return{};
			};

			Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const uint32* inIndices,
				int inIndexCount) override {
				return{};
			};

			void DrawGeometry(RMat44Arg inModelMatrix, const AABox& inWorldSpaceBounds, float inLODScaleSq,
				ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode, ECastShadow inCastShadow,
				EDrawMode inDrawMode) override {
				
			};

			void DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor,
				float inHeight) override {
				
			};


		};

		static renderer lel;
		BodyManager::DrawSettings settings;
		settings.mDrawShapeWireframe = false;
		settings.mDrawShape = true;
		settings.mDrawBoundingBox = true;
		settings.mDrawCenterOfMassTransform = true;
		settings.mDrawMassAndInertia = true;
		settings.mDrawSleepStats = true;
		settings.mDrawSupportDirection = true;
		physics_system->DrawBodies(settings, &lel, nullptr );*/
	}
}
