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

void SFE::Render::RenderPasses::ShadersPass::render(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK;

	static GLW::VertexArray VAO;
	if (!VAO) {
		static GLW::Buffer VBO;
		static GLW::Buffer EBO;

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
		VBO.generate(SFE::GLW::ARRAY_BUFFER);

		VAO.bind();
		VBO.bind();
		VBO.allocateData(vertices.size(), SFE::GLW::STATIC_DRAW, vertices.data());

		if (!indices.empty()) {
			EBO.generate(SFE::GLW::ELEMENT_ARRAY_BUFFER);
			EBO.bind();
			EBO.allocateData(indices.size(), SFE::GLW::STATIC_DRAW, indices.data());
		}

		VAO.addAttribute(0, 3, GLW::AttributeFType::FLOAT, true, &AssetsModule::Vertex::mPosition);
		VAO.addAttribute(1, 3, GLW::AttributeFType::FLOAT, true, &AssetsModule::Vertex::mNormal);
		VAO.addAttribute(2, 2, GLW::AttributeFType::FLOAT, true, &AssetsModule::Vertex::mTexCoords);

		VAO.bindDefault();
		VBO.unbind();
		EBO.unbind();
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);

	renderDataHandle.mGeometryPassData->gFramebuffer.bind();

	const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/testSh.vs", "shaders/testSh.fs");
	shader->use();

	GLW::bindTextureToSlot(1, &renderDataHandle.mGeometryPassData->outlinesBuffer);
	shader->setUniform("outline", 1);

	shader->setUniform("cameraPos", Math::Vec3{renderDataHandle.mCameraPos});

	shader->setUniform("far", cameraComp->getProjection().getFar());
	shader->setUniform("near", cameraComp->getProjection().getNear());

	GLW::drawVertices(GLW::TRIANGLES, VAO.getID(), 6, 4);
	Batcher batcher;
	batcher.addToDrawList(VAO.getID(), 4, 6, {}, Math::translate(Math::Mat4(1.f), renderDataHandle.mCameraPos * Math::Vec3(1.f, 0.f, 1.f)) * Math::scale(Math::Mat4{ 1.f }, Math::Vec3(1.f)), {}, false);
	batcher.flushAll(true);

	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}

	renderDataHandle.mGeometryPassData->gFramebuffer.bind();

	auto& cameraPos = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos();

	auto flush = [this, &renderDataHandle, &batcher, &cameraPos](size_t shaderId) {
		const auto shader = SHADER_CONTROLLER->getShader(shaderId);

		shader->use();

		shader->setUniform("P", renderDataHandle.current.projection);
		shader->setUniform("V", renderDataHandle.current.view);
		shader->setUniform("PV", renderDataHandle.current.PV);

		GLW::bindTextureToSlot(0, &renderDataHandle.mGeometryPassData->positionBuffer);
		GLW::bindTextureToSlot(1, &renderDataHandle.mGeometryPassData->normalBuffer);
		GLW::bindTextureToSlot(2, &renderDataHandle.mGeometryPassData->albedoBuffer);

		shader->setUniform("gPosition", 0);
		shader->setUniform("gNormal", 1);
		shader->setUniform("gAlbedoSpec", 2);
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

	renderDataHandle.mGeometryPassData->gFramebuffer.bindDefaultFramebuffer();
}
