#include "ShadersPass.h"
#include "componentsModule/ModelComponent.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/systems/ShaderSystem.h"
#include "systemsModule/SystemManager.h"
#include "systemsModule/SystemsPriority.h"


SFE::RenderModule::RenderPasses::ShadersPass::ShadersPass() {

}

void SFE::RenderModule::RenderPasses::ShadersPass::render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) {
	FUNCTION_BENCHMARK
	const auto& drawableEntities = ECSHandler::getSystem<SystemsModule::ShaderSystem>()->drawableEntities;
	if (drawableEntities.empty()) {
		return;
	}
	
	glViewport(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, renderDataHandle.mGeometryPassData.mGBuffer);
	auto& cameraPos = ECSHandler::registry().getComponent<TransformComponent>(ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera())->getPos();

	auto flush = [this, &renderDataHandle, &batcher, &cameraPos](size_t shaderId) {
		const auto shader = SHADER_CONTROLLER->getShader(shaderId);

		shader->use();

		shader->setMat4("P", renderDataHandle.current.projection);
		shader->setMat4("V", renderDataHandle.current.view);
		shader->setMat4("PV", renderDataHandle.current.PV);

		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gPosition);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE1, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gNormal);
		AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE2, GL_TEXTURE_2D, renderDataHandle.mGeometryPassData.gAlbedoSpec);

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

	/*	if (auto modelComp = ECSHandler::registry().getComponent<ModelComponent>(entityId)) {
			auto& transform = ECSHandler::registry().getComponent<TransformComponent>(entityId)->getTransform();
			auto& model = modelComp->getModel();
			for (auto& mesh : model.mMeshHandles) {
				if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform)) {
					batcher.addToDrawList(mesh.mData->mVao, mesh.mData->mVertices.size(), mesh.mData->mIndices.size(), *mesh.mMaterial, transform, false);

				}
			}
		}*/
	}

	flush(curShaderId);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
