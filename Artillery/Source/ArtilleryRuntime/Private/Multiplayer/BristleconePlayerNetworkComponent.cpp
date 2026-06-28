// NOTE: in the patch this file was misfiled under Public/. Moved to Private/ here per Unreal convention.
#include "Multiplayer/BristleconePlayerNetworkComponent.h"

#include "PacketRegistry.h"

void FPlayerNetworkComponent::SetPacketRegistry(PacketRegistry* InRegistry)
{
    
}

bool FPlayerNetworkComponent::Initialize(bool bLogReceive)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get socket subsystem"));
        return false;
    }

    local_endpoint = FIPv4Endpoint(FIPv4Address::Any, ReceivePort);
    //TODO! ADD HANDLING FOR BINDING THE SLOT!!!!!!!!!!!!!
    
    // FUdpSocketBuilder socket_factory = FUdpSocketBuilder(TEXT("Bristlecone.Receiver.Socket"))
    //                                    .AsNonBlocking()
    //                                    .AsReusable()
    //                                    .BoundToEndpoint(local_endpoint)
    //                                    .WithReceiveBufferSize(sizeof(FBristleconePacketBase) * 25)
    //                                    .WithSendBufferSize(sizeof(FBristleconePacketBase) * 25);
    // SocketHigh = MakeShareable(socket_factory.Build());
    // SocketLow = MakeShareable(socket_factory.Build());
    // SocketBackground = MakeShareable(socket_factory.Build());
    // if (!SocketHigh || !SocketLow || !SocketBackground)
    // {
    //     UE_LOG(LogTemp, Error, TEXT("Failed to create one or more sockets for Network %s"),
    //         *ConnectionOwner.ToString());
    //     return false;
    // }
    //
    // // Set up sender
    // SenderRunner.BindSource(QueueToSend);
    // SenderRunner.AddTargetAddress(RemoteAddress, SendPort);
    // SenderRunner.SetLocalSockets(SocketHigh, SocketLow, SocketBackground);
    // SenderRunner.SetWakeSender(WakeSender);
    //
    // // Set up receiver
    // ReceiverRunner.BindSink(QueueOfReceived);
    // ReceiveTimes = MakeShareable(new TheCone::TimestampQ(140));
    // ReceiverRunner.BindStatsSink(ReceiveTimes);
    // ReceiverRunner.SetLocalSocket(SocketHigh);
    // ReceiverRunner.LogOnReceive = bLogReceive;
    //
    // // Start threads
    // SenderThread.Reset(FRunnableThread::Create(&SenderRunner, TEXT("Bristlecone.Sender")));
    //
    // ReceiverThread.Reset(FRunnableThread::Create(&ReceiverRunner, TEXT("Bristlecone.Receiver")));
    //
    // bool Success = (SenderThread != nullptr && ReceiverThread != nullptr);
    //
    // if (Success)
    // {
    //     UE_LOG(LogTemp, Log, TEXT("Successfully initialized network %s"),
    //         *ConnectionOwner.PrettyPrint());
    // }
    // else
    // {
    //     UE_LOG(LogTemp, Error, TEXT("Failed to create threads for network %s"), *ConnectionOwner.PrettyPrint());
    //
    //     Shutdown();
    // }

    return true;
}

void FPlayerNetworkComponent::Shutdown()
{

    UE_LOG(LogTemp, Verbose, TEXT("Network %s shut down"), *ConnectionOwner.PrettyPrint());
}
