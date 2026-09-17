#include "GrassPass.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Terrain.h"
#include "assetsModule/modelModule/Vertex.h"
#include "componentsModule/CharacterComponent.h"
#include "componentsModule/GarbageComponent.h"
#include "componentsModule/GrassComponent.h"
#include "componentsModule/TerrainComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/VegetationComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "debugModule/Benchmark.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/RenderSystem.h"

using namespace SFE::Render::RenderPasses;

namespace {
	// The grass map's texels per metre, and the most it may have along a side.
	constexpr float kMapTexelsPerMetre = 0.8f;
	constexpr uint32_t kMaxMapSize = 4096;
	// Parts of a reed plant past its leaves; grass.vs has the same.
	constexpr float kReedStem = 10.f;
	constexpr float kReedHead = 11.f;
	constexpr float kReedSpike = 12.f;
	// A clump of `leaves` reed leaves of `leafSegments` each, or with no leaves a cattail's stem, head and spike, as
	// strips sharing their vertices. Indices count from the plant's own first vertex.
	void appendReedPlant(std::vector<SFE::Vertex2D>& vertices, std::vector<uint32_t>& indices, uint32_t leaves, uint32_t leafSegments,
		uint32_t headSegments) {
		const uint32_t plantStart = static_cast<uint32_t>(vertices.size());
		const auto strip = [&](float part, uint32_t segments) {
			const uint32_t first = static_cast<uint32_t>(vertices.size()) - plantStart;
			for (uint32_t s = 0; s <= segments; ++s) {
				const float along = static_cast<float>(s) / static_cast<float>(segments);
				vertices.push_back({ { along, -1.f }, { part, 0.f } });
				vertices.push_back({ { along, 1.f }, { part, 0.f } });
			}
			for (uint32_t s = 0; s < segments; ++s) {
				const uint32_t a = first + s * 2;
				indices.insert(indices.end(), { a, a + 2, a + 1, a + 1, a + 2, a + 3 });
			}
		};
		for (uint32_t leaf = 0; leaf < leaves; ++leaf) {
			strip(static_cast<float>(leaf), leafSegments);
		}
		if (leaves == 0) {
			strip(kReedStem, 1);
			strip(kReedHead, headSegments);
			strip(kReedSpike, 1);
		}
	}
	// A lily pad: a disc of `rings` rings of `segments` vertices round a centre, the outer ring just outside the unit
	// circle so the polygon covers all of it for lily.fs to cut the rim from. Positions are the pad's own x/y in the
	// unit disc, texCoords (0, 0). Indices count from the pad's own first vertex.
	void appendLilyPad(std::vector<SFE::Vertex2D>& vertices, std::vector<uint32_t>& indices, uint32_t rings, uint32_t segments) {
		const float outer = 1.f / std::cos(3.14159265f / static_cast<float>(segments));
		vertices.push_back({ { 0.f, 0.f }, { 0.f, 0.f } });
		for (uint32_t ring = 1; ring <= rings; ++ring) {
			const float radius = static_cast<float>(ring) / static_cast<float>(rings) * (ring == rings ? outer : 1.f);
			for (uint32_t s = 0; s < segments; ++s) {
				const float angle = static_cast<float>(s) / static_cast<float>(segments) * 6.2831853f;
				vertices.push_back({ { std::cos(angle) * radius, std::sin(angle) * radius }, { 0.f, 0.f } });
			}
		}
		const auto at = [&](uint32_t ring, uint32_t s) { return ring == 0 ? 0u : 1u + (ring - 1) * segments + s % segments; };
		for (uint32_t ring = 0; ring < rings; ++ring) {
			for (uint32_t s = 0; s < segments; ++s) {
				if (ring == 0) {
					indices.insert(indices.end(), { 0u, at(1, s), at(1, s + 1) });
				}
				else {
					const uint32_t a = at(ring, s), b = at(ring, s + 1), c = at(ring + 1, s), d = at(ring + 1, s + 1);
					indices.insert(indices.end(), { a, c, b, b, c, d });
				}
			}
		}
	}

	// A white water lily: two rings of eight petals, each a quad whose position is (along the petal 0..1, across -1..1)
	// and texCoords (ring 1 or 2, petal index), and a quad for the yellow middle, texCoords (3, 0). Indices count from
	// the flower's own first vertex.
	void appendLilyFlower(std::vector<SFE::Vertex2D>& vertices, std::vector<uint32_t>& indices) {
		const uint32_t first = static_cast<uint32_t>(vertices.size());
		const auto quad = [&](float part, float index, float u0, float v0, float u1, float v1) {
			const uint32_t a = static_cast<uint32_t>(vertices.size()) - first;
			vertices.push_back({ { u0, v0 }, { part, index } });
			vertices.push_back({ { u0, v1 }, { part, index } });
			vertices.push_back({ { u1, v0 }, { part, index } });
			vertices.push_back({ { u1, v1 }, { part, index } });
			indices.insert(indices.end(), { a, a + 2, a + 1, a + 1, a + 2, a + 3 });
		};
		for (const float ring : { 1.f, 2.f }) {
			for (int petal = 0; petal < 8; ++petal) {
				quad(ring, static_cast<float>(petal), 0.f, -1.f, 1.f, 1.f);
			}
		}
		quad(3.f, 0.f, -1.f, -1.f, 1.f, 1.f);
	}

	// Metres along a side of a square of mBandMin/mBandMax.
	constexpr float kBandCell = 32.f;
	// Plants along one side of a chunk: one instanced draw is kChunkSide squared of them.
	constexpr int kChunkSide = 64;

	// The grass clumps: the kit's models, metres a chunk of them is placed at a time, metres between the cells of a
	// chunk's grid (one clump at most in each), the reach within which they are drawn, where the full meshes give way
	// to the coarser ones, and from where they shrink away towards the reach.
	constexpr const char* kClumpModels[] = {
		"models/forestkit/Forest_Grass_1.fbx", "models/forestkit/Forest_Grass_2.fbx", "models/forestkit/Forest_Grass_3.fbx",
		"models/forestkit/Forest_Grass_4.fbx", "models/forestkit/Forest_Grass_5.fbx", "models/forestkit/Forest_Grass_6.fbx",
	};
	constexpr const char* kClumpAtlas = "models/forestkit/Forest_Pack_Atlas_1.tga";
	constexpr float kClumpChunk = 16.f;
	constexpr float kClumpSpacing = 0.55f;
	constexpr float kClumpDetail = 16.f;
	// Out to here every clump is drawn; past it fewer and fewer, each the larger for it, so the meadow covers the
	// ground as before for far fewer of them: a clump a hundred metres off is a few pixels.
	constexpr float kClumpFull = 30.f;
	// The models are in centimetres, and a clump at scale 1 would be most of a metre tall.
	constexpr float kClumpScale = 0.0085f;
	// Metres a clump's shadow can reach past it: half a metre of grass under a sun a dozen degrees up.
	constexpr float kClumpShadowReach = 3.f;
	// The shadow cascades clumps are drawn into: the first two, which between them cover the clumps' whole reach. With
	// the first alone their shadows ended on its circle, a ring round the camera wherever the grass went on past it.
	constexpr uint32_t kClumpShadowCascades = 2;

	uint32_t cellHash(int x, int z, uint32_t salt) {
		uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(z) * 19349663u ^ salt * 83492791u;
		h ^= h >> 13;
		h *= 0x5bd1e995u;
		h ^= h >> 15;
		return h;
	}

	float cellRandom(int x, int z, uint32_t salt) {
		return static_cast<float>(cellHash(x, z, salt) >> 8) / 16777216.f;
	}

	float smoothstep(float edge0, float edge1, float x) {
		const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.f, 1.f);
		return t * t * (3.f - 2.f * t);
	}
}

void GrassPass::init(SystemsModule::RenderData& renderDataHandle) {
	auto& registry = ECSHandler::registry();
	ComponentsModule::TerrainComponent terrain;
	for (const auto& [id, grass, terrainComp, transform] : registry.view<const ComponentsModule::GrassComponent, const ComponentsModule::TerrainComponent,
		const TransformComponent>()) {
		if (!grass || !terrainComp || !transform) {
			continue;
		}
		terrain = *terrainComp;
		const auto pos = transform->getPos();
		mParams.terrain[0] = pos.x;
		mParams.terrain[1] = pos.y;
		mParams.terrain[2] = pos.z;
		mParams.terrain[3] = terrain.size;
		if (const auto placements = registry.pinComponent<const ComponentsModule::VegetationPlacementsComponent>(id)) {
			mForest = placements->forestMap;
		}
		mEnabled = true;
		break;
	}
	if (!mEnabled) {
		return;
	}
	mTerrain = terrain;

	auto* rhi = Engine::instance()->rhi();
	const auto& heights = AssetsModule::terrainHeights(terrain);
	const auto side = static_cast<uint32_t>(std::lround(std::sqrt(static_cast<double>(heights.size()))));
	mParams.grid[0] = static_cast<float>(side);
	mParams.grid[1] = terrain.size / static_cast<float>(side - 1);
	const auto [lo, hi] = std::minmax_element(heights.begin(), heights.end());
	mMinHeight = *lo;
	mMaxHeight = *hi;
	mParams.grid[3] = mMinHeight;
	// Where the litter lies the grass is kept short and thin, so the litter shows.
	for (const auto& [id, garbage] : ECSHandler::registry().view<const ComponentsModule::GarbageComponent>()) {
		if (garbage && !garbage->items.empty()) {
			mParams.litter[0] = garbage->shoreBelow;
			mParams.litter[1] = garbage->shoreAbove;
			mParams.litter[2] = 1.f;
			break;
		}
	}
	mBandSide = (std::max)(1, static_cast<int>(std::ceil(terrain.size / kBandCell)));
	mBandMin.assign(static_cast<size_t>(mBandSide) * mBandSide, (std::numeric_limits<float>::max)());
	mBandMax.assign(mBandMin.size(), std::numeric_limits<float>::lowest());
	for (uint32_t j = 0; j < side; ++j) {
		for (uint32_t i = 0; i < side; ++i) {
			const int bx = (std::min)(mBandSide - 1, static_cast<int>(static_cast<float>(i) * mParams.grid[1] / kBandCell));
			const int bz = (std::min)(mBandSide - 1, static_cast<int>(static_cast<float>(j) * mParams.grid[1] / kBandCell));
			const size_t band = static_cast<size_t>(bz) * mBandSide + bx;
			const float h = heights[static_cast<size_t>(j) * side + i];
			mBandMin[band] = (std::min)(mBandMin[band], h);
			mBandMax[band] = (std::max)(mBandMax[band], h);
		}
	}

	TextureDesc heightDesc;
	heightDesc.width = side;
	heightDesc.height = side;
	heightDesc.format = TextureFormat::R32F;
	heightDesc.usage = TextureUsage::Sampled;
	mHeights = rhi->createTexture(heightDesc, heights.data());
	mTerrainNoise = AssetsModule::terrainNoiseTexture(terrain);
	if (!mForest) {
		TextureDesc forestDesc;
		forestDesc.width = 1;
		forestDesc.height = 1;
		forestDesc.format = TextureFormat::R8;
		const uint8_t open = 0;
		mForest = rhi->createTexture(forestDesc, &open);
	}

	mMapSize = 256;
	while (mMapSize < kMaxMapSize && static_cast<float>(mMapSize) < terrain.size * kMapTexelsPerMetre) {
		mMapSize *= 2;
	}
	TextureDesc mapDesc;
	mapDesc.width = mMapSize;
	mapDesc.height = mMapSize;
	mapDesc.format = TextureFormat::RGBA8;
	mapDesc.usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
	mMap = rhi->createTexture(mapDesc);

	SamplerDesc nearest;
	nearest.minFilter = FilterMode::Nearest;
	nearest.magFilter = FilterMode::Nearest;
	nearest.addressU = AddressMode::ClampToEdge;
	nearest.addressV = AddressMode::ClampToEdge;
	mNearest = rhi->createSampler(nearest);
	SamplerDesc linear = nearest;
	linear.minFilter = FilterMode::Linear;
	linear.magFilter = FilterMode::Linear;
	mLinear = rhi->createSampler(linear);

	// Reed plants up close, in the middle distance, and far off.
	// Every vertex of every plant in reach runs the whole vertex shader, grown or not, so the plants keep
	// few of them: leaves, leaf segments and head segments per level.
	std::vector<Vertex2D> reeds;
	std::vector<uint32_t> reedIndices;
	// Cattails apart from the leaves, on a sparser grid of their own: only some plants carry one, and drawn with every
	// clump their vertices cost as much as the leaves'.
	const std::array<std::array<uint32_t, 3>, 5> reedDetail{ { { 5, 3, 0 }, { 3, 2, 0 }, { 2, 1, 0 }, { 0, 0, 2 }, { 0, 0, 1 } } };
	// Lily pads with the curl of their rim up close, simpler further off, then the flower.
	const std::array<std::array<uint32_t, 2>, 3> padDetail{ { { 3, 18 }, { 1, 10 }, { 1, 6 } } };
	size_t meshIndex = 0;
	const auto addMesh = [&](const auto& append) {
		auto& mesh = mPlantMeshes[meshIndex++];
		mesh.firstIndex = static_cast<uint32_t>(reedIndices.size());
		mesh.firstVertex = static_cast<int32_t>(reeds.size());
		append();
		mesh.indexCount = static_cast<uint32_t>(reedIndices.size()) - mesh.firstIndex;
	};
	for (const auto& detail : reedDetail) {
		addMesh([&] { appendReedPlant(reeds, reedIndices, detail[0], detail[1], detail[2]); });
	}
	for (const auto& detail : padDetail) {
		addMesh([&] { appendLilyPad(reeds, reedIndices, detail[0], detail[1]); });
	}
	addMesh([&] { appendLilyFlower(reeds, reedIndices); });
	BufferDesc reedDesc;
	reedDesc.size = reeds.size() * sizeof(Vertex2D);
	reedDesc.usage = BufferUsage::Vertex;
	reedDesc.cpuAccess = CpuAccess::None;
	mPlants = rhi->createBuffer(reedDesc, reeds.data());
	reedDesc.size = reedIndices.size() * sizeof(uint32_t);
	reedDesc.usage = BufferUsage::Index;
	mPlantIndices = rhi->createBuffer(reedDesc, reedIndices.data());

	GraphicsPipelineDesc base;
	base.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(Vertex2D)), VertexInputRate::PerVertex);
	base.layout.attributes.emplace_back(0, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, position)), false);
	base.layout.attributes.emplace_back(1, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex2D, texCoords)), false);
	base.cullMode = CullMode::None;

	// The grass map: baked once, the first frame.
	{
		RenderPassDesc passDesc;
		passDesc.colorAttachmentCount = 1;
		passDesc.colorAttachments[0] = { AttachmentLoadOp::Clear, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 0.f }, TextureFormat::RGBA8 };
		mMapPass = rhi->createRenderPass(passDesc);
		FramebufferDesc fb;
		fb.width = mMapSize;
		fb.height = mMapSize;
		fb.renderPass = mMapPass;
		fb.colorAttachments = { { mMap } };
		mMapFbo = rhi->createFramebuffer(fb);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.emplace_back(16, DescriptorType::UniformBufferDynamic, ShaderStage::Fragment);
		layoutDesc.bindings.emplace_back(17, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		layoutDesc.bindings.emplace_back(19, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		layoutDesc.bindings.emplace_back(20, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		mMapLayout = rhi->createDescriptorSetLayout(layoutDesc);
		mMapSet = rhi->allocateDescriptorSet(mMapLayout);

		auto desc = base;
		desc.topology = PrimitiveTopology::Triangles;
		desc.depthStencilState.depthTest = false;
		desc.depthStencilState.depthWrite = false;
		desc.renderPass = mMapPass;
		desc.descriptorSetLayouts = { mMapLayout };
		desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/fullscreen.vs");
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/grass_map.fs");
		mMapPipeline = rhi->createGraphicsPipeline(desc);
	}

	// The reeds, cattails and pads, into the G-buffer GeometryPass has just filled.
	{
		RenderPassDesc passDesc;
		passDesc.colorAttachmentCount = 4;
		passDesc.colorAttachments[0] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA8 };
		passDesc.colorAttachments[1] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA16F };
		passDesc.colorAttachments[2] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 1.f }, TextureFormat::RGBA16F };
		passDesc.colorAttachments[3] = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, { 0.f, 0.f, 0.f, 0.f }, TextureFormat::RGBA16F };
		passDesc.hasDepthStencil = true;
		passDesc.depthAttachment = { AttachmentLoadOp::Load, AttachmentStoreOp::Store, 1.f, 0, false, TextureFormat::DEPTH_COMPONENT24 };
		mGBufferPass = rhi->createRenderPass(passDesc);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex);
		layoutDesc.bindings.emplace_back(16, DescriptorType::UniformBufferDynamic, ShaderStage::Vertex | ShaderStage::Fragment);
		layoutDesc.bindings.emplace_back(17, DescriptorType::CombinedImageSampler, ShaderStage::Vertex);
		layoutDesc.bindings.emplace_back(18, DescriptorType::CombinedImageSampler, ShaderStage::Vertex);
		layoutDesc.bindings.emplace_back(19, DescriptorType::CombinedImageSampler, ShaderStage::Vertex);
		mLayout = rhi->createDescriptorSetLayout(layoutDesc);
		mSets.resize((std::max)(1u, rhi->framesInFlight()));
		for (auto& set : mSets) {
			set = rhi->allocateDescriptorSet(mLayout);
		}

		auto desc = base;
		desc.topology = PrimitiveTopology::Triangles;
		desc.depthStencilState.depthTest = true;
		desc.depthStencilState.depthWrite = true;
		desc.depthStencilState.depthCompareOp = CompareOp::Less;
		desc.renderPass = mGBufferPass;
		desc.descriptorSetLayouts = { mLayout };
		desc.pushConstantSize = sizeof(DrawParams);
		desc.pushConstantStages = ShaderStage::Vertex;
		desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/grass.vs");
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/grass.fs");
		mPlantPipeline = rhi->createGraphicsPipeline(desc);
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/lily.fs");
		mLilyPipeline = rhi->createGraphicsPipeline(desc);
	}

	loadClumps();
	if (mClumpMeshes.empty()) {
		return;
	}
	// The clumps, into the G-buffer and into a shadow cascade.
	{
		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.emplace_back(5, DescriptorType::UniformBuffer, ShaderStage::Vertex);
		layoutDesc.bindings.emplace_back(16, DescriptorType::UniformBufferDynamic, ShaderStage::Vertex | ShaderStage::Fragment);
		layoutDesc.bindings.emplace_back(21, DescriptorType::StorageBuffer, ShaderStage::Vertex);
		layoutDesc.bindings.emplace_back(22, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		mClumpLayout = rhi->createDescriptorSetLayout(layoutDesc);
		const uint32_t slots = (std::max)(1u, rhi->framesInFlight());
		mClumpSets.resize(slots);
		mClumpBuffers.resize(slots);
		mClumpCapacity.assign(slots, 0);
		for (auto& set : mClumpSets) {
			set = rhi->allocateDescriptorSet(mClumpLayout);
		}

		GraphicsPipelineDesc desc;
		desc.layout.bindings.emplace_back(0, static_cast<uint32_t>(sizeof(ClumpVertex)), VertexInputRate::PerVertex);
		desc.layout.attributes.emplace_back(0, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(ClumpVertex, position)), false);
		desc.layout.attributes.emplace_back(1, 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(ClumpVertex, normal)), false);
		desc.layout.attributes.emplace_back(2, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(ClumpVertex, uv)), false);
		desc.topology = PrimitiveTopology::Triangles;
		desc.cullMode = CullMode::None;
		desc.depthStencilState.depthTest = true;
		desc.depthStencilState.depthWrite = true;
		desc.depthStencilState.depthCompareOp = CompareOp::Less;
		desc.descriptorSetLayouts = { mClumpLayout };
		desc.pushConstantSize = sizeof(ClumpDrawParams);
		desc.pushConstantStages = ShaderStage::Vertex | ShaderStage::Fragment;
		desc.shaders.vertex = rhi->loadShader(ShaderStage::Vertex, "shaders/grass_clump.vs");
		desc.renderPass = mGBufferPass;
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/grass_clump.fs");
		mClumpPipeline = rhi->createGraphicsPipeline(desc);

		RenderPassDesc shadowDesc;
		shadowDesc.hasDepthStencil = true;
		shadowDesc.depthAttachment.loadOp = AttachmentLoadOp::Load;
		shadowDesc.depthAttachment.storeOp = AttachmentStoreOp::Store;
		shadowDesc.depthAttachment.format = TextureFormat::DEPTH_COMPONENT32;
		mShadowPass = rhi->createRenderPass(shadowDesc);
		desc.renderPass = mShadowPass;
		desc.shaders.fragment = rhi->loadShader(ShaderStage::Fragment, "shaders/grass_clump_shadow.fs");
		mClumpShadowPipeline = rhi->createGraphicsPipeline(desc);
	}
	mRuns.resize(mClumpMeshes.size() * kClumpLods);
	mRunFirst.resize(mRuns.size());
	mRunCount.resize(mRuns.size());
}

void GrassPass::loadClumps() {
	std::vector<ClumpVertex> vertices;
	std::vector<uint32_t> indices;
	for (const auto* path : kClumpModels) {
		auto* model = AssetsModule::ModelLoader::instance()->load(path);
		if (!model || model->getLODs()->empty() || model->getLODs()->front().meshes.empty()) {
			continue;
		}
		// Each kit clump is one mesh.
		const auto& mesh = model->getLODs()->front().meshes.front()->mesh;
		ClumpMesh clump;
		clump.firstVertex = static_cast<int32_t>(vertices.size());
		for (const auto& v : mesh.vertices) {
			ClumpVertex out;
			std::copy_n(&v.position.x, 3, out.position);
			std::copy_n(&v.normal.x, 3, out.normal);
			std::copy_n(&v.texCoords.x, 2, out.uv);
			vertices.push_back(out);
		}
		for (size_t level = 0; level < kClumpLods; ++level) {
			const auto& levelIndices = level == 0 || mesh.lodIndices.empty() ? mesh.indices : mesh.lodIndices.front();
			clump.firstIndex[level] = static_cast<uint32_t>(indices.size());
			clump.indexCount[level] = static_cast<uint32_t>(levelIndices.size());
			indices.insert(indices.end(), levelIndices.begin(), levelIndices.end());
		}
		mClumpMeshes.push_back(clump);
	}
	auto* texture = AssetsModule::TextureHandler::loadTexture(kClumpAtlas);
	if (mClumpMeshes.empty() || !texture || !texture->isValid()) {
		mClumpMeshes.clear();
		return;
	}
	mAtlas = texture->texture;
	auto* rhi = Engine::instance()->rhi();
	BufferDesc desc;
	desc.size = vertices.size() * sizeof(ClumpVertex);
	desc.usage = BufferUsage::Vertex;
	desc.cpuAccess = CpuAccess::None;
	mClumpVertices = rhi->createBuffer(desc, vertices.data());
	desc.size = indices.size() * sizeof(uint32_t);
	desc.usage = BufferUsage::Index;
	mClumpIndices = rhi->createBuffer(desc, indices.data());
	SamplerDesc sampler;
	sampler.addressU = AddressMode::ClampToEdge;
	sampler.addressV = AddressMode::ClampToEdge;
	mAtlasSampler = rhi->createSampler(sampler);
}

const std::vector<GrassPass::ClumpInstance>& GrassPass::chunkClumps(const int cx, const int cz) {
	const int64_t key = (static_cast<int64_t>(cx) << 32) ^ static_cast<uint32_t>(cz);
	auto [it, added] = mChunks.try_emplace(key);
	if (!added) {
		return it->second;
	}
	auto& clumps = it->second;
	const auto& heights = AssetsModule::terrainHeights(mTerrain);
	const float half = mParams.terrain[3] * 0.5f;
	const int cells = (std::max)(1, static_cast<int>(kClumpChunk / mClumpSpacing));
	const float spacing = kClumpChunk / static_cast<float>(cells);
	for (int j = 0; j < cells; ++j) {
		for (int i = 0; i < cells; ++i) {
			const int gx = cx * cells + i;
			const int gz = cz * cells + j;
			const float x = (static_cast<float>(gx) + cellRandom(gx, gz, 1)) * spacing;
			const float z = (static_cast<float>(gz) + cellRandom(gx, gz, 2)) * spacing;
			const float localX = x - mParams.terrain[0];
			const float localZ = z - mParams.terrain[2];
			if (std::fabs(localX) >= half || std::fabs(localZ) >= half) {
				continue;
			}
			// The grass map as the terrain's rules left it; the readback's rows run top-down, so v counts up from the last.
			const float u = (localX + half) / mParams.terrain[3];
			const float v = (localZ + half) / mParams.terrain[3];
			const int tx = std::clamp(static_cast<int>(u * static_cast<float>(mMapTexelsSide)), 0, mMapTexelsSide - 1);
			const int ty = std::clamp(static_cast<int>((1.f - v) * static_cast<float>(mMapTexelsSide)), 0, mMapTexelsSide - 1);
			const size_t texel = (static_cast<size_t>(ty) * mMapTexelsSide + tx) * 4;
			const float cover = static_cast<float>(mMapTexels[texel]) / 255.f;
			const float broad = static_cast<float>(mMapTexels[texel + 1]) / 255.f;
			const float ground = AssetsModule::terrainSurfaceHeight(mTerrain, heights, localX, localZ);
			// Where the ground is mostly grass all of it grows; towards the edges of its patch it thins out.
			if (cellRandom(gx, gz, 3) > smoothstep(0.f, 0.55f, cover * smoothstep(0.02f, 0.2f, ground))) {
				continue;
			}
			ClumpInstance clump;
			// Taller where the meadow runs lush.
			const float lush = smoothstep(0.35f, 0.65f, 1.f - broad);
			clump.posScale[0] = x;
			clump.posScale[1] = mParams.terrain[1] + ground - 0.03f;
			clump.posScale[2] = z;
			clump.posScale[3] = kClumpScale * (0.6f + 0.5f * cellRandom(gx, gz, 4)) * (1.f + 0.35f * lush) * (0.7f + 0.3f * cover);
			clump.yawShade[0] = cellRandom(gx, gz, 5) * 6.2831853f;
			clump.yawShade[1] = 0.8f + 0.35f * cellRandom(gx, gz, 6);
			clump.yawShade[2] = cellRandom(gx, gz, 7) * 6.2831853f;
			clump.yawShade[3] = static_cast<float>(cellHash(gx, gz, 8) % mClumpMeshes.size());
			clumps.push_back(clump);
		}
	}
	return clumps;
}

void GrassPass::prepare(SystemsModule::RenderData& renderDataHandle) {
	FUNCTION_BENCHMARK;
	mClumpsReady = false;
	mBlock = {};
	if (!mEnabled) {
		return;
	}
	auto* rhi = Engine::instance()->rhi();
	const auto camera = renderDataHandle.mCameraPos;
	mParams.camera[0] = camera.x;
	mParams.camera[1] = camera.y;
	mParams.camera[2] = camera.z;
	mParams.trample[3] = 0.f;
	for (const auto& [id, character, matrix] : ECSHandler::registry().view<const ComponentsModule::CharacterComponent, const ComponentsModule::TransformMatComp>()) {
		if (character && matrix) {
			const auto feet = matrix->mTransform[3];
			mParams.trample[0] = feet.x;
			mParams.trample[1] = feet.y;
			mParams.trample[2] = feet.z;
			mParams.trample[3] = 0.7f;
			break;
		}
	}
	mBlock = rhi->allocTransient(mParams);
	if (mClumpMeshes.empty() || !mClumpPipeline) {
		return;
	}

	// The map is baked in the first frame's list; a few frames on it is there to read.
	if (mMapTexels.empty()) {
		if (!mMapBaked || ++mFramesSinceBake <= rhi->framesInFlight() + 1) {
			return;
		}
		int width = 0;
		int height = 0;
		if (!rhi->readbackTexture(mMap, mMapTexels, width, height) || width != height || width == 0) {
			mMapTexels.clear();
			return;
		}
		mMapTexelsSide = width;
	}

	const auto& graphics = renderDataHandle.graphics;
	const float spacing = kClumpSpacing / std::sqrt(std::clamp(graphics.grassDensity, 0.1f, 4.f));
	if (std::fabs(spacing - mClumpSpacing) > 1e-4f) {
		mClumpSpacing = spacing;
		mChunks.clear();
	}
	mClumpReach = std::clamp(graphics.grassDistance, 10.f, 400.f);
	mClumpFadeStart = mClumpReach * 0.73f;

	// The chunks in reach, placed when new; the ones well out of it forgotten.
	const int x0 = static_cast<int>(std::floor((camera.x - mClumpReach) / kClumpChunk));
	const int x1 = static_cast<int>(std::floor((camera.x + mClumpReach) / kClumpChunk));
	const int z0 = static_cast<int>(std::floor((camera.z - mClumpReach) / kClumpChunk));
	const int z1 = static_cast<int>(std::floor((camera.z + mClumpReach) / kClumpChunk));
	for (auto it = mChunks.begin(); it != mChunks.end();) {
		const int cx = static_cast<int>(it->first >> 32);
		const int cz = static_cast<int>(static_cast<int32_t>(it->first & 0xffffffff));
		const bool far = cx < x0 - 2 || cx > x1 + 2 || cz < z0 - 2 || cz > z1 + 2;
		it = far ? mChunks.erase(it) : std::next(it);
	}
	for (auto& run : mRuns) {
		run.clear();
	}
	const auto& frustum = renderDataHandle.mCamFrustum;
	const float reach2 = mClumpReach * mClumpReach;
	for (int cz = z0; cz <= z1; ++cz) {
		for (int cx = x0; cx <= x1; ++cx) {
			const float ox = static_cast<float>(cx) * kClumpChunk;
			const float oz = static_cast<float>(cz) * kClumpChunk;
			const float nearX = std::clamp(camera.x, ox, ox + kClumpChunk) - camera.x;
			const float nearZ = std::clamp(camera.z, oz, oz + kClumpChunk) - camera.z;
			if (nearX * nearX + nearZ * nearZ > reach2) {
				continue;
			}
			const auto& clumps = chunkClumps(cx, cz);
			if (clumps.empty()) {
				continue;
			}
			float low = clumps.front().posScale[1];
			float high = low;
			for (const auto& clump : clumps) {
				low = (std::min)(low, clump.posScale[1]);
				high = (std::max)(high, clump.posScale[1]);
			}
			// Tall enough for the tallest clump, and wide enough for its shadow: the drawn clumps are also the shadow
			// casters, and a low sun lays a clump's shadow a few metres out, into view from a chunk just out of it.
			const Math::Vec3 centre{ ox + kClumpChunk * 0.5f, (low + high) * 0.5f + 0.5f, oz + kClumpChunk * 0.5f };
			const Math::Vec3 extents{ kClumpChunk * 0.5f + kClumpShadowReach, (high - low) * 0.5f + 1.f, kClumpChunk * 0.5f + kClumpShadowReach };
			if (!FrustumModule::AABB::isOnFrustum(frustum, centre, extents)) {
				continue;
			}
			for (const auto& clump : clumps) {
				const float dx = clump.posScale[0] - camera.x;
				const float dz = clump.posScale[2] - camera.z;
				const float d2 = dx * dx + dz * dz;
				if (d2 > reach2) {
					continue;
				}
				const size_t level = d2 < kClumpDetail * kClumpDetail ? 0 : 1;
				auto& run = mRuns[static_cast<size_t>(clump.yawShade[3]) * kClumpLods + level];
				if (d2 <= kClumpFull * kClumpFull) {
					run.push_back(clump);
					continue;
				}
				// Kept by the share of the ground it stands for, from its own random yaw, and grown to make up for
				// the ones left out: the same clumps drop out whichever way the camera turns.
				const float keep = kClumpFull * kClumpFull / d2;
				if (clump.yawShade[2] / 6.2831853f > keep) {
					continue;
				}
				run.push_back(clump);
				run.back().posScale[3] *= (std::min)(1.f / std::sqrt(keep), 2.f);
			}
		}
	}
	mVisible.clear();
	for (size_t r = 0; r < mRuns.size(); ++r) {
		mRunFirst[r] = static_cast<uint32_t>(mVisible.size());
		mRunCount[r] = static_cast<uint32_t>(mRuns[r].size());
		mVisible.insert(mVisible.end(), mRuns[r].begin(), mRuns[r].end());
	}
	if (mVisible.empty()) {
		return;
	}
	const size_t slot = rhi->frameInFlight() % mClumpBuffers.size();
	if (mClumpCapacity[slot] < mVisible.size()) {
		if (mClumpBuffers[slot]) {
			rhi->destroyBuffer(mClumpBuffers[slot]);
		}
		mClumpCapacity[slot] = mVisible.size() * 3 / 2 + 1024;
		BufferDesc desc;
		desc.size = mClumpCapacity[slot] * sizeof(ClumpInstance);
		desc.usage = BufferUsage::Storage;
		desc.cpuAccess = CpuAccess::Write;
		desc.debugName = "Grass clumps";
		mClumpBuffers[slot] = rhi->createBuffer(desc);
	}
	rhi->updateBuffer(mClumpBuffers[slot], 0, mVisible.size() * sizeof(ClumpInstance), mVisible.data());
	rhi->updateDescriptorSets({
		{ .set = mClumpSets[slot], .binding = 5, .buffers{ { renderDataHandle.cameraMatricesUBO } } },
		{ .set = mClumpSets[slot], .binding = 16, .buffers{ { mBlock.buffer, 0, sizeof(GrassParams) } } },
		{ .set = mClumpSets[slot], .binding = 21, .buffers{ { mClumpBuffers[slot] } } },
		{ .set = mClumpSets[slot], .binding = 22, .images{ { mAtlas, mAtlasSampler } } },
	});
	mClumpsReady = true;
}

void GrassPass::drawClumps(const Render::CommandListHandle cmd, const PipelineHandle pipeline, const ClumpDrawParams& params, const bool shadow) {
	auto* rhi = Engine::instance()->rhi();
	const auto set = mClumpSets[rhi->frameInFlight() % mClumpSets.size()];
	rhi->cmdBindPipeline(cmd, pipeline);
	// After the pipeline: OpenGL keeps the vertex and index buffers on the pipeline's VAO.
	rhi->cmdBindVertexBuffer(cmd, 0, mClumpVertices, 0);
	rhi->cmdBindIndexBuffer(cmd, mClumpIndices, IndexType::Uint32, 0);
	rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1, &mBlock.offset, 1);
	rhi->cmdPushConstants(cmd, pipeline, params);
	for (size_t m = 0; m < mClumpMeshes.size(); ++m) {
		const auto& mesh = mClumpMeshes[m];
		for (size_t level = 0; level < kClumpLods; ++level) {
			const size_t run = m * kClumpLods + level;
			if (mRunCount[run] == 0) {
				continue;
			}
			// A shadow a level coarser: its cut-out outline is all that shows.
			const size_t drawn = shadow ? kClumpLods - 1 : level;
			rhi->cmdDrawIndexed(cmd, mesh.indexCount[drawn], mRunCount[run], mesh.firstIndex[drawn], mesh.firstVertex, mRunFirst[run]);
		}
	}
}

void GrassPass::drawShadow(const Render::CommandListHandle cmd, const Math::Mat4& viewProj, const uint32_t layer) {
	if (!mClumpsReady || layer >= kClumpShadowCascades) {
		return;
	}
	ClumpDrawParams params;
	for (int c = 0; c < 4; ++c) {
		for (int row = 0; row < 4; ++row) {
			params.viewProj[c * 4 + row] = viewProj[c][row];
		}
	}
	params.params[0] = 1.f;
	params.params[1] = mClumpFadeStart;
	params.params[2] = mClumpReach;
	drawClumps(cmd, mClumpShadowPipeline, params, true);
}


void GrassPass::bakeMap(Render::CommandListHandle cmd, const TransientAllocation& block) {
	auto* rhi = Engine::instance()->rhi();
	rhi->updateDescriptorSets({
		{ .set = mMapSet, .binding = 16, .buffers{ { block.buffer, 0, sizeof(GrassParams) } } },
		{ .set = mMapSet, .binding = 17, .images{ { mHeights, mNearest } } },
		{ .set = mMapSet, .binding = 19, .images{ { mForest, mLinear } } },
		{ .set = mMapSet, .binding = 20, .images{ { mTerrainNoise, mLinear } } },
	});
	RenderPassBeginInfo begin;
	begin.renderPass = mMapPass;
	begin.framebuffer = mMapFbo;
	begin.width = mMapSize;
	begin.height = mMapSize;
	rhi->cmdBeginRenderPass(cmd, begin);
	rhi->cmdBindPipeline(cmd, mMapPipeline);
	rhi->cmdBindDescriptorSets(cmd, mMapPipeline, 0, &mMapSet, 1, &block.offset, 1);
	Utils::renderQuad(cmd);
	rhi->cmdEndRenderPass(cmd);
}

void GrassPass::render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) {
	FUNCTION_BENCHMARK;
	if (!mEnabled || !mLilyPipeline || !mPlantPipeline || !mMapPipeline) {
		return;
	}
	const auto gBuffer = renderDataHandle.frame.framebuffer(FrameId::GBufferFbo);
	if (!gBuffer) {
		return;
	}
	auto* rhi = Engine::instance()->rhi();
	const auto camera = renderDataHandle.mCameraPos;
	if (!mBlock) {
		mBlock = rhi->allocTransient(mParams);
	}
	const auto& block = mBlock;

	if (!mMapBaked) {
		bakeMap(cmd, block);
		mMapBaked = true;
	}

	const auto set = mSets[rhi->frameInFlight() % mSets.size()];
	rhi->updateDescriptorSets({
		{ .set = set, .binding = 5, .buffers{ { renderDataHandle.cameraMatricesUBO } } },
		{ .set = set, .binding = 16, .buffers{ { block.buffer, 0, sizeof(GrassParams) } } },
		{ .set = set, .binding = 17, .images{ { mHeights, mNearest } } },
		{ .set = set, .binding = 18, .images{ { mMap, mLinear } } },
		{ .set = set, .binding = 19, .images{ { mForest, mLinear } } },
	});

	const auto& screen = Engine::instance()->getWindow()->getScreenData();
	RenderPassBeginInfo begin;
	begin.renderPass = mGBufferPass;
	begin.framebuffer = gBuffer;
	begin.width = screen.renderW;
	begin.height = screen.renderH;
	rhi->cmdBeginRenderPass(cmd, begin);

	if (mClumpsReady) {
		ClumpDrawParams params;
		params.params[1] = mClumpFadeStart;
		params.params[2] = mClumpReach;
		drawClumps(cmd, mClumpPipeline, params, false);
	}

	enum Kind : uint8_t { Reeds = 1, Lilies, Cattails };
	struct Tier {
		Kind kind;
		float spacing;
		float width;
		float fadeInStart;
		float fadeInEnd;
		float fadeOutStart;
		float fadeOutEnd;
		int plantMesh = 0;   // which of mPlantMeshes the tier draws
		int seed = -1;       // the tier whose plants these are, when it is another's at a different detail
	};
	// They reach far: they stand out against the water from across a bay.
	const Tier tiers[] = {
		// The same clumps in less detail at 12 and 35 m; past 90 m a sparser grid of the simplest ones. The cattails
		// the same way on their own grid.
		{ Reeds, 0.33f, 0.03f, 0.f, 0.f, 11.9f, 12.f, 0 },
		{ Reeds, 0.33f, 0.03f, 11.9f, 12.f, 34.9f, 35.f, 1, 2 },
		{ Reeds, 0.33f, 0.03f, 34.9f, 35.f, 70.f, 90.f, 2, 2 },
		{ Reeds, 0.9f, 0.05f, 60.f, 85.f, 200.f, 260.f, 2 },
		{ Cattails, 0.55f, 0.f, 0.f, 0.f, 34.9f, 35.f, 3 },
		{ Cattails, 0.55f, 0.f, 34.9f, 35.f, 200.f, 260.f, 4, 6 },
		// Pads in less detail at 25 and 60 m, and the flowers on some of them up to 60 m.
		{ Lilies, 0.6f, 0.f, 0.f, 0.f, 24.9f, 25.f, 5 },
		{ Lilies, 0.6f, 0.f, 24.9f, 25.f, 59.9f, 60.f, 6, 8 },
		{ Lilies, 0.6f, 0.f, 59.9f, 60.f, 110.f, 140.f, 7, 8 },
		{ Lilies, 0.6f, 0.f, 0.f, 0.f, 59.9f, 60.f, 8, 8 },
	};
	// What ground a chunk of this kind needs somewhere under it, and how high above that ground it reaches.
	const auto growsIn = [&](const Kind kind, float ox, float oz, float chunk, float& low, float& high) {
		const float localX = ox - (mParams.terrain[0] - mParams.terrain[3] * 0.5f);
		const float localZ = oz - (mParams.terrain[2] - mParams.terrain[3] * 0.5f);
		const int bx0 = std::clamp(static_cast<int>(std::floor(localX / kBandCell)), 0, mBandSide - 1);
		const int bx1 = std::clamp(static_cast<int>(std::floor((localX + chunk) / kBandCell)), 0, mBandSide - 1);
		const int bz0 = std::clamp(static_cast<int>(std::floor(localZ / kBandCell)), 0, mBandSide - 1);
		const int bz1 = std::clamp(static_cast<int>(std::floor((localZ + chunk) / kBandCell)), 0, mBandSide - 1);
		low = (std::numeric_limits<float>::max)();
		high = std::numeric_limits<float>::lowest();
		for (int bz = bz0; bz <= bz1; ++bz) {
			for (int bx = bx0; bx <= bx1; ++bx) {
				low = (std::min)(low, mBandMin[static_cast<size_t>(bz) * mBandSide + bx]);
				high = (std::max)(high, mBandMax[static_cast<size_t>(bz) * mBandSide + bx]);
			}
		}
		switch (kind) {
		case Reeds:
		case Cattails: { // the shallows, reaching a couple of metres above them
			const bool grows = low < 0.4f && high > -0.9f;
			high = (std::min)(high, 0.4f) + 2.5f;
			return grows;
		}
		case Lilies: { // on the water, over ground between 2.4 and 0.5 metres down
			const bool grows = low < -0.5f && high > -2.4f;
			low = -0.05f;
			high = 0.1f;
			return grows;
		}
		}
		return false;
	};
	const float half = mParams.terrain[3] * 0.5f;
	PipelineHandle bound{};
	for (size_t t = 0; t < std::size(tiers); ++t) {
		const auto& tier = tiers[t];
		const auto pipeline = tier.kind == Lilies ? mLilyPipeline : mPlantPipeline;
		const bool rebind = pipeline.id != bound.id;
		if (rebind) {
			bound = pipeline;
			rhi->cmdBindPipeline(cmd, pipeline);
			rhi->cmdBindDescriptorSets(cmd, pipeline, 0, &set, 1, &block.offset, 1);
		}
		// After the pipeline: OpenGL keeps the vertex buffer on the pipeline's VAO.
		if (rebind) {
			rhi->cmdBindVertexBuffer(cmd, 0, mPlants, 0);
			rhi->cmdBindIndexBuffer(cmd, mPlantIndices, IndexType::Uint32, 0);
		}
		const float chunk = tier.spacing * static_cast<float>(kChunkSide);
		DrawParams draw;
		draw.spacing = tier.spacing;
		draw.side = static_cast<float>(kChunkSide);
		draw.width = tier.width;
		draw.fadeInStart = tier.fadeInStart;
		draw.fadeInEnd = tier.fadeInEnd;
		draw.fadeOutStart = tier.fadeOutStart;
		draw.fadeOutEnd = tier.fadeOutEnd;
		draw.tier = static_cast<float>(tier.seed >= 0 ? static_cast<size_t>(tier.seed) : t);
		draw.kind = static_cast<float>(tier.kind);

		const int x0 = static_cast<int>(std::floor((camera.x - tier.fadeOutEnd) / chunk));
		const int x1 = static_cast<int>(std::floor((camera.x + tier.fadeOutEnd) / chunk));
		const int z0 = static_cast<int>(std::floor((camera.z - tier.fadeOutEnd) / chunk));
		const int z1 = static_cast<int>(std::floor((camera.z + tier.fadeOutEnd) / chunk));
		for (int cz = z0; cz <= z1; ++cz) {
			for (int cx = x0; cx <= x1; ++cx) {
				const float ox = static_cast<float>(cx) * chunk;
				const float oz = static_cast<float>(cz) * chunk;
				// Off the terrain, past this tier's reach, or wholly inside the region the finer tier covers.
				if (ox + chunk < mParams.terrain[0] - half || ox > mParams.terrain[0] + half
					|| oz + chunk < mParams.terrain[2] - half || oz > mParams.terrain[2] + half) {
					continue;
				}
				const float nearX = std::clamp(camera.x, ox, ox + chunk) - camera.x;
				const float nearZ = std::clamp(camera.z, oz, oz + chunk) - camera.z;
				if (nearX * nearX + nearZ * nearZ > tier.fadeOutEnd * tier.fadeOutEnd) {
					continue;
				}
				const float farX = (std::max)(std::fabs(ox - camera.x), std::fabs(ox + chunk - camera.x));
				const float farZ = (std::max)(std::fabs(oz - camera.z), std::fabs(oz + chunk - camera.z));
				if (farX * farX + farZ * farZ < tier.fadeInStart * tier.fadeInStart) {
					continue;
				}
				float groundLow = 0.f;
				float groundHigh = 0.f;
				if (!growsIn(tier.kind, ox, oz, chunk, groundLow, groundHigh)) {
					continue;
				}
				groundLow += mParams.terrain[1];
				groundHigh += mParams.terrain[1];
				const Math::Vec3 centre{ ox + chunk * 0.5f, (groundLow + groundHigh) * 0.5f, oz + chunk * 0.5f };
				const Math::Vec3 extents{ chunk * 0.5f + 1.f, (groundHigh - groundLow) * 0.5f + 0.5f, chunk * 0.5f + 1.f };
				if (!FrustumModule::AABB::isOnFrustum(renderDataHandle.mCamFrustum, centre, extents)) {
					continue;
				}
				draw.originX = ox;
				draw.originZ = oz;
				rhi->cmdPushConstants(cmd, pipeline, draw, ShaderStage::Vertex);
				const auto& plant = mPlantMeshes[tier.plantMesh];
				rhi->cmdDrawIndexed(cmd, plant.indexCount, static_cast<uint32_t>(kChunkSide * kChunkSide), plant.firstIndex, plant.firstVertex);
			}
		}
	}
	rhi->cmdEndRenderPass(cmd);
}
