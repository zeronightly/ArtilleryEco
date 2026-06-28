#pragma once
#include "CoreMinimal.h"
#include "BristleconeCommonTypes.h"

class PacketRegistry {
public:
	using FactoryFunction = TFunction<TSharedPtr<TheCone::FBristleconePacketBase>()>;

	PacketRegistry(const PacketRegistry&) = delete;
	PacketRegistry& operator=(const PacketRegistry&) = delete;
	PacketRegistry(PacketRegistry&&) = delete;
	PacketRegistry& operator=(PacketRegistry&&) = delete;

	void RegisterPacket(const FString& InTypeId, FactoryFunction Factory) {
		uint32_t HashId = GetTypeHash(InTypeId);

		if (Registry.Contains(HashId)) {
			return;
		}

		Registry.Add(HashId, Factory);
	}

	TSharedPtr<TheCone::FBristleconePacketBase> CreatePacket(uint32_t HashId) {
		if (FactoryFunction* FactoryFunction = Registry.Find(HashId)) {
			return (*FactoryFunction)();
		}

		return nullptr;
	}

	bool IsPacketRegistered(const FString& InTypeId) {
		uint32_t HashId = GetTypeHash(InTypeId);

		return Registry.Contains(HashId);
	}

	static void Shutdown() {
	}
	PacketRegistry() {}

	~PacketRegistry() {}
private:


	TMap<uint32_t, FactoryFunction> Registry;
};