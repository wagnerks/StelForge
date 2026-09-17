#pragma once

#include <string_view>
#include <vector>

#include <json/value.h>

#include "TypeName.h"
#include "core/ECSHandler.h"

namespace SFE::PropertiesModule {
	/**
	 * @brief The JSON contract for one component type.
	 *
	 * Specialize for every component that belongs in a scene file; a component without a
	 * specialization is runtime-only and is never written or read.
	 *
	 * Declared outside the component on purpose. A virtual serialize()/deserialize() pair
	 * gives the component a vtable, which makes it non-trivially-copyable, which makes the
	 * whole ecss array non-trivial (see SectorLayoutMeta::initData): insertion and
	 * defragmentation then move and destroy element by element instead of one memmove.
	 *
	 * save() receives the component, load() receives the entity, because loading may have to
	 * create the component or write through to a neighbouring one.
	 */
	template<class T>
	struct ComponentSerializer;

	/// @brief When a component can be read back while a scene is loading.
	enum class LoadPhase {
		Never,     ///< Written for inspection only; restored by whoever owns that state.
		Direct,    ///< As soon as the entity exists.
		AfterBind  ///< After bindRenderable(), for components that write into the built mesh.
	};

	/**
	 * @brief The one list of scene-serializable components.
	 *
	 * Both directions walk this table, so a component can no longer be written but never read
	 * back -- which is what CascadeShadowComponent silently did while saving and loading were
	 * two separate hand-written lists. Adding a component is one entry plus its
	 * ComponentSerializer.
	 */
	class ComponentRegistry {
	public:
		struct Entry {
			std::string_view name;
			LoadPhase loadPhase;
			void (*save)(ecss::EntityId, Json::Value&);
			void (*load)(ecss::EntityId, const Json::Value&);
		};

		/// @brief The engine's own components, and whatever a game added. @see SceneComponents.cpp
		static const std::vector<Entry>& entries();

		/// @brief Add a component of a game's, so scenes carry it and the editor can show it.
		static void add(Entry entry);

		/// @brief Add T, taking its contract from its ComponentSerializer.
		template<class T>
		static void add(LoadPhase phase) { add(entryFor<T>(phase)); }

		/// @brief The engine's own components, in the order they are read. @see SceneComponents.cpp
		static std::vector<Entry> engineEntries();

		/// @brief Write every registered component this entity has into @p properties.
		static void save(ecss::EntityId entity, Json::Value& properties) {
			if (entity == ecss::INVALID_ID) {
				return;
			}
			for (const auto& entry : entries()) {
				entry.save(entity, properties);
			}
		}

		/// @brief Read back the components of @p phase that @p properties carries.
		static void load(ecss::EntityId entity, const Json::Value& properties, LoadPhase phase) {
			if (entity == ecss::INVALID_ID || !properties.isObject()) {
				return;
			}
			for (const auto& entry : entries()) {
				if (entry.loadPhase == phase && properties.isMember(entry.name.data())) {
					entry.load(entity, properties[entry.name.data()]);
				}
			}
		}

		/// @brief Build a table entry for T out of its ComponentSerializer.
		template<class T>
		static Entry entryFor(LoadPhase phase) {
			return Entry{
				TypeName<T>::name(),
				phase,
				[](ecss::EntityId entity, Json::Value& properties) {
					if (auto component = ECSHandler::registry().pinComponent<const T>(entity)) {
						ComponentSerializer<T>::save(*component, properties[TypeName<T>::name().data()]);
					}
				},
				[](ecss::EntityId entity, const Json::Value& data) {
					ComponentSerializer<T>::load(entity, data);
				}
			};
		}
	};
}
