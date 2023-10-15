#pragma once

#include <cassert>

#include "ComponentsArraySector.h"
#include "ComponentsArrayUtils.h"
#include "Reflection.h"

namespace ecss::Memory {
	/* data container with sectors of custom data in it
	 *
	 * the Sector is object in memory which includes SectorInfo {uint32_t(by default) sectorId; unsigned long nullBits;}
	 * nullBits is the value, which contains information about custom data in sector availability
	 * for example the object on the first place is not created, or deleted, first bit of nullBits == 0, the third object of sector is created, third bit == 1
	 * it allows to return nullptr if bit is zero
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
	 * N can be any number, but here locked on 32 for optimization purposes, if more then 32 - need to adjust nullBits memory
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
			IteratorSectors() : mPtr(nullptr), mSectorSize(0) {}
			IteratorSectors(void* ptr, const ChunkData& data) : mPtr(ptr), mSectorSize(data.sectorSize) {}

			inline bool operator!=(const IteratorSectors& other) const { return mPtr != other.mPtr; }
			inline bool operator==(const IteratorSectors& other) const { return mPtr == other.mPtr; }

			inline IteratorSectors& operator=(const IteratorSectors& other) {
				if (this != &other) {
					mPtr = other.mPtr;
					mSectorSize = other.mSectorSize;
				}
				return *this;
			}

			inline void* operator*() const { return mPtr; }

			inline IteratorSectors& operator++() { return mPtr = static_cast<char*>(mPtr) + mSectorSize, *this; }

			inline IteratorSectors& operator+(size_t i) { return mPtr = static_cast<char*>(mPtr) + i * mSectorSize, *this; }
		private:
			void* mPtr;
			uint16_t mSectorSize;
		};

	protected:
		template <typename... Types>
		void initChunkData() {
			size_t idx = 2; //first is 0, 1 is for sector data
			((mChunkData.sectorMembersOffsets[idx++] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(Types)) + 1), ...);
			mChunkData.sectorCapacity = static_cast<uint8_t>(idx - 2);

			uint8_t i = 0;
			((mChunkData.sectorMembersIndexes[ReflectionHelper::getTypeId<Types>()] = ++i), ...);
		}

	public:
		ComponentsArray(uint32_t capacity = 0);
		virtual ~ComponentsArray();

		IteratorSectors beginSectors() const;
		IteratorSectors endSectors() const;

		inline SectorInfo* operator[](size_t i) const { return static_cast<SectorInfo*>(static_cast<void*>(static_cast<char*>(mData) + i * mChunkData.sectorSize)); }

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

		void* mData = nullptr;

	public:
		class Iterator {
		public:
			inline constexpr EntityId getSectorId() const { return *static_cast<EntityId*>(mPtr); }

			inline Iterator() {};
			inline Iterator(void* ptr, uint16_t sectorSize, std::array<uint16_t, 10> offsets, ContiguousMap<ECSType, uint8_t> members) : mPtr(ptr), mSectorSize(sectorSize), mOffsets(offsets), mMembers(members) {}

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

		public:
			void* mPtr = nullptr;
			uint16_t mSectorSize = 0;

			std::array<uint16_t, 10> mOffsets = {};
			ContiguousMap<ECSType, uint8_t> mMembers;
		};

		Iterator begin() { return { mData, mChunkData.sectorSize, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes }; }
		Iterator end() { return { static_cast<char*>(mData) + size() * mChunkData.sectorSize, mChunkData.sectorSize, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes }; }
	};

	template <typename... Types>
	class ComponentsArrayInitializer final : public ComponentsArray  {
		ComponentsArrayInitializer(const ComponentsArrayInitializer&) = delete;
		ComponentsArrayInitializer& operator=(ComponentsArrayInitializer&) = delete;

	public:
		ComponentsArrayInitializer(uint32_t capacity = 0) : ComponentsArray(capacity){
			static_assert(Utils::areUnique<Types...>(), "Duplicates detected in types");
			static_assert(sizeof...(Types) <= 32, "More then 32 components in one container not allowed");

			initChunkData<Types...>();
			reserve(capacity);
		}
	};
}
