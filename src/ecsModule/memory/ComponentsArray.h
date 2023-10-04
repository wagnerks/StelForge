#pragma once
#include <array>
#include <cassert>
#include <unordered_map>

#include "ECSMemoryStack.h"
#include "settings.h"

namespace ECS::Memory {
	struct SectorsChunk;

	template <class T>
	struct NoHash {	std::size_t operator()(T const& s) const { return static_cast<T>(s); }};

	namespace Utils {
		void* getTypePlace(void* start, ECSType typeId, const std::array<uint16_t, 34>& offsets, const std::unordered_map<ECSType, uint8_t, NoHash<ECSType>>& types);
		void* getTypePlace(void* start, uint16_t offset);

		size_t distance(void* beg, void* end, size_t size);

		void* binarySearch(EntityId sectorId, size_t& idx, SectorsChunk* sectors);
	}

	struct ChunkData {
		size_t chunkCapacity = 0;
		size_t chunkAllocationSize = 0;

		uint16_t sectorSize = 0;
		std::array<uint16_t, 34> sectorMembersOffsets;
		std::unordered_map<ECSType, uint8_t, NoHash<ECSType>> sectorMembersIndexes; // < {type id} , {idx in members offsets} >
	};

	/* SectorsChunk is data container with sectors of custom data in it
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
	struct SectorsChunk  {
		const ChunkData data;

		size_t size = 0;
		void* beginAdr;
		
		SectorsChunk(void* start, const ChunkData& data);
		void* operator[](size_t i);

		class Iterator {
		private:
			void* ptr;
			uint16_t sectorSize;
		public:
			Iterator() : ptr(nullptr), sectorSize(0){}
			Iterator(void* ptr, const ChunkData& data) : ptr(ptr), sectorSize(data.sectorSize){}

			bool operator!=(const Iterator& other) const;
			bool operator==(const Iterator& other) const;

			Iterator& operator=(const Iterator& other);
			void* operator*() const;

			Iterator& operator++();
			Iterator& operator+(size_t i);
		};

		Iterator begin();
		Iterator end();
	};

	/*
	*sector stores data for any custom type in theory, offset to type stores in ChunkData struct
	* --------------------------------------------------------------------------------------------
	*                                       [SECTOR]
	* 0x 00                                                         { SectorInfo  }
	* 0x sizeof(SectorInfo)                                         { SomeObject  }
	* 0x sizeof(SectorInfo + SomeObject)                            { SomeObject1 }
	* 0x sizeof(SectorInfo + SomeObject + SomeObject1)              { SomeObject2 }
	* ...
	* 0x sizeof(SectorInfo... + ...SomeObjectN - 1 + SomeObjectN)   { SomeObjectN }
	*
	*--------------------------------------------------------------------------------------------
	*/
	struct SectorInfo {
		EntityId id;
		unsigned long nullBits = 0; //each bit means that component 0 1 2 3 etc alive or not, maximum 32 components

		bool isTypeNull(uint8_t typeIdx) const {
			return !((nullBits >> (typeIdx - 1)) & 1);
		}

		void setTypeBitTrue(uint8_t typeIdx) {
			nullBits |= (1 << (typeIdx - 1)); //typeIdx in sector starts after sectorInfo, so move index 1 to left
		}

		void setTypeBitFalse(uint8_t typeIdx) {
			nullBits &= ~(1 << (typeIdx - 1));
		}
	};

	class ComponentsArray : protected ECSMemoryUser {
		ComponentsArray(const ComponentsArray&) = delete;
		ComponentsArray& operator=(ComponentsArray&) = delete;
	public:
		template<typename T>
		class Iterator {
		private:
			using VoidIt = SectorsChunk::Iterator;
			const uint16_t mOffset = 0;
			const uint8_t mTypePos = 0;
			VoidIt mIt;
		public:
			Iterator() {};
			Iterator(const VoidIt& listIt, uint16_t offset, uint8_t typePos) : mOffset(offset), mTypePos(typePos), mIt(listIt) {}

			T* operator*() {
				if (static_cast<SectorInfo*>(*mIt)->isTypeNull(mTypePos)) {
					return nullptr;
				}

				return static_cast<T*>(Utils::getTypePlace(*mIt, mOffset));
			}

			EntityId getSectorId() const {
				return *static_cast<EntityId*>(*mIt);
			}

			Iterator& operator++() {
				++mIt;

				return *this;
			}

			bool operator!=(const Iterator& other) const {
				return mIt != other.mIt;
			}
		};

		ComponentsArray(size_t capacity, ECSMemoryStack* memoryManager);
		~ComponentsArray() override;

		void clearAllSectors();
		size_t size() const;
		bool empty() const;

		template<typename T>
		void moveToSector(EntityId sectorId, T* data) {
			if (!data) {
				assert(false);
				return;
			}

			if (!mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_TYPE_ID, sectorId);
			if (!sector) {
				assert(false);
				return;
			}
			data->mOwnerId = sectorId;
			memmove(sector, data, sizeof(T) + alignof(T));
		}

		template<typename T>
		void copyToSector(EntityId sectorId, T* data) {
			if (!data) {
				assert(false);
				return;
			}

			if (!mChunkData.sectorMembersIndexes.contains(T::STATIC_COMPONENT_TYPE_ID)) {
				assert(false);
				return;
			}

			auto sector = acquireSector(T::STATIC_COMPONENT_TYPE_ID, sectorId);
			if (!sector) {
				assert(false);
				return;
			}
			data->mOwnerId = sectorId;
			memcpy(sector, data, sizeof(T) + alignof(T));
		}

		void* acquireSector(ECSType componentTypeId, EntityId entityId);

		void destroyObject(ECSType componentTypeId, EntityId entityId, bool _erase = true);
		void destroySector(EntityId entityId, bool _erase = true);

		template<typename T>
		T* getComponent(EntityId sectorId) {
			if (sectorId >= mSectorsMap.size()) {
				return nullptr;
			}
			const auto it = mChunk->operator[](mSectorsMap[sectorId]);
			if (!it) {
				return nullptr;
			}

			if (static_cast<SectorInfo*>(it)->isTypeNull(mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID])) {
				return nullptr;
			}

			return static_cast<T*>(Utils::getTypePlace(it, T::STATIC_COMPONENT_TYPE_ID, mChunkData.sectorMembersOffsets, mChunkData.sectorMembersIndexes));
		}

		template <typename T>
		Iterator<T> begin() { return { mChunk->begin(), mChunkData.sectorMembersOffsets[mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID]], mChunkData.sectorMembersIndexes[T::STATIC_COMPONENT_TYPE_ID]}; }
		template<typename T>
		Iterator<T> end() { return { mChunk->end(), 0, 0 }; }

	private:
		void* initSectorMember(void* sectorPtr, ECSType componentTypeId);

		void* createSector(size_t pos, size_t size, EntityId sectorId);
		void erase(size_t pos, size_t size);

		std::vector<EntityId> mSectorsMap;
		SectorsChunk* mChunk = nullptr;

	protected:
		void allocateNewChunk();
		ChunkData mChunkData;
	};

	
	template <typename T>
	struct Base {};

	template <typename... Ts>
	struct TypeSet : Base<Ts>... {
		template<typename T>
		constexpr auto operator+(Base<T>) {
			if constexpr (std::is_base_of_v<Base<T>, TypeSet>) {
				return TypeSet{};
			}
			else {
				return TypeSet<Ts..., T>{};
			}
		}

		constexpr std::size_t size() const {
			return sizeof...(Ts);
		}
	};

	template<typename... Ts>
	constexpr bool areUnique() {
		constexpr auto set = (TypeSet<>{} + ... + Base<Ts>{});
		return set.size() == sizeof...(Ts);
	}

	template <typename... Types>
	class ComponentsArrayInitializer : public ComponentsArray {
		ComponentsArrayInitializer(const ComponentsArrayInitializer&) = delete;
		ComponentsArrayInitializer& operator=(ComponentsArrayInitializer&) = delete;

	public:
		ComponentsArrayInitializer(size_t capacity, ECSMemoryStack* memoryManager) : ComponentsArray(capacity, memoryManager){
			static_assert(areUnique<Types...>(), "Duplicates detected in types");
			static_assert(sizeof...(Types) <= 32, "More then 32 components in one container not allowed");

			size_t idx = 2; //first is 0, 1 is for sector data
			((mChunkData.sectorMembersOffsets[idx++] = mChunkData.sectorSize += static_cast<uint16_t>(sizeof(Types) + alignof(Types))), ...);
			mChunkData.chunkAllocationSize = mChunkData.sectorSize * mChunkData.chunkCapacity;

			uint8_t i = 0;
			((mChunkData.sectorMembersIndexes[Types::STATIC_COMPONENT_TYPE_ID] = ++i), ...);

			allocateNewChunk();
		}
	};
}
