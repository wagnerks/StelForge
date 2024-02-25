#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ShaderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"


SFE::Render::RenderPasses::ShadersPass::ShadersPass() {

}

void SFE::Render::RenderPasses::ShadersPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	FUNCTION_BENCHMARK;

	static VertexArray VAO;
	if (!VAO.getID()) {
		static Buffer VBO;
		static Buffer EBO;

		std::vector<AssetsModule::Vertex> vertices = {
			{{ 1.0, 0.f, -1.0}, {0.f,1.f,0.f}, {1.f,1.f}}, //far right
			{{ 1.0, 0.f,  1.0}, {0.f,1.f,0.f}, {1.f,0.f}}, //near right
			{{-1.0, 0.f,  1.0}, {0.f,1.f,0.f}, {0.f,0.f}}, //near left
			{{-1.0, 0.f, -1.0}, {0.f,1.f,0.f}, {0.f,1.f}}, //far left
		};

		std::vector<unsigned> indices = {
			0,2,1,
			3,2,0
		};

		VAO.generate();
		VBO.generate(SFE::Render::ARRAY_BUFFER);

		VAO.bind();
		VBO.bind();
		VBO.allocateData(vertices.size(), SFE::Render::STATIC_DRAW, vertices.data());

		if (!indices.empty()) {
			EBO.generate(SFE::Render::ELEMENT_ARRAY_BUFFER);
			EBO.bind();
			EBO.allocateData(indices.size(), SFE::Render::STATIC_DRAW, indices.data());
		}

		VAO.addAttribute(0, 3, FLOAT, true, &AssetsModule::Vertex::mPosition);
		VAO.addAttribute(1, 3, FLOAT, true, &AssetsModule::Vertex::mNormal);
		VAO.addAttribute(2, 2, FLOAT, true, &AssetsModule::Vertex::mTexCoords);

		VAO.bindDefault();
		VBO.unbind();
		EBO.unbind();
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);

	glViewport(0, 0, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);
	renderDataHandle.mGeometryPassData.gFramebuffer->bind();

	const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/testSh.vs", "shaders/testSh.fs");
	shader->use();

	shader->setVec3("cameraPos", Math::Vec3{renderDataHandle.mCameraPos});

	shader->setFloat("far", cameraComp->getProjection().getFar());
	shader->setFloat("near", cameraComp->getProjection().getNear());


	batcher.addToDrawList(VAO.getID(), 4, 6, {}, Math::translate(Math::Mat4(1.f), renderDataHandle.mCameraPos * Math::Vec3(1.f, 0.f, 1.f)) * Math::scale(Math::Mat4{ 1.f }, Math::Vec3(1.f)), {}, false);
	batcher.flushAll(true);

	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}
	
	glViewport(0, 0, Renderer::SCR_RENDER_W, Renderer::SCR_RENDER_H);
	renderDataHandle.mGeometryPassData.gFramebuffer->bind();

	auto& cameraPos = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos();

	auto flush = [this, &renderDataHandle, &batcher, &cameraPos](size_t shaderId) {
		const auto shader = SHADER_CONTROLLER->getShader(shaderId);

		shader->use();

		shader->setMat4("P", renderDataHandle.current.projection);
		shader->setMat4("V", renderDataHandle.current.view);
		shader->setMat4("PV", renderDataHandle.current.PV);

		AssetsModule::TextureHandler::instance()->bindTextureToSlot(0, renderDataHandle.mGeometryPassData.positionBuffer);
		AssetsModule::TextureHandler::instance()->bindTextureToSlot(1, renderDataHandle.mGeometryPassData.normalBuffer);
		AssetsModule::TextureHandler::instance()->bindTextureToSlot(2, renderDataHandle.mGeometryPassData.albedoBuffer);

		shader->setInt("gPosition", 0);
		shader->setInt("gNormal", 1);
		shader->setInt("gAlbedoSpec", 2);
		batcher.sort(cameraPos);
		batcher.flushAll(true);
	};

	size_t curShaderId = drawableEntities.front().first;
	for (auto& [shaderId, entityId] : drawableEntities) {
		if (shaderId != curShaderId) {
			flush(shaderId);

			curShaderId = shaderId;
		}

		/*if (auto modelComp = ECSHandler::registry().getComponent<ModelComponent>(entityId)) {
			auto& transform = ECSHandler::registry().getComponent<TransformComponent>(entityId)->getTransform();
			auto& model = modelComp->getModel();
			for (auto& mesh : model.meshes) {
				if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform)) {
					batcher.addToDrawList(mesh.vertices->mVao, mesh.vertices->vertices.size(), mesh.vertices->indices.size(), *mesh.mMaterial, transform, false);

				}
			}
		}*/
	}

	flush(curShaderId);

	renderDataHandle.mGeometryPassData.gFramebuffer->bindDefaultFramebuffer();
}
