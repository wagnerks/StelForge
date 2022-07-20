#pragma once
namespace GameEngine::ComponentsModule {
	class ComponentHolder;

	class Component {
	public:
		Component(ComponentHolder* holder);
		virtual ~Component() = default;
		virtual void updateComponent() = 0;

		Component(const Component& other) = delete;
		Component(Component&& other) noexcept = delete;
		Component& operator=(const Component& other) = delete;
		Component& operator=(Component&& other) noexcept = delete;
	protected:
		ComponentHolder* owner = nullptr;
	};
}