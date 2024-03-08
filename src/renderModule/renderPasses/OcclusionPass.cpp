#include "OcclusionPass.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "glWrapper/Depth.h"
#include "glWrapper/ViewportStack.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render::RenderPasses {
	OcclusionPass::~OcclusionPass() {}

	void OcclusionPass::init() {
		depthTex.generate();
		depthTex.parameters.minFilter = GLW::TextureMinFilter::LINEAR;
		depthTex.parameters.magFilter = GLW::TextureMagFilter::LINEAR;
		depthTex.parameters.wrap.S = GLW::TextureWrap::CLAMP_TO_EDGE;
		depthTex.parameters.wrap.T = GLW::TextureWrap::CLAMP_TO_EDGE;
		depthTex.parameters.compareMode = GLW::TextureCompareMode::COMPARE_REF_TO_TEXTURE;
		depthTex.parameters.compareFunc = GLW::CompareFunc::LESS;
		depthTex.width = static_cast<int>(w);
		depthTex.height = static_cast<int>(h);
		depthTex.pixelFormat = GLW::DEPTH_COMPONENT32;
		depthTex.textureFormat = GLW::DEPTH_COMPONENT;
		depthTex.pixelType = GLW::FLOAT;
		depthTex.create2D();

	
		occlusionFrameBuffer.bind();
		occlusionFrameBuffer.addAttachmentTexture(GLW::AttachmentType::DEPTH, &depthTex);
		occlusionFrameBuffer.setDrawBuffer(GLW::NONE);
		occlusionFrameBuffer.setReadBuffer(GLW::NONE);
		occlusionFrameBuffer.finalize();
	}

	void OcclusionPass::render(SystemsModule::RenderData& renderDataHandle) {
		//todo update it only for objects with dirty transforms, or if camera moved (especially for shadows)
		SFE::Vector<unsigned> entities;
		{
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			std::mutex addMtx;
			auto aabbOctrees = octreeSys->getAABBOctrees(renderDataHandle.mNextCamFrustum.generateAABB());
			ThreadPool::instance()->addBatchTasks(aabbOctrees.size(), 5, [aabbOctrees, octreeSys, &addMtx, &renderDataHandle, &entities](size_t it)mutable {
				if (auto treeIt = octreeSys->getOctree(aabbOctrees[it])) {
					auto lock = treeIt->readLock();
					treeIt->forEachObjectInFrustum(renderDataHandle.mNextCamFrustum, [&entities, &renderDataHandle, &addMtx](const auto& obj) {
						if (FrustumModule::AABB::isOnFrustum(renderDataHandle.mNextCamFrustum, obj.pos, obj.size)) {
							std::unique_lock lock(addMtx);
							entities.emplace_back(obj.data.getID());
						}
					});
				}

			}).waitAll();
		}

		if (entities.empty()) {
			return;
		}
		entities.sort();
		
		GLW::ViewportStack::push({ {static_cast<int>(w), static_cast<int>(h)} });
		occlusionFrameBuffer.bind();
		GLW::clear(GLW::ColorBit::DEPTH);

		const auto simpleDepthShader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/occlusion.vs", "shaders/occlusion.fs");
		simpleDepthShader->use();
		simpleDepthShader->setUniform("PV", renderDataHandle.next.PV);

		struct DrawObj {
			GLW::Query<GLW::QueryType::SAMPLES_PASSED>* query;
			std::vector<FrustumModule::AABB> aabb;
		};

		SFE::Vector<DrawObj> occluders;
		SFE::Vector<DrawObj> occludees;

		for (auto [entity, occlusion] : ECSHandler::registry().forEach<ComponentsModule::OcclusionComponent>(entities)) {
			if (!occlusion->query->isGenerated()) {
				occlusion->query->generate();
			}
			else {
				unsigned int res = 0;

				int available = 0;
				while (!available) {
					occlusion->query->getResult(available, 0, GLW::QueryResult::QUERY_RESULT_AVAILABLE);
				}
				occlusion->query->getResult(res);
				occlusion->occluded = res == 0;
			}

			if (!occlusion->occluderAABB.empty()) {
				occluders.emplace_back(occlusion->query, occlusion->occluderAABB);
			}

			if (!occlusion->occludeeAABB.empty()) {
				occludees.emplace_back(occlusion->query, occlusion->occludeeAABB);
			}
		}
		
		occluders.sort([&renderDataHandle](const DrawObj& a, const DrawObj& b) {
			return distance(a.aabb[0].center, renderDataHandle.mNextCameraPos) < distance(b.aabb[0].center, renderDataHandle.mNextCameraPos);
		});
		
		for (auto& obj : occluders) {
			obj.query->begin();
			for (auto& aabb : obj.aabb) {
				Math::Vec3 LTN = aabb.center + Math::Vec3{-aabb.extents.x, aabb.extents.y, aabb.extents.z};
				Math::Vec3 RBF = aabb.center + Math::Vec3{aabb.extents.x, -aabb.extents.y, -aabb.extents.z};;

				Math::Vec3 RTN = { RBF.x, LTN.y, LTN.z };
				Math::Vec3 LBN = { LTN.x, RBF.y, LTN.z };
				Math::Vec3 RBN = { RBF.x, RBF.y, LTN.z };

				Math::Vec3 LTF = { LTN.x, LTN.y, RBF.z };
				Math::Vec3 RTF = { RBF.x, LTN.y, RBF.z };
				Math::Vec3 LBF = { LTN.x, RBF.y, RBF.z };

				std::vector<Vertex3D> vertices = {
					{LTN}, {RTN}, {LBN },
					{RTN}, {RBN}, {LBN},

					{LTN}, {LBN}, {LBF},
					{LTN}, {LBF}, {LTF},

					{LTN}, {LTF}, {RTN},
					{LTF}, {RTF}, {RTN},

					{RTF}, {RBF}, {RBN},
					{RTF}, {RBN}, {RTN},

					{RTF}, {LTF}, {LBF},
					{RTF}, {LBF}, {RBF},
				};

				GLW::VertexArray vao;
				GLW::Buffer vbo{GLW::BufferType::ARRAY_BUFFER};

				vao.generate();
				vao.bind();
				vbo.bind();
				vbo.allocateData(vertices);

				vao.addAttribute(0, 3, GLW::AttributeFType::FLOAT, false, &Vertex3D::position);

				GLW::drawArrays(GLW::TRIANGLES, vertices.size());

				vao.bindDefault();
			}
			obj.query->end();
		}

		
		//draw occluders
		GLW::DepthMaskStack::push(false);
		//draw occludee
		occludees.sort([&renderDataHandle](const DrawObj& a, const DrawObj& b) {
			return distance(a.aabb[0].center, renderDataHandle.mNextCameraPos) < distance(b.aabb[0].center, renderDataHandle.mNextCameraPos);
		});
		for (auto& obj : occludees) {
			obj.query->begin();
			for (auto& aabb : obj.aabb) {
				Math::Vec3 LTN = aabb.center + Math::Vec3{-aabb.extents.x, aabb.extents.y, aabb.extents.z};
				Math::Vec3 RBF = aabb.center + Math::Vec3{aabb.extents.x, -aabb.extents.y, -aabb.extents.z};;

				Math::Vec3 RTN = { RBF.x, LTN.y, LTN.z };
				Math::Vec3 LBN = { LTN.x, RBF.y, LTN.z };
				Math::Vec3 RBN = { RBF.x, RBF.y, LTN.z };

				Math::Vec3 LTF = { LTN.x, LTN.y, RBF.z };
				Math::Vec3 RTF = { RBF.x, LTN.y, RBF.z };
				Math::Vec3 LBF = { LTN.x, RBF.y, RBF.z };

				std::vector<Vertex3D> vertices = {
					{LTN}, {RTN}, {LBN },
					{RTN}, {RBN}, {LBN},

					{LTN}, {LBN}, {LBF},
					{LTN}, {LBF}, {LTF},

					{LTN}, {LTF}, {RTN},
					{LTF}, {RTF}, {RTN},

					{RTF}, {RBF}, {RBN},
					{RTF}, {RBN}, {RTN},

					{RTF}, {LTF}, {LBF},
					{RTF}, {LBF}, {RBF},
				};

				GLW::VertexArray vao;
				GLW::Buffer vbo{GLW::BufferType::ARRAY_BUFFER};

				vao.generate();
				vao.bind();
				vbo.bind();
				vbo.allocateData(vertices);

				vao.addAttribute(0, 3, GLW::AttributeFType::FLOAT, false, &Vertex3D::position);

				GLW::drawArrays(GLW::TRIANGLES, vertices.size());

				vao.bindDefault();
			}
			obj.query->end();
		}
		GLW::DepthMaskStack::pop();

		GLW::Framebuffer::bindDefaultFramebuffer();
		GLW::ViewportStack::pop();
	}
}
