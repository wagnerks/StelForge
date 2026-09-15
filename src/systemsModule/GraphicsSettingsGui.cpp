#include "GraphicsSettingsGui.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

#include "core/Engine.h"
#include "rhi/GraphicsBackend.h"

#include <json/reader.h>
#include <json/value.h>
#include <json/writer.h>
#include <sstream>

namespace SFE::SystemsModule {
	// Frame time over the last few seconds, next to what is drawing it.
	void drawFrameStats() {
		// A capture has to come out the same on both backends and on every run, and none of this does.
		if (Engine::instance()->capturingFrame()) {
			return;
		}
		const auto* info = RenderDebugInfo::instance();
		const auto* rhi = Engine::instance()->rhi();
		ImGui::Text("%s | %s", graphicsBackendName(Render::activeGraphicsBackend()), rhi ? rhi->deviceName() : "");

		float worst = 0.f;
		float sum = 0.f;
		int samples = 0;
		for (const auto ms : info->frameMsHistory) {
			if (ms <= 0.f) {
				continue; // the ring is not full yet
			}
			worst = (std::max)(worst, ms);
			sum += ms;
			++samples;
		}
		const float average = samples ? sum / static_cast<float>(samples) : 0.f;
		// A single stall would flatten everything else against the top of the plot, so the scale follows the
		// average and lets a spike beyond four times it clip; the label still says how bad the worst frame was.
		// The 1.2 is headroom: at a steady 60 fps every sample equals the maximum, and the line would be drawn
		// along the top edge, where it reads as an empty plot.
		const float scale = (std::max)(8.f, (std::min)(worst, average * 4.f)) * 1.2f;

		// The numbers go above the plots rather than into them: PlotLines draws its overlay text across the top,
		// which is exactly where a steady line sits.
		ImGui::Text("frame %.1f ms (%.0f fps)  worst %.1f", static_cast<double>(average),
			average > 0.f ? 1000.0 / static_cast<double>(average) : 0.0, static_cast<double>(worst));
		// The cursor is the oldest sample, so the plot reads left to right in time.
		ImGui::PlotLines("##frameMs", info->frameMsHistory, RenderDebugInfo::kHistory, info->historyCursor, nullptr,
			0.f, scale, ImVec2(0.f, 60.f));
		ImGui::Text("gpu %.1f ms", static_cast<double>(info->gpuMs));
		ImGui::PlotLines("##gpuMs", info->gpuMsHistory, RenderDebugInfo::kHistory, info->historyCursor, nullptr,
			0.f, scale, ImVec2(0.f, 40.f));
	}

	bool drawGraphicsSettings(GraphicsSettings& gfx) {
		bool changed = false;

		drawFrameStats();

		if (ImGui::Button("Reset defaults")) {
			gfx = GraphicsSettings{};
			changed = true;
		}

		if (ImGui::CollapsingHeader("Look", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::SliderFloat("exposure", &gfx.exposure, 0.2f, 2.5f, "%.2f");
			changed |= ImGui::SliderFloat("sun intensity", &gfx.sunIntensity, 0.f, 8.f, "%.2f");
			changed |= ImGui::SliderFloat("IBL", &gfx.iblStrength, 0.f, 2.f, "%.2f");
			changed |= ImGui::SliderFloat("shadow fill", &gfx.shadowAmbient, 0.f, 1.f, "%.2f");
			changed |= ImGui::SliderFloat("bloom", &gfx.bloomStrength, 0.f, 2.f, "%.2f");
			changed |= ImGui::SliderFloat("vignette", &gfx.vignette, 0.f, 0.5f, "%.3f");
		}

		if (ImGui::CollapsingHeader("Fur / skin", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::Checkbox("sheen", &gfx.sheen);
			changed |= ImGui::SliderFloat("sheen strength", &gfx.sheenStrength, 0.f, 3.f, "%.2f");
			changed |= ImGui::SliderFloat("rim", &gfx.rimStrength, 0.f, 2.f, "%.2f");
			changed |= ImGui::Checkbox("SSS", &gfx.sss);
			changed |= ImGui::SliderFloat("sss wrap", &gfx.sssWrap, 0.f, 3.f, "%.2f");
			changed |= ImGui::SliderFloat("sss strength", &gfx.sssStrength, 0.f, 2.5f, "%.2f");
			changed |= ImGui::SliderFloat("sss radius", &gfx.sssRadius, 1.f, 16.f, "%.1f");
		}

		if (ImGui::CollapsingHeader("Passes")) {
			changed |= ImGui::Checkbox("SSAO", &gfx.ssao);
			changed |= ImGui::Checkbox("SSR", &gfx.ssr);
			changed |= ImGui::Checkbox("Volumetric", &gfx.volumetric);
			changed |= ImGui::Checkbox("TAA", &gfx.taa);
			changed |= ImGui::Checkbox("DoF", &gfx.dof);
			changed |= ImGui::Checkbox("Bloom", &gfx.bloom);
			changed |= ImGui::Checkbox("Contact GI", &gfx.contactGi);
		}

		if (ImGui::CollapsingHeader("SSR / Volume")) {
			changed |= ImGui::SliderFloat("ssr distance", &gfx.ssrDistance, 2.f, 80.f, "%.1f");
			changed |= ImGui::SliderFloat("vol density", &gfx.volumetricDensity, 0.f, 0.08f, "%.4f");
			changed |= ImGui::SliderFloat("vol intensity", &gfx.volumetricIntensity, 0.f, 8.f, "%.2f");
			changed |= ImGui::SliderFloat("vol anisotropy", &gfx.volumetricAnisotropy, -0.9f, 0.9f, "%.2f");
			changed |= ImGui::SliderFloat("vol distance", &gfx.volumetricDistance, 10.f, 200.f, "%.1f");
		}

		if (ImGui::CollapsingHeader("TAA / DoF / fog")) {
			changed |= ImGui::SliderFloat("history blend", &gfx.taaBlend, 0.5f, 0.99f, "%.2f");
			changed |= ImGui::SliderFloat("dof focus", &gfx.dofFocus, 1.f, 80.f, "%.1f");
			changed |= ImGui::SliderFloat("dof range", &gfx.dofRange, 1.f, 80.f, "%.1f");
			changed |= ImGui::SliderFloat("dof blur", &gfx.dofBlur, 0.f, 16.f, "%.1f");
			changed |= ImGui::SliderFloat("fog start", &gfx.fogStart, 0.1f, 1.f, "%.2f");
			changed |= ImGui::SliderFloat("CA", &gfx.chromaticAberration, 0.f, 0.012f, "%.4f");
			changed |= ImGui::SliderFloat("glass roughness", &gfx.glassRoughness, 0.04f, 0.6f, "%.2f");
		}

		if (ImGui::CollapsingHeader("GI")) {
			changed |= ImGui::SliderFloat("contact", &gfx.contactStrength, 0.f, 2.f, "%.2f");
			changed |= ImGui::SliderFloat("contact dist", &gfx.contactDistance, 0.2f, 4.f, "%.2f");
			changed |= ImGui::SliderFloat("contact thick", &gfx.contactThickness, 0.01f, 0.5f, "%.2f");
			changed |= ImGui::SliderFloat("gi", &gfx.giStrength, 0.f, 2.f, "%.2f");
			changed |= ImGui::SliderFloat("gi radius", &gfx.giRadius, 0.1f, 2.5f, "%.2f");
		}

		return changed;
	}

	std::string graphicsToJson(const GraphicsSettings& gfx) {
		Json::Value root;
		root["ssao"] = gfx.ssao;
		root["ssr"] = gfx.ssr;
		root["volumetric"] = gfx.volumetric;
		root["taa"] = gfx.taa;
		root["dof"] = gfx.dof;
		root["bloom"] = gfx.bloom;
		root["sheen"] = gfx.sheen;
		root["sss"] = gfx.sss;
		root["contactGi"] = gfx.contactGi;
		root["ssrDistance"] = gfx.ssrDistance;
		root["volumetricDensity"] = gfx.volumetricDensity;
		root["volumetricIntensity"] = gfx.volumetricIntensity;
		root["volumetricAnisotropy"] = gfx.volumetricAnisotropy;
		root["volumetricDistance"] = gfx.volumetricDistance;
		root["taaBlend"] = gfx.taaBlend;
		root["dofFocus"] = gfx.dofFocus;
		root["dofRange"] = gfx.dofRange;
		root["dofBlur"] = gfx.dofBlur;
		root["bloomStrength"] = gfx.bloomStrength;
		root["exposure"] = gfx.exposure;
		root["vignette"] = gfx.vignette;
		root["chromaticAberration"] = gfx.chromaticAberration;
		root["fogStart"] = gfx.fogStart;
		root["glassRoughness"] = gfx.glassRoughness;
		root["sheenStrength"] = gfx.sheenStrength;
		root["rimStrength"] = gfx.rimStrength;
		root["sssWrap"] = gfx.sssWrap;
		root["sssStrength"] = gfx.sssStrength;
		root["sssRadius"] = gfx.sssRadius;
		root["contactStrength"] = gfx.contactStrength;
		root["contactDistance"] = gfx.contactDistance;
		root["contactThickness"] = gfx.contactThickness;
		root["giStrength"] = gfx.giStrength;
		root["giRadius"] = gfx.giRadius;
		root["sunIntensity"] = gfx.sunIntensity;
		root["iblStrength"] = gfx.iblStrength;
		root["shadowAmbient"] = gfx.shadowAmbient;
		Json::StreamWriterBuilder builder;
		builder["indentation"] = "";
		return Json::writeString(builder, root);
	}

	void graphicsFromJson(const Json::Value& in, GraphicsSettings& gfx) {
		auto b = [&](const char* k, bool& v) {
			if (in.isMember(k)) {
				v = in[k].asBool();
			}
		};
		auto f = [&](const char* k, float& v) {
			if (in.isMember(k)) {
				v = in[k].asFloat();
			}
		};
		b("ssao", gfx.ssao);
		b("ssr", gfx.ssr);
		b("volumetric", gfx.volumetric);
		b("taa", gfx.taa);
		b("dof", gfx.dof);
		b("bloom", gfx.bloom);
		b("sheen", gfx.sheen);
		b("sss", gfx.sss);
		b("contactGi", gfx.contactGi);
		f("ssrDistance", gfx.ssrDistance);
		f("volumetricDensity", gfx.volumetricDensity);
		f("volumetricIntensity", gfx.volumetricIntensity);
		f("volumetricAnisotropy", gfx.volumetricAnisotropy);
		f("volumetricDistance", gfx.volumetricDistance);
		f("taaBlend", gfx.taaBlend);
		f("dofFocus", gfx.dofFocus);
		f("dofRange", gfx.dofRange);
		f("dofBlur", gfx.dofBlur);
		f("bloomStrength", gfx.bloomStrength);
		f("exposure", gfx.exposure);
		f("vignette", gfx.vignette);
		f("chromaticAberration", gfx.chromaticAberration);
		f("fogStart", gfx.fogStart);
		f("glassRoughness", gfx.glassRoughness);
		f("sheenStrength", gfx.sheenStrength);
		f("rimStrength", gfx.rimStrength);
		f("sssWrap", gfx.sssWrap);
		f("sssStrength", gfx.sssStrength);
		f("sssRadius", gfx.sssRadius);
		f("contactStrength", gfx.contactStrength);
		f("contactDistance", gfx.contactDistance);
		f("contactThickness", gfx.contactThickness);
		f("giStrength", gfx.giStrength);
		f("giRadius", gfx.giRadius);
		f("sunIntensity", gfx.sunIntensity);
		f("iblStrength", gfx.iblStrength);
		f("shadowAmbient", gfx.shadowAmbient);
	}

	bool graphicsFromJson(const std::string& json, GraphicsSettings& gfx) {
		if (json.empty()) {
			return false;
		}
		Json::Value root;
		Json::CharReaderBuilder builder;
		std::string errors;
		std::istringstream stream(json);
		if (!Json::parseFromStream(builder, stream, &root, &errors)) {
			return false;
		}
		graphicsFromJson(root, gfx);
		return true;
	}
}
