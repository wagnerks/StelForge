#pragma once
#include "renderModule/Gizmo.h"

namespace SFE::ComponentsModule {
	struct GizmoComponent {
		Render::Gizmo gizmo;
	};
}

template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::GizmoComponent> : std::true_type {};
