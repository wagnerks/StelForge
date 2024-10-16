#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
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


SFE::Render::RenderPasses::ShadersPass::ShadersPass() {}

void SFE::Render::RenderPasses::ShadersPass::render(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK;

	static GLW::VertexArray VAO;
	if (!VAO) {
		static GLW::Buffer<GLW::ARRAY_BUFFER, Vertex3D> VBO;
		static GLW::Buffer<GLW::ELEMENT_ARRAY_BUFFER, unsigned int> EBO;

		std::vector<SFE::Vertex3D> vertices = {
			{{ 1.f, 0.f, -1.f}, {1.f,1.f}, {0.f,1.f,0.f}}, //far right
			{{ 1.f, 0.f,  1.f}, {1.f,0.f}, {0.f,1.f,0.f}}, //near right
			{{-1.f, 0.f,  1.f}, {0.f,0.f}, {0.f,1.f,0.f}}, //near left
			{{-1.f, 0.f, -1.f}, {0.f,1.f}, {0.f,1.f,0.f}}, //far left
		};

		std::vector<unsigned> indices = {
			0,2,1,
			3,2,0
		};

		VAO.generate();
		VBO.generate();

		VAO.bind();
		VBO.bind();
		VBO.allocateData(vertices);

		if (!indices.empty()) {
			EBO.generate();
			EBO.bind();
			EBO.allocateData(indices);
		}

		VAO.addAttribute(0, &SFE::Vertex3D::position, true);
		VAO.addAttribute(1, &SFE::Vertex3D::normal, true);
		VAO.addAttribute(2, &SFE::Vertex3D::texCoords, true);

		VAO.bindDefault();
		VBO.unbind();
		EBO.unbind();
	}

	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().getComponent<CameraComponent>(camera);

	renderDataHandle.mGeometryPassData->gFramebuffer.bind();

	const auto shader = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/testSh.vs", "shaders/testSh.fs");
	shader->use();

	shader->setUniform("cameraPos", Math::Vec3{renderDataHandle.mCameraPos});

	shader->setUniform("far", cameraComp->getProjection().getFar());
	shader->setUniform("near", cameraComp->getProjection().getNear());

	GLW::drawVertices(GLW::TRIANGLES, VAO.getID(), 4, 6);
	GLW::Framebuffer::bindDefaultFramebuffer();

	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}

	renderDataHandle.mGeometryPassData->gFramebuffer.bind();

	auto& cameraPos = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos();
	Batcher batcher;
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
		batcher.flushAll();
		batcher.clear();
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
					batcher.addToDrawList(mesh.vertices->mVao, mesh.vertices->vertices.size(), mesh.vertices->indices.size(), *mesh.mMaterial, transform);

				}
			}
		}*/
	}

	flush(curShaderId);

	renderDataHandle.mGeometryPassData->gFramebuffer.bindDefaultFramebuffer();
	
}
