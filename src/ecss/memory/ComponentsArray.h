#pragma once

#include <cassert>
#include <vector>

#include "ComponentsArraySector.h"

namespace ecss::Memory {

	class ComponentsArray {
		ComponentsArray(const ComponentsArray&) = delete;
		ComponentsArray& operator=(ComponentsArray&) = delete;

	public:
		template<typename T>
		class Iterator {
		private:
			using VoidIt = SectorsChunk::Iterator;
			const uint16_t mOffset = 0;
			const uint8_t mTypePos = 0;
			std::array<uint16_t, 34> offsets;
			std::map<ECSType, uint8_t> members;
		public:
			VoidIt mIt;

			EntityId getSectorId() const { return *static_cast<EntityId*>(*mIt); }

			inline Iterator() {};
			inline Iterator(const VoidIt& listIt, uint16_t offset, uint8_t typePos, std::array<uint16_t, 34> offsets, std::map<ECSType, uint8_t> members) : mOffset(offset), mTypePos(typePos), mIt(listIt), offsets(offsets), members(members){}
			
			inline T* operator*() {
				return getTyped<T>();
			}

			template<typename Type>
			inline bool isType() {
				return members.contains(Type::STATIC_COMPONENT_TYPE_ID);
			}

			template<typename Type>
			inline Type* getTyped() {
				return static_cast<SectorInfo*>(*mIt)->nullBits[Type::STATIC_COMPONENT_SECTOR_IDX - 1] ? static_cast<Type*>(Utils::getTypePlace(*mIt, offsets[Type::STATIC_COMPONENT_SECTOR_IDX])) : nullptr;
			}

			inline Iterator& operator+(size_t i) {	mIt = mIt + i;	return *this; }
			inline Iterator& operator++() {
				++mIt;
				return *this;
			}

			inline bool operator!=(const Iterator& other) const { return mIt != other.mIt; }
		};

		template <typename T>
		Iterator<T> begin() {

			return { mChunk->begin(), mChunkData.sectorMembersOffsets[T::STATIC_COMPONENT_SECTOR_IDX], T::STATIC_COMPONENT_SECTOR_IDX, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes };
		}

		template<typename T>
		Iterator<T> end() { return { mChunk->end(), 0, 0, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes }; }

		ComponentsArray(size_t capacity = 1);
		virtual ~ComponentsArray();

		size_t size() const;
		bool empty() const;
		void clear();
		size_t capacity() const;
		size_t entitiesCapacity() const;
		void reserve(size_t newCapacity);
		void shrinkToFit();

		void* acquireSector(uint8_t componentTypeIdx, EntityId entityId);
		void destroyObject(ECSType componentTypeId, EntityId entityId);
		void destroySector(EntityId entityId);

		template<typename T>
		inline T* getComponent(const EntityId& sectorId) {
			if (!T::STATIC_COMPONENT_SECTOR_IDX || sectorId >= mSectorsMap.size()) {
				return nullptr;
			}

			const auto sectorPtr = mSectorsMap[sectorId];
			if (!sectorPtr || !static_cast<SectorInfo*>(sectorPtr)->nullBits[T::STATIC_COMPONENT_SECTOR_IDX - 1]) {
				return nullptr;
			}

			return static_cast<T*>(Utils::getTypePlace(sectorPtr, mChunkData.sectorMembersOffsets[T::STATIC_COMPONENT_SECTOR_IDX]));
		}

		template<typename T>
		void moveToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_SECTOR_IDX, sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			data->mOwnerId = sectorId;
			new (sector) T(std::move(*data));
		}

		template<typename T>
		void copyToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_SECTOR_IDX, sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			data->mOwnerId = sectorId;
			new (sector) T(*data);
		}

	private:
		void* initSectorMember(void* sectorPtr, uint8_t componentTypeIdx);

		void setCapacity(size_t newCap);

		void* createSector(size_t pos, EntityId sectorId);
		void destroyObject(void* sectorPtr, uint8_t typeIdx) const;
		void destroySector(void* sectorPtr) const;

		void erase(size_t pos);

		std::vector<void*> mSectorsMap;
		SectorsChunk* mChunk = nullptr;
		size_t mSize = 0;
		size_t mCapacity = 0;

	protected:
		template <typename... Types>
		void initChunkData() {
			size_t idx = 2; //first is 0, 1 is for sector data
			((mChunkData.sectorMembersOffsets[idx++] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(Types)) + alignof(Types)), ...);


			uint8_t i = 0;
			((mChunkData.sectorMembersIndexes[Types::STATIC_COMPONENT_TYPE_ID] = ++i), ...);
			i = 0;
			((Types::STATIC_COMPONENT_SECTOR_IDX = ++i), ...);
		}

		void allocateChunk();
		ChunkData mChunkData;
	};

	template <typename... Types>
	class ComponentsArrayInitializer : public ComponentsArray {
		ComponentsArrayInitializer(const ComponentsArrayInitializer&) = delete;
		ComponentsArrayInitializer& operator=(ComponentsArrayInitializer&) = delete;

	public:
		ComponentsArrayInitializer(size_t capacity = 1) : ComponentsArray(capacity){
			static_assert(Utils::areUnique<Types...>(), "Duplicates detected in types");
			static_assert(sizeof...(Types) <= 32, "More then 32 components in one container not allowed");

			initChunkData<Types...>();

			allocateChunk();
		}
	};
}
