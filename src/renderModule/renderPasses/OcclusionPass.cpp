#include "OcclusionPass.h"

#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcclusionComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "glWrapper/Depth.h"
#include "glWrapper/ViewportStack.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"

namespace SFE::Render::RenderPasses {
	OcclusionPass::~OcclusionPass() {}

	void OcclusionPass::init() {
		auto rhi = Engine::instance()->rhi();
		TextureDesc depthDesc;
		depthDesc.width = static_cast<uint32_t>(w);
		depthDesc.height = static_cast<uint32_t>(h);
		depthDesc.format = TextureFormat::DEPTH_COMPONENT32F;
		depthDesc.usage = TextureUsage::DepthStencilAttachment | TextureUsage::Sampled;

		depthTex = rhi->createTexture(depthDesc);

		SamplerDesc sampler;
		sampler.minFilter = FilterMode::Linear;
		sampler.magFilter = FilterMode::Linear;
		sampler.addressU = AddressMode::ClampToEdge;
		sampler.addressV = AddressMode::ClampToEdge;
		sampler.compareOp = CompareOp::Less;

		depthSampler = rhi->createSampler(sampler);

		FramebufferDesc fbDesc;
		fbDesc.width = static_cast<uint32_t>(w);
		fbDesc.height = static_cast<uint32_t>(h);
		fbDesc.depthStencilAttachment = FramebufferAttachmentDesc{ depthTex };
		occlusionFrameBuffer = rhi->createFramebuffer(fbDesc);
	}

	void OcclusionPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
		FUNCTION_BENCHMARK
			return;
		//todo update it only for objects with dirty transforms, or if camera moved (especially for shadows)
		ecss::Ranges<ecss::EntityId> entities;
		{
			const auto octreeSys = ECSHandler::getSystem<SystemsModule::OcTreeSystem>();
			std::mutex addMtx;
			auto aabbOctrees = octreeSys->getAABBOctrees(renderDataHandle.mNextCamFrustum.generateAABB());
			ThreadPool::instance()->addBatchTasks(aabbOctrees.size(), 5, [aabbOctrees, octreeSys, &addMtx, &renderDataHandle, &entities](size_t it)mutable {
				if (auto treeIt = octreeSys->getOctree(aabbOctrees[it])) {
					auto lock = treeIt->readLock();
					treeIt->forEachObjectInsideFrustum(renderDataHandle.mNextCamFrustum, [&entities, &addMtx](auto id) {
						std::unique_lock lock(addMtx);
						entities.insert(id);
					});
				}

			}).waitAll();
		}

		if (entities.empty()) {
			return;
		}
		
		GLW::ViewportStack::push({ {static_cast<int>(w), static_cast<int>(h)} });
		Engine::instance()->rhi()->bindFramebuffer(occlusionFrameBuffer);
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

		for (auto [entity, occlusion] : ECSHandler::registry().view<ComponentsModule::OcclusionComponent>(entities)) {
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
				auto occluded = ECSHandler::registry().addComponent<ComponentsModule::OccludedComponent>(entity);
				occluded->occluded = occlusion->occluded;
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

				GLW::drawArrays(GLW::TRIANGLES, static_cast<GLsizei>(vertices.size()));

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

				GLW::drawArrays(GLW::TRIANGLES, static_cast<GLsizei>(vertices.size()));

				vao.bindDefault();
			}
			obj.query->end();
		}
		GLW::DepthMaskStack::pop();

		Engine::instance()->rhi()->bindFramebuffer({});
		GLW::ViewportStack::pop();
	}
}
