#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"
#include "systemsModule/RenderSystem.h"
#include "systemsModule/ShaderSystem.h"


void Engine::RenderModule::RenderPasses::ShadersPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	const auto& drawableEntities = ECSHandler::systemManagerInstance()->getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}

	auto batcher = renderer->getBatcher();

	glViewport(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, renderDataHandle.mGeometryPassData.mGBuffer);
	auto& cameraPos = ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<TransformComponent>()->getPos();

	auto flush = [this, &renderDataHandle, &batcher, &cameraPos](size_t shaderId) {
		const auto shader = SHADER_CONTROLLER->getShader(shaderId);

		shader->use();

		shader->setMat4("P", renderDataHandle.mProjection);
		shader->setMat4("V", renderDataHandle.mView);
		shader->setMat4("PV", renderDataHandle.mProjection * renderDataHandle.mView);

		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gPosition);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gAlbedoSpec);

		shader->setInt("gPosition", 0);
		shader->setInt("gNormal", 1);
		shader->setInt("gAlbedoSpec", 2);

		batcher->flushAll(true, cameraPos);
	};

	size_t curShaderId = drawableEntities.front().first;
	for (auto& [shaderId, entityId] : drawableEntities) {
		if (shaderId != curShaderId) {
			flush(shaderId);

			curShaderId = shaderId;
		}

		auto entity = ECSHandler::entityManagerInstance()->getEntity(entityId);

		if (auto modelComp = entity->getComponent<ModelComponent>()) {
			auto& transform = entity->getComponent<TransformComponent>()->getTransform();
			auto& model = modelComp->getModel();
			for (auto& mesh : model.mMeshHandles) {
				if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform)) {
					batcher->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transform, false);

				}
			}
		}
	}

	flush(curShaderId);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
