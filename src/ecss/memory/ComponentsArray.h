#pragma once

#include <cassert>

#include "ComponentsArraySector.h"
#include "ComponentsArrayUtils.h"
#include "Reflection.h"

namespace ecss::Memory {
	/* data container with sectors of custom data in it
	 *
	 * the Sector is object in memory which includes SectorInfo {uint32_t(by default) sectorId;}
	 *
	 * sector stores data for any custom type in theory, offset to type stores in ChunkData struct
	 *--------------------------------------------------------------------------------------------
	 *                                          [SECTOR]
	 * 0x 00                                                           {SectorInfo}
	 * 0x sizeof(SectorInfo)                                           {SomeObject}
	 * 0x sizeof(SectorInfo + SomeObject)                              {SomeObject1}
	 * 0x sizeof(SectorInfo + SomeObject + SomeObject1)                {SomeObject2}
	 * ...
	 * 0x sizeof(SectorInfo... + ...SomeObjectN-1 + SomeObjectN)       {SomeObjectN}
	 *
	 *--------------------------------------------------------------------------------------------
	 *
	 * objects from sector can be pulled as void*, and then casted to desired type, as long as the information about offsets for every object stored according to their type ids
	 * you can't get object without knowing it's type
	 *
	 */
	class ComponentsArray {
	private:
		ComponentsArray(const ComponentsArray&) = delete;
		ComponentsArray& operator=(ComponentsArray&) = delete;

		//shifts chunk data right
		//[][][][from][][][]   -> [][][] [empty] [from][][][]
		void shiftDataRight(size_t from);

		//shifts chunk data left
		//[][][][from][][][]   -> [][][from][][][]
		//caution - shifting on alive data will produce memory leak
		void shiftDataLeft(size_t from, size_t offset = 1);

		bool isSectorAlive(SectorInfo* sector) const;

		class IteratorSectors {
		public:
			IteratorSectors(){}
			IteratorSectors(ComponentsArray* ptr, size_t idx) : mArray(ptr), mIdx(idx) {}

			inline bool operator!=(const IteratorSectors& other) const { return mIdx != other.mIdx; }
			inline bool operator==(const IteratorSectors& other) const { return mIdx == other.mIdx; }

			inline IteratorSectors& operator=(const IteratorSectors& other) {
				if (this != &other) {
					mIdx = other.mIdx;
					mArray = other.mArray;
				}
				return *this;
			}

			inline void* operator*() const { return (*mArray)[mIdx]; }

			inline IteratorSectors& operator++() { return ++mIdx, *this; }

			inline IteratorSectors& operator+(size_t i) { return mIdx += i, *this; }
		private:
			ComponentsArray* mArray = nullptr;
			size_t mIdx = 0;
		};

	protected:
		template <typename... Types>
		void initChunkData() {
			((mChunkData.sectorMembersOffsets.emplace_back(mChunkData.sectorSize += static_cast<uint16_t>(sizeof(Types)) + 1)), ...);
			mChunkData.sectorMembersOffsets.shrink_to_fit();

			uint8_t i = 0;
			((mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<Types>()] = ++i), ...);
		}

	public:
		ComponentsArray(uint32_t capacity = 0);
		virtual ~ComponentsArray();

		IteratorSectors beginSectors();
		IteratorSectors endSectors();

		inline SectorInfo* operator[](size_t i) const { return mChunks.size() <= i / mChunkSize ? nullptr : static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>(mChunks[i / mChunkSize]) + (i % mChunkSize) * mChunkData.sectorSize)); }

		uint32_t size() const;
		bool empty() const;
		void clear();

		uint32_t capacity() const;
		void reserve(uint32_t newCapacity);
		void shrinkToFit();

		size_t entitiesCapacity() const;

		void* acquireSector(uint8_t componentTypeIdx, EntityId entityId);
		void destroyObject(ECSType componentTypeId, EntityId entityId);
		void destroyObjects(ECSType componentTypeId, std::vector<EntityId> entityIds);
		void destroySector(EntityId entityId);

		inline SectorInfo* getSectorInfo(EntityId sectorId) const {
			return sectorId >= mSectorsMap.size() || mSectorsMap[sectorId] >= size() ? nullptr : (*this)[mSectorsMap[sectorId]];
		}

		template<typename T>
		inline T* getComponent(EntityId sectorId) {
			return getComponentImpl<T>(sectorId, mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<T>()]);
		}

		template<typename T>
		inline T* getComponentImpl(EntityId sectorId, uint8_t sectorIdx) {
			auto info = getSectorInfo(sectorId);
			return info ? info->getObject<T>(mChunkData.sectorMembersOffsets[sectorIdx]) : nullptr;
		}

		template<typename T>
		void moveToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(ReflectionHelper::getTypeId<T>())) {
				assert(false);
				return;
			}
			
			auto sector = acquireSector(mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<T>()], sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			new (sector) T(std::move(*data));
		}

		template<typename T>
		void copyToSector(EntityId sectorId, T* data) {
			if (!data || !mChunkData.sectorMembersIndexes.contains(ReflectionHelper::getTypeId<T>())) {
				assert(false);
				return;
			}

			auto sector = acquireSector(mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<T>()], sectorId);
			if (!sector) {
				assert(false);
				return;
			}

			new (sector) T(*data);
		}

		template<typename T>
		inline uint8_t getTypeIdx() {
			return mChunkData.sectorMembersIndexes.contains(ReflectionHelper::getTypeId<T>()) ? mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<T>()] : 0;
		}

		template<typename T>
		inline bool hasType() const {
			return mChunkData.sectorMembersIndexes.contains(ReflectionHelper::getTypeId<T>());
		}
		const ChunkData& getChunkData() { return mChunkData; }
	private:
		void* initSectorMember(void* sectorPtr, uint8_t componentTypeIdx) const;

		void setCapacity(uint32_t newCap);

		void* createSector(size_t pos, EntityId sectorId);
		void destroyObject(void* sectorPtr, uint8_t typeIdx) const;
		void destroySector(void* sectorPtr) const;

		void erase(size_t pos);
		void erase(size_t from, size_t to);

		std::vector<EntityId> mSectorsMap;

		ChunkData mChunkData;

		uint32_t mSize = 0;
		uint32_t mCapacity = 0;

		size_t mChunkSize = 512;
		std::vector<void*> mChunks;

	public:
		class Iterator {
		public:
			inline constexpr EntityId getSectorId() const { return *static_cast<EntityId*>(mPtr); }

			inline Iterator() {};
			inline Iterator(void* ptr, uint16_t sectorSize, std::vector<uint16_t> offsets, ContiguousMap<ECSType, uint8_t> members) : mPtr(ptr), mSectorSize(sectorSize), mOffsets(offsets), mMembers(members) {}

			inline SectorInfo* operator*() const { return static_cast<SectorInfo*>(mPtr); }

			template<typename Type>
			inline bool hasType() { return mMembers.contains(Memory::ReflectionHelper::getTypeId<Type>()); }

			template<typename Type>
			inline Type* getTyped(uint8_t typeIdx) { return static_cast<SectorInfo*>(mPtr)->getObject<Type>(mOffsets[typeIdx]); }

			template<typename Type>
			inline uint8_t getTypeIdx() { return mMembers[Memory::ReflectionHelper::getTypeId<Type>()]; }

			inline Iterator& operator+(size_t i) { return mPtr = static_cast<char*>(mPtr) + i * mSectorSize, *this; }
			inline Iterator& operator++() { return mPtr = static_cast<char*>(mPtr) + mSectorSize, *this; }

			inline bool operator!=(const Iterator& other) const { return mPtr != other.mPtr; }

		private:
			void* mPtr = nullptr;
			uint16_t mSectorSize = 0;

			std::vector<uint16_t> mOffsets = {};
			ContiguousMap<ECSType, uint8_t> mMembers;
		};

		Iterator begin() { return { mChunks.front(), mChunkData.sectorSize, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes}; }
		Iterator end() { return { static_cast<char*>(mChunks.front()) + size() * mChunkData.sectorSize, mChunkData.sectorSize, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes }; }
	};

	template <typename... Types>
	class ComponentsArrayInitializer final : public ComponentsArray  {
		ComponentsArrayInitializer(const ComponentsArrayInitializer&) = delete;
		ComponentsArrayInitializer& operator=(ComponentsArrayInitializer&) = delete;

	public:
		ComponentsArrayInitializer(uint32_t capacity = 0) : ComponentsArray(capacity){
			static_assert(types::areUnique<Types...>(), "Duplicates detected in types");

			initChunkData<Types...>();
			reserve(capacity);
		}
	};
}
