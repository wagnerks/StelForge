#pragma once
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>
#include <vector>
#include <cwchar>
#include <algorithm>

namespace SFE::Debug {

	class SystemLoadWin {
	public:
		bool init() {
			if (inited_) return true;

			// Defer until a graphics context likely exists (some drivers expose GPU Engine only after usage)
			if (PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS) return false;
			if (!addCounter(L"\\Processor(_Total)\\% Processor Time", cpuCounter_)) return false;

			// First attempt
			gpuAvailable_ = initGpuCounters_();
			if (!gpuAvailable_) {
				// Try a refresh of the performance objects catalogue
				refreshObjects_();
				gpuAvailable_ = initGpuCounters_();
			}

			PdhCollectQueryData(query_); // prime (first sample is always 0)
			primed_ = true;
			inited_ = true;

			if (!gpuAvailable_) {
				OutputDebugStringW(L"[SystemLoadWin] GPU Engine counters not found (will report -1)\n");
			}
			return true;
		}
		double sampleAcc_ = 0.0;
		const double sampleEverySec_ = 0.2; // опрос PDH 5 Гц

		// сглаженные значения и константа времени для EMA
		double cpuEma_ = 0.0, gpuEma_ = 0.0;
		const double tauSec_ = 0.8; // 0.6–1.0 c нормально

		// helper: коэффициент EMA под реальный шаг Δt
		static inline double emaAlpha(double dt, double tau) {
			// α = 1 - e^{-Δt/τ}
			return 1.0 - std::exp(-dt / max(1e-3, tau));
		}

		// вызывай из игрового апдейта и передавай dt (сек)
		void update(double dt)
		{
			if (!primed_) return;
			sampleAcc_ += dt;
			if (sampleAcc_ < sampleEverySec_) {
				// только сглаживаем к текущему (без нового сэмпла)
				double a = emaAlpha(dt, tauSec_);
				cpuEma_ += a * (cpuLoad_ - cpuEma_);
				gpuEma_ += a * (gpuLoad_ - gpuEma_);
				return;
			}
			sampleAcc_ = 0.0;

			if (PdhCollectQueryData(query_) != ERROR_SUCCESS) return;

			// raw samples
			double cpuNow = readSingle_(cpuCounter_);
			double gpuNow = 0.0;
			if (gpuAvailable_) {
				// если читаешь массивом — посчитай sum здесь; если уже хранится sum — оставь как есть
				for (auto c : gpuCounters_) gpuNow += readSingle_(c);
				gpuNow = std::clamp(gpuNow, 0.0, 100.0);
			}

			cpuLoad_ = cpuNow;
			gpuLoad_ = gpuNow;

			// EMA обновляем с шагом опроса
			double a = emaAlpha(sampleEverySec_, tauSec_);
			cpuEma_ += a * (cpuNow - cpuEma_);
			gpuEma_ += a * (gpuNow - gpuEma_);
		}

		// В UI показывай сглаженные значения:
		double cpuPercentUI() const { return cpuEma_; }
		double gpuPercentUI() const { return gpuAvailable_ ? gpuEma_ : -1.0; }

		//void update() {
		//	if (!primed_) return;

		//	if (PdhCollectQueryData(query_) != ERROR_SUCCESS)
		//		return;

		//	cpuLoad_ = readSingle_(cpuCounter_);

		//	if (gpuAvailable_) {
		//		double sum = 0.0;
		//		for (auto c : gpuCounters_) sum += readSingle_(c);
		//		// Some drivers report overlapping engine times; clamp to 100
		//		gpuLoad_ = std::clamp(sum, 0.0, 100.0);
		//	}
		//	else {
		//		static int notify = 0;
		//		if (notify++ == 120) { // log once in a while, not every frame
		//			OutputDebugStringW(L"[SystemLoadWin] No GPU engine counters available.\n");
		//			notify = 0;
		//		}
		//	}
		//}

		double cpu() const { return cpuLoad_; }      // %
		double gpu() const { return gpuLoad_; }      // % (-1 if unavailable)

	private:
		// Add a single PDH counter (English name first, then localized fallback)
		static bool addCounter(const wchar_t* path, PDH_HCOUNTER& out) {
			PDH_STATUS s = PdhAddEnglishCounterW(query_, path, 0, &out);
			if (s == PDH_CSTATUS_BAD_COUNTERNAME)
				s = PdhAddCounterW(query_, path, 0, &out);
			if (s != ERROR_SUCCESS) {
				logCounterFail_(path, s);
				return false;
			}
			return true;
		}

		bool initGpuCounters_() {
			DWORD szCounters = 0, szInstances = 0;
			PDH_STATUS pre = PdhEnumObjectItemsW(nullptr, nullptr,
				L"GPU Engine",
				nullptr, &szCounters,
				nullptr, &szInstances,
				PERF_DETAIL_WIZARD, 0);

			if (pre != PDH_MORE_DATA) {
				return false;
			}
			if (!szCounters || !szInstances) return false;

			std::wstring counters(szCounters, L'\0');
			std::wstring instances(szInstances, L'\0');

			PDH_STATUS st = PdhEnumObjectItemsW(nullptr, nullptr,
				L"GPU Engine",
				counters.data(), &szCounters,
				instances.data(), &szInstances,
				PERF_DETAIL_WIZARD, 0);

			if (st != ERROR_SUCCESS) {
				return false;
			}

			const DWORD myPid = GetCurrentProcessId();
			const std::wstring pidToken = L"pid_" + std::to_wstring(myPid) + L"_";

			for (const wchar_t* p = instances.c_str(); *p; p += wcslen(p) + 1) {
				std::wstring name = p;

				// оставить только свой процесс
				if (name.find(pidToken) == std::wstring::npos) continue;

				// оставить только 3D/Compute
				if (name.find(L"engtype_3D") == std::wstring::npos &&
					name.find(L"engtype_Compute") == std::wstring::npos) continue;

				std::wstring path = L"\\GPU Engine(" + name + L")\\Utilization Percentage";
				PDH_HCOUNTER h{};
				if (addCounter(path.c_str(), h)) gpuCounters_.push_back(h);
			}

			if (gpuCounters_.empty()) {
				return false;
			}

			OutputDebugStringW(L"[SystemLoadWin] GPU Engine counters registered.\n");
			return true;
		}

		// Force refresh of performance objects (may help right after boot / driver init)
		static void refreshObjects_() {
			DWORD sz = 0;
			PdhEnumObjectsW(nullptr, nullptr, nullptr, &sz, PERF_DETAIL_WIZARD, TRUE);
			if (sz) {
				std::wstring buf(sz, L'\0');
				if (PdhEnumObjectsW(nullptr, nullptr, buf.data(), &sz, PERF_DETAIL_WIZARD, TRUE) == ERROR_SUCCESS) {
					OutputDebugStringW(L"[SystemLoadWin] Refreshed PDH objects.\n");
				}
			}
		}

		static double readSingle_(PDH_HCOUNTER h) {
			PDH_FMT_COUNTERVALUE v{};
			if (PdhGetFormattedCounterValue(h, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS) {
				return std::clamp(v.doubleValue, 0.0, 100.0);
			}
			return 0.0;
		}

		static void logCounterFail_(const wchar_t* path, PDH_STATUS st) {
			wchar_t buf[512];
			swprintf_s(buf, L"[SystemLoadWin] Failed to add counter \"%s\" (0x%08X)\n", path, st);
			OutputDebugStringW(buf);
		}

	private:
		inline static PDH_HQUERY query_{};
		PDH_HCOUNTER cpuCounter_{};
		std::vector<PDH_HCOUNTER> gpuCounters_;
		bool primed_ = false;
		bool gpuAvailable_ = false;
		bool inited_ = false;
		double cpuLoad_ = 0.0;
		double gpuLoad_ = -1.0; // -1 => unavailable
	};
}
