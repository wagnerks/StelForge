#include "GeometryPass.h"

#include "componentsModule/ModelComponent.h"
#include "ecsModule/EntityManager.h"
#include "renderModule/Renderer.h"
#include "assetsModule/TextureHandler.h"
#include "renderModule/Utils.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "ecsModule/SystemManager.h"
#include "systemsModule/CameraSystem.h"
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
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gPosition);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mData.gPosition, 0);

	// normal color buffer
	glGenTextures(1, &mData.gNormal);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mData.gNormal, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gAlbedoSpec);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gAlbedoSpec);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Renderer::SCR_WIDTH, Renderer::SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mData.gAlbedoSpec, 0);

	// color + specular color buffer
	glGenTextures(1, &mData.gViewPosition);
	AssetsModule::TextureHandler::instance()->bindTexture(GL_TEXTURE0, GL_TEXTURE_2D, mData.gViewPosition);
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
	auto batcher = renderer->getBatcher();

	const auto& drawableEntities = renderDataHandle.mDrawableEntities;

	auto addToDraw = [batcher, &drawableEntities, this, &renderDataHandle](size_t chunkBegin, size_t chunkEnd) {
		for (size_t i = chunkBegin; i < chunkEnd; i++) {
			auto entityId = drawableEntities[i];
			auto entity = ecsModule::ECSHandler::entityManagerInstance()->getEntity(entityId);
			if (auto modelComp = entity->getComponent<ModelComponent>()) {
				auto& transform = entity->getComponent<TransformComponent>()->getTransform();
				auto& model = modelComp->getModel();
				for (auto& mesh : model.mMeshHandles) {
					if (mesh.mBounds->isOnFrustum(renderDataHandle.mCamFrustum, transform)) {

						mtx.lock();
						batcher->addToDrawList(mesh.mData.mVao, mesh.mData.mVertices.size(), mesh.mData.mIndices.size(), mesh.mMaterial, transform, false);
						mtx.unlock();
					}
				}
			}
		}
	};

	auto size = drawableEntities.size();
	size_t chunk = 0;
	size_t growSpeed = 500;


	threads.reserve(size / growSpeed);
	while (chunk < size) {
		threads.emplace_back(addToDraw, chunk, std::min(chunk + growSpeed, size));
		chunk += growSpeed;
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

	for (auto& thread : threads) {
		thread.join();
	}

	threads.clear();

	batcher->flushAll(true, ecsModule::ECSHandler::systemManagerInstance()->getSystem<Engine::SystemsModule::CameraSystem>()->getCurrentCamera()->getComponent<TransformComponent>()->getPos());

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (renderDataHandle.mWireframeMode) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}
