#pragma once
#include "ArtilleryCommonTypes.h"
#include "BristleconeCommonTypes.h"
#include "FBristleconeReceiver.h"
#include "FBristleconeSender.h"
#include "PacketRegistry.h"
#include "UCablingWorldSubsystem.h"


//this binds a world object with a skeletonkey to a given player with the player key. It's part of how we connect the player to the world, since the player
//persists far beyond world or even engine.
class FPlayerNetworkComponent
{
public:
	void SetOwner(const FSkeletonKey& InOwner, const PlayerKey Player)
	{
		ConnectionOwner = InOwner;
		MyPlayer = Player;
	};

	void SetDebugNetMode(ENetRole InNetMode)
	{
		DebugNetMode = InNetMode;
	};

	ENetRole DebugNetMode;
    TheCone::TimestampQueue ReceiveTimes;

    // Sockets use different ports for each player
    int32 SendPort;
    int32 ReceivePort;
	//players may not share the same broker in production. I hope we don't have to support that, but better to build the way to do it now.
    FString RemoteAddress;
    FSkeletonKey ConnectionOwner;
	PacketRegistry* MyRegistry;
	PlayerKey MyPlayer;

    // Queues for data
    TSharedPtr<TCircularQueue<TSharedPtr<TheCone::FControllerStatePacket>>> QueueToSend;
    TheCone::RecvQueue QueueOfReceived;

    // Runtime components

	void SetPacketRegistry(PacketRegistry* InRegistry);

    // Constructor
    FPlayerNetworkComponent()
    {
        QueueToSend = MakeShareable(new TCircularQueue<TSharedPtr<TheCone::FControllerStatePacket>>(256));
        QueueOfReceived = MakeShareable(new TheCone::PacketQ(256));
    }

    bool Initialize(bool bLogReceive);

    /**
     * Shut down networking components
     */
    void Shutdown();

    /**
     * Trigger the send event to wake the sender thread
     */
    void TriggerSend();

    FIPv4Endpoint local_endpoint;
};
