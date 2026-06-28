// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArtilleryDispatch.h"
#include "CloverEssentialTypes.h"
#include "GenericQuadTree.h"
#include "KeyedConcept.h"
#include "FBarrageKey.h"
#include "seq/SeqU64Prefix.hpp"

#include "BarrageDispatch.h"
#include "FlattenedBodyBox.h"
#include "ORDIN.h"
#include "Subsystems/WorldSubsystem.h"

#include "CloverDispatch.generated.h"

/**
 * Hi! If you're looking at clover, you're probably (hopefully) trying to either do fast queries that allow you to compose behaviors
 * or looking to do, you know, inventory. The secret is that we don't really see a difference between those two things. This is a 
 * design philosophy inherited from Destiny 2, where it proved pretty essential in, ya know, actually making the game. And that did
 * make about a billion dollars and live for a long time. I don't want to appeal to authority though. So let's talk about what this
 * does and you can make your own call. First, what it's not:
 * 
 * This is not a complete inventory system. I'm not sure a fully general one is possible.
 * It's not something we wanted to include in the core of artillery, because it is a little out of scope.
 * Finally, this is headless - there's no UI included in clover. Eventually, a lot of it will appear in sunflower!
 * 
 * What it is:
 * A set of powerful and expressive primitives for describing any inventory system.
 * Enough actual meat that building most inventory systems with it will be pretty simple.
 * A basic Model Query Binding design where the model is an impure ECS, that is to say, Artillery
 * The query is set intersections. sets are entities themself, and referenced by key.
 * Sets have a subtype marker, unlike most other keys in our system.
 * So a vendor is a set, an inventory is a set, the enemies that can spawn in a level would be a set.
 * Sets have three kinds of members: 
 *		1) regular keys like you've seen everywhere else! These are used as behavior binders! 
 *		for example, you can get a state tree this way! These tend to be things you want to be able to reference quickly.
 *		And aren't always what the set actually stores. When they are, they just go in the set. When they're properties of a set, they go in...
 *		2) Sockets! Sockets hold keys. They allow one-to-many relationships, which is something we intentionally do not support seriously with our relationship keys.
 *		Where relationship keys should be thought of a noun-verb-noun triples, or facts, sockets cover all the bullshit that comes up in games all the time.
 *		like when some fucker puts on three rings instead of two, because someone made a hand of glory item? well, the hand of glory item just adds a third ring socket.
 *		That's not something that describes well with the triples. you can do it, but it's awkward, and as someone with a decade of experience using graph DBs:
 *		paradigm purity can fuck itself.
 *		3) Item Instance keys! These are special keys that contain a reference to their item archetype uniquely in their actual key layout!
 *		In simple systems, they may also contain a set reference! Sockets actually do, and it's very useful. This isn't a great fit for everything,
 *		so we don't enforce or assume it. (It also makes almost the whole key just the set and instance. This is Not Great for technical reasons.)
 *		Instance keys and variations on this hierarchical key concept form the backbone of what allows clover to answer certain kinds of queries in
 *		literally constant time. We don't use lookup tables for a lot of relationships. They're just encoded in the key.
 *		
 *		Here are the actual bits you'll see built out:
 *		Vendor Sets
 *		Inventory Sets
 *		Results Sets
 *		
 *		Item Archetype Keys
 *		Item Instance Keys
 *		Attribute-keys-as-currency
 *		
 *		Sockets. So many sockets:
 *		In destiny 2, there was a privileged kind of item that specifically went into sockets called plugs. 
 *		We uh... just let you put whatever in there. So you might see _a lot_ of sockets show up in your design.
 *		You'll likely use those or ConservedAttributeKeys to connect guns (Abilities) to things.
 *		
 *		
 *		Why these? Well, I built a game on this design already, so I like it. In Destiny, most things were vendors.
 *		Seriously. Most things were just vendors. Perks? Sockets and plugs. 
 *		Xur, obviously a vendor. Bounties? Vendors. Lootable crates? Often vendors! 
 *		Your seasonal artifact? vendor that sold plugs for its own sockets.
 *		
 *		Look, man. I know it seems oversimplified. But it's actually a pretty normal design.
 *		
 *		
 * 
 */




UCLASS()
class ARTILLERYRUNTIME_API UCloverDispatch : public UTickableWorldSubsystem, public ISkeletonLord, public ITickHeavy
{
	GENERATED_BODY()
    //TPair<FSkeletonKey, FVector2d> worked fine...
	friend class UArtilleryDispatch;
public:
	constexpr static uint32 CLOVER_MAPPINGCADENCE = 32;
	enum SpecialPlugKeySlices : uint64
	{
		InventorySetMarkers = SFIX_InventoryOf,
		
		MainInventory = 0xFFAABB,// FAB. IT IS FAB.
		Quests		  = 0x111111, //best if this one is really identifiable in binary, hex, and decimal.
		Conditions	  = 0x51c51c
	};

	using OwnedSets = FCloverResultSet;//GOT THAT OUROBOROS IN ME, OUROBOROS
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	uint64 GetSubtypedPrefix(SKELLY type, uint32 Notched28BitKey) // if you didn't notch it, it will _GET_ notched
	{
		uint64 ret = (type >> 28) | ((Notched28BitKey & SKELLY::SFIX_NotchKeyForMetaUse) << 4);
		return ret;
	}
	
	uint64 BuildKnownKey(SKELLY type, uint32 Notched28BitKey, SpecialPlugKeySlices KnownSlice) // if you didn't notch it, it will _GET_ notched
{
		uint64 ret = GetSubtypedPrefix(type, Notched28BitKey);
		ret = (ret << 28) | KnownSlice;
		return ret;
}
	
	
	UArtilleryDispatch* ArtilleryDispatch;
	bool GetSetKeysByOwner(FSkeletonKey Owner, OwnedSets& OutParamOptionallyLiveMemory);
	//Result sets are not concurrent datatypes. If you must use them across threads, copy them.
	//result sets do not have their own keys, they are a transient primitive unique to Clover.
	
	
	//CoreFunction
	bool TrackGameplayTagSet_NotRetroactive(FGameplayTag Tag);
	//TArray<FCloverSetKey> FilterBySubtype(ESetSubtype Subtype, FCloverSetKey ToFilter); //inventory, vendor, loot table, etc
	FCloverResultSet IntersectSets(FCloverResultSet smaller, FCloverResultSet larger);
	FCloverResultSet IntersectSetsByKey(FCloverSetKey A, FCloverSetKey B);
	bool GetPlugsForSocketKey(FSKSocketKey ASocket, OwnedSets& OutParamOptionallyLiveMemory);
	FSkeletonKey AddToSet(FSkeletonKey Place, FCloverSetKey Into);
	
	//Second-order functions
	FCloverResultSet GetPlayerInventories(PlayerKey TargetPlayer);
	FCloverResultSet GetNPInventories(FSkeletonKey Target);
	std::optional<FCloverSetKey> GetMainPlayerInventory(PlayerKey A);
	std::optional<FCloverSetKey> GetPlayerQuests(PlayerKey TargetPlayer);
	std::optional<FCloverSetKey> GetPlayerConditions(PlayerKey TargetPlayer);
	std::optional<FCloverSetKey> GetNPConditions(FSkeletonKey Target);
	FName GetHumanReadable(FSKPlugKey A); //generally for debug
	FCloverResultSet GetSubsetByTag(FGameplayTag Tag, FCloverResultSet B);
	seq::radix_set<unsigned long long>::const_iterator GetInventoriesOfKey(FSkeletonKey Owner); // privileged for sanity reasons. i'm not a monster.
	TArray<FCloverSetKey> GetKeysFromResultSet(FCloverResultSet ToUnpack);
	
	
	//pairwise
	TPair<FSKSocketKey, FSkeletonKey> PlugToSocket(FSkeletonKey A, FSKSocketKey Into, FSkeletonKey SocketOwnedBy); 
	TPair<FSKSocketKey, FSkeletonKey> PlugToSocket(FSkeletonKey A, FSKSocketKey Into, PlayerKey SocketOwnedBy);
	//Bind Operators
	FSKItemKey AddItemInstanceToSet(FSKItemKey Place, FCloverSetKey Into);
	FCloverSetKey GiveShiny(FSkeletonKey A, FSKItemArchetypeKey B);
	FSKItemKey InstanceItem(FSkeletonKey OptionalOwner, FSKItemArchetypeKey A);
	FCloverSetKey GiveSpecificShiny(FSkeletonKey A, FSKItemKey B);
	FSKSocketKey GrantSocketToSet(FCloverSetKey Set);
	FCloverSetKey GrantSetToKey (FSkeletonKey Owner);
	
	//Socket ops
	FCloverResultSet GetAllEntitiesWithinDistanceOfPoint(FVector2d& Point, double Radius);
	//Man I dunno about this. I'm gonna implement this if we need it.
	FSKPlugKey BindAbilityToSocket(FGunKey Instance, FSKSocketKey BindTo);
	//This is provided without warranty for people who don't wanna use guns.
	FSKPlugKey BindArbitraryKeyToSocket(FSkeletonKey NoWarranty, FSKSocketKey BindingTo); 
	
	
	class TagHasher
	{
	public:
		SEQ_STR_INLINE_STRONG auto operator()(const FGameplayTag A) const noexcept -> size_t
		{
			return MashFunctions::FastHash32( A.GetTagName().GetNumber());
		}
	};
	seq::concurrent_map<FSkeletonKey, OwnedSets> SetsByOwner;//we don't need to go the other way around.
	seq::concurrent_map<PlayerKey, OwnedSets> SetsByPlayer; //speed and sanity demands we separate these out.
	//TODO: expose properly for use!!!
	seq::concurrent_map<FBarrageKey, JPH::BodyBoxFlatCopy> FrozenPhysicsState;
protected:
	//tracked tags only.
	seq::concurrent_map<FGameplayTag, FCloverResultSet, TagHasher> SetsByTag;
	TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> QuadTreeForDistance = {};
	bool QuadTreeMaintenance = true;
	bool PhysicsStateMaintence = true;
	UBarrageDispatch* ContingentPhysicsLinkage = nullptr;
	JPH::BodyIDVector MyTrackingSet = {};
	TArray<JPH::BodyBoxFlatCopy> MyShadowCopies = {};
public:
	virtual void ArtilleryTick(uint64_t TicksSoFar) override;
	

private:
	constexpr static int OrdinateSeqKey = ORDIN::E_D_C::InventorySystem;
	virtual bool RegistrationImplementation() override; 
};
