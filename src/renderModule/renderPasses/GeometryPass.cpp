#include "GeometryPass.h"

#include "componentsModule/LodComponent.h"
#include "componentsModule/MeshComponent.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/Renderer.h"
#include "renderModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "shaderModule/ShaderController.h"
#include "systemsModule/RenderSystem.h"

using namespace Engine::RenderModule::RenderPasses;

void GeometryPass::init() {
	if (mInited) {
		return;
	}
	mInited = true;

	// configure g-buffer framebuffer
	// ------------------------------

	glGenFramebuffers(1, &mData.mGBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);

	// position color buffer
	glGenTextures(1, &mData.gPosition);
	TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gPosition, 0);

	// normal color buffer
	glGenTextures(1, &mData.gNormal);
	TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gNormal, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gAlbedoSpec);
	TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mData.gAlbedoSpec, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gViewPosition);
	TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gViewPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mData.gViewPosition, 0);

	// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
	constexpr unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, attachments);

	// create and attach depth buffer (renderbuffer)
	glGenRenderbuffers(1, &mData.rboDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mData.rboDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mData.rboDepth);
	// finally check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LogsModule::Logger::LOG_WARNING("Framebuffer not complete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GeometryPass::render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) {
	if (!mInited) {
		return;
	}
	if (renderDataHandle.mWireframeMode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}


	renderDataHandle.mGeometryPassData = mData;

	auto& drawableEntities = renderDataHandle.mDrawableEntities;
	for (auto entityId : drawableEntities) {
		auto transform = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<TransformComponent>();
		if (auto modelComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<MeshComponent>()) {
			size_t LODLevel = 0;
			if (auto lodComp = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId)->getComponent<LodComponent>()) {
				LODLevel = lodComp->getLodLevel();
			}
			auto& mesh = modelComp->getMesh(LODLevel);
			if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, *transform)) {
				renderer->getBatcher()->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transform->getTransform(), false);
			}

		}
	}

	glViewport(0, 0, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT);


	glBindFramebuffer(GL_FRAMEBUFFER, mData.mGBuffer);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	auto shaderGeometryPass = SHADER_CONTROLLER->loadVertexFragmentShader("shaders/g_buffer.vs", "shaders/g_buffer.fs");
	shaderGeometryPass->use();
	shaderGeometryPass->setMat4("P", renderDataHandle.mProjection);
	shaderGeometryPass->setMat4("V", renderDataHandle.mView);
	shaderGeometryPass->setMat4("PV", renderDataHandle.mProjection * renderDataHandle.mView);
	shaderGeometryPass->setInt("texture_diffuse1", 0);
	shaderGeometryPass->setInt("normalMap", 1);

	renderer->getBatcher()->flushAll(true);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (renderDataHandle.mWireframeMode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}
