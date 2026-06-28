// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Kines.h"
#include "ORDIN.h"
#include "SkeletonTypes.h"
#include "SwarmKine.h"
#include "TransformDispatch.h"
#include "Engine/Engine.h"
#include "seq/concurrent_map.hpp"
#include "Subsystems/WorldSubsystem.h"

THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
typedef seq::concurrent_map<FSkeletonKey, FName> NameLookup;
typedef seq::concurrent_map<FSkeletonKey, TSharedPtr<FString>> TextLookup;
typedef seq::concurrent_set<FSkeletonKey> DeferredKeySet;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

#include "KeyToTextDispatch.generated.h"
/* Provides a threadsafe and typesafe way to store text, fnames, and even ink files against any given key.
 * NOTE: this is not deterministic UNLESS you manage insertion\read event sequencing, use the request router, or bulk load at start.
 * As we use the request router for adds, that's the approach I would suggest.
 * 
 * 
 */
UCLASS()
class SKELETONKEY_API UKeyToTextDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()
	FSkeletonKey DefaultObjectKey;
	NameLookup Names;
	TextLookup Blocks;
	//the non-prime deferred set gets deleted.
	//bool IsFrontPrime = true;
	//DeferredKeySet OverkillBack; //we can afford the time cost. this isn't necessary to make concurrent, due to how we handle it but I can't guarantee no one will make Decisions.	
	
	DeferredKeySet OverkillFront; //this lets us be pretty stupid, so while the machinery above would let you pretty easily do a doublebuffer for tombstoning, I'm just... gonna not.
	UTransformDispatch* TransformDispatch;
	//besides, I'd recommend a triple buffer for your sanity.
	
	//TextLookup InkBlob; // we'll probably not want to use a std string for something as big as an inkblob. likely a mmap. but this is a placeholder for now.
	
public:
	UKeyToTextDispatch();
	virtual ~UKeyToTextDispatch() override;


	static UKeyToTextDispatch* Get(UWorld& World)
	{
		return World.GetSubsystem<UKeyToTextDispatch>();
	}
	
	static UKeyToTextDispatch* Get(UObject* WorldContextObject) 
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if (ensure(World)) 
		{
			return UKeyToTextDispatch::Get(*World);
		}

		return nullptr;
	}
	
	//A natural question is: why not make these attributes?
	//We find that these concurrent maps get modified or accessed in heavy bursts, and separating them out allows us to 
	//both better manage this load and better customize the memory management for the unique and pernicious requirements of strings.
	//We also generally don't have a concept at this point of "updates" to a string, and I don't foresee adding one in the near-term,
	//and I almost certainly wouldn't add it here. This is for immutables like object description blocks, names, and inkblobs. For
	//dynamic tooltips or dynamic descriptions, you can use an inkblob, embed the format specifiers in the immutable, or use another
	//mechanism that's better suited elsewhere in the ecosystem. Separating out these sorts of runtime defined but write-once literals
	//allows us both speed and consistency.	It also makes it far easier to make this data accessible during debug or crash scenarios,
	//either with autoserialize, natviz, or similar. That'd be much less safe if we were using mutable attributes that needed
	//to be concurrent write safe and total ordered.
	constexpr static int OrdinateSeqKey = ORDIN::TextNames;
	//threadsafe but not natively ordered. see RequestRouter
	bool RegisterFName(FSkeletonKey Target, FName Name);
	void DeferDelete(FSkeletonKey Target);
	void ProcessDeletes();
	//threadsafe but not natively ordered. see RequestRouter
	const FName GetFName(FSkeletonKey Target) const;
	//threadsafe but not natively ordered. see RequestRouter
	bool RegisterConstBlock(FSkeletonKey Target, const TSharedPtr<FString> Block);
	//threadsafe but not natively ordered. see RequestRouter
	const TSharedPtr<FString> GetConstBlock(FSkeletonKey Target) const;
	

	//BEGIN OVERRIDES
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	virtual bool RegistrationImplementation() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void PostInitialize() override;
	virtual void PostLoad() override;
	virtual void Tick(float DeltaTime) override;
	
private:
	//used by process deletes.
	void DeregisterKey(FSkeletonKey Target);
};
