#include "Benchmark.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

namespace SFE::Debug {
	void BenchmarkGUI::onGui() {
		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Debug")) {
					ImGui::Checkbox("benchmark", &windowOpened);
					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();
		}
		if (!windowOpened) {
			return;
		}

		if (ImGui::Begin("performance stats", &windowOpened)) {
			if (ImGui::InputText("Filter", &filter)) {
				std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) {
					return std::tolower(c);
				});
			}

			for (auto& [measureName, measureData] : measurements) {
				if (measureData.measurements.empty()) {
					continue;
				}

				if (filter.size()) {
					std::string compareStr = measureName;
					std::transform(compareStr.begin(), compareStr.end(), compareStr.begin(), [](unsigned char c) {
						return std::tolower(c);
					});
					if (compareStr.find(filter) == std::string::npos) {
						continue;
					}
				}
				
				long long averageTime = 0;
				for (const auto& time : measureData.measurements) {
					averageTime += time.delta;
				}
				averageTime /= measureData.measurements.size();

				ImGui::Text("%s", measureName.c_str());
				ImGui::PlotLines(std::string("##" + measureName).c_str(), measureData.plotData.data(), measureData.plotData.size());
				ImGui::SameLine();
				ImGui::Text("average: %d ns", averageTime);
			}
		}
		ImGui::End();
	}
}

