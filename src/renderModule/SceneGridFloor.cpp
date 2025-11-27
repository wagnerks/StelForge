#include "SceneGridFloor.h"

#include "componentsModule/CameraComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/Engine.h"
#include "assetsModule/modelModule/Mesh.h"
#include "assetsModule/shaderModule/ShaderController.h"
#include "core/ECSHandler.h"
#include "ecss/Registry.h"
#include "glWrapper/BlendStack.h"
#include "glWrapper/Buffer.h"
#include "glWrapper/CapabilitiesStack.h"
#include "glWrapper/Draw.h"
#include "glWrapper/VertexArray.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/systems/RenderSystem.h"
#include "systemsModule/SystemManager.h"

using namespace SFE::Render;

SceneGridFloor::SceneGridFloor() {
	init();
}
SceneGridFloor::~SceneGridFloor() {}

void SceneGridFloor::init() {
	transform = Math::translate(transform, Math::Vec3(0.f));

	Math::Vec3 vertices[] = {
		{ 1.f, 0.f, -1.f}, //far right
		{-1.f, 0.f, -1.f},//far left
		{-1.f, 0.f,  1.f}, //near left

		{ 1.f, 0.f, -1.f}, //far right
		{-1.f, 0.f,  1.f},//near left
		{ 1.f, 0.f,  1.f}, //near right
	};

	BufferDesc bd;
	bd.cpuAccess = CpuAccess::None;
	bd.size = sizeof(vertices);
	bd.usage = BufferUsage::Vertex;
	VBO = Engine::instance()->rhi()->createBuffer(bd, vertices);

	VertexLayoutDesc layout;
	layout.bindings.push_back({ 0, sizeof(Math::Vec3), VertexInputRate::PerVertex });
	layout.attributes.push_back(VertexAttributeDesc{0, 0, VertexFormat::Float3, 0, false });
	
	GraphicsPipelineDesc pd;
	pd.shaders.vertex = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Vertex, "Floor Grid Vertex Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/floorGrid.vs") });
	pd.shaders.fragment = Engine::instance()->rhi()->createShaderModule({ ShaderStage::Fragment, "Floor Grid Fragment Shader", ShaderModule::ShaderBase::loadShaderCode("shaders/floorGrid.fs") });
	pd.layout = layout;
	pd.cullMode = CullMode::Back;
	pd.frontFace = FrontFace::CounterClockWise;
	pd.blend = { true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };

	floorPipeline = Engine::instance()->rhi()->createGraphicsPipeline(pd);
}

void SceneGridFloor::draw() {
	auto& renderData = ECSHandler::getSystem<SFE::SystemsModule::RenderSystem>()->getRenderData();
	auto camera = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>()->getCurrentCamera();
	auto cameraComp = ECSHandler::registry().pinComponent<const CameraComponent>(camera);

	auto rhi = Engine::instance()->rhi();

	rhi->bindPipeline(floorPipeline);
	VBO.bindVertex(0);

	rhi->setUniform(floorPipeline, "PVM", renderData.current.PV * transform);
	rhi->setUniform(floorPipeline, "PV", renderData.current.PV);
	rhi->setUniform(floorPipeline, "model", transform);

	rhi->setUniform(floorPipeline, "cameraPos", Math::Vec3{ renderData.mCameraPos });

	rhi->setUniform(floorPipeline, "far", cameraComp->getProjection().getFar());
	rhi->setUniform(floorPipeline, "near", cameraComp->getProjection().getNear());
	rhi->draw(6);
}
