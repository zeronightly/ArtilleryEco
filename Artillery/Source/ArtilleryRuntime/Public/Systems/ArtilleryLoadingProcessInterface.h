#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "ArtilleryLoadingProcessInterface.generated.h"

UINTERFACE(BlueprintType)
class UArtilleryLoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

class IArtilleryLoadingProcessInterface
{
	GENERATED_BODY()

public:
	// Checks to see if this object implements the interface, and if so asks whether or not we should
	// be currently starting simulation
	static bool ShouldStartArtillerySimulation(UObject* TestObject, FString& OutReason)
	{
		if (TestObject != nullptr)
		{
			if (IArtilleryLoadingProcessInterface* LoadObserver = Cast<IArtilleryLoadingProcessInterface>(TestObject))
			{
				FString ObserverReason;
				if (LoadObserver->ShouldStartArtillerySimulation(/*out*/ ObserverReason))
				{
					return true;
				}

				if (ensureMsgf(!ObserverReason.IsEmpty(), TEXT("%s failed to set a reason why it wants to delay simulation"), *GetPathNameSafe(TestObject)))
				{
					OutReason = ObserverReason;
				}
			}
		}

		return false;
	};

	virtual bool ShouldStartArtillerySimulation(FString& OutReason) const
	{
		return true;
	}
};
