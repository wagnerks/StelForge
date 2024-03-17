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
				if (measureData.second.measurements.empty()) {
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
				bool treeOpened = false;
				{
					long long averageTime = 0;
					for (const auto& time : measureData.second.measurements) {
						averageTime += time.delta;
					}
					averageTime /= measureData.second.measurements.size();
					struct Time {

						long long seconds;
						long long millisecond;
						long long microsecond;
						long long nanosecond;
						long long delta;
					};

					Time time;
					time.seconds = averageTime / 1'000'000'000;
					time.millisecond = averageTime / 1'000'000 % 1'000;
					time.microsecond = averageTime / 1'000 % 1'000;
					time.nanosecond = averageTime % 1'000;
					time.delta = averageTime;

					
					if (measureData.first.empty()) {
						ImGui::Text("%s", measureName.c_str());
					}
					else {
						treeOpened = ImGui::TreeNode(measureName.c_str());
					}

					ImGui::PlotLines(std::string("##" + measureName).c_str(), measureData.second.plotData.data(), measureData.second.plotData.size());
					ImGui::SameLine();
					ImGui::Text("avg: %ds: %dms: %dmu: %dns", time.seconds, time.millisecond, time.microsecond, time.nanosecond);
				}

				
				if (treeOpened) {
					ImGui::Separator();
					ImGui::Separator();
					for (auto& [name, timings] : measureData.first) {
						long long averageTime = 0;
						for (const auto& time : timings.measurements) {
							averageTime += time.delta;
						}
						averageTime /= timings.measurements.size();
						struct Time {

							long long seconds;
							long long millisecond;
							long long microsecond;
							long long nanosecond;
							long long delta;
						};

						Time time;
						time.seconds = averageTime / 1'000'000'000;
						time.millisecond = averageTime / 1'000'000 % 1'000;
						time.microsecond = averageTime / 1'000 % 1'000;
						time.nanosecond = averageTime % 1'000;
						time.delta = averageTime;

						ImGui::Text("%s", name.c_str());
						ImGui::PlotLines(std::string("##" + name).c_str(), timings.plotData.data(), timings.plotData.size());
						ImGui::SameLine();
						ImGui::Text("avg: %ds: %dms: %dmu: %dns", time.seconds, time.millisecond, time.microsecond, time.nanosecond);
					}


					ImGui::TreePop();
				}
			}
		}
		ImGui::End();
	}
}

