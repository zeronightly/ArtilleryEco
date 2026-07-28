#pragma once
#include "FArtilleryGun.h"
#include "SkeletonTypes.h"
#include "Templates/TypeHash.h"
THIRD_PARTY_INCLUDES_START
#include "seq/ordered_map.hpp"
#include "seq/concurrent_map.hpp"
#include "seq/radix_hash_map.hpp"
#include "seq/SeqU64Prefix.hpp"
#include "seq/flat_map.hpp"
#include "Structures/ApproximateMembership/FLargeGate.h"
THIRD_PARTY_INCLUDES_END
#include "InventoryEssentialTypes.generated.h"
#define Inventory_VERIFIEDFRAMETESTMODE true
//hi! Using a radix trie here allows us to search by partial prefix! this lets us pull just plugs, or just sockets, very very
//very fast! it also allows us to search elegantly over the _meta_ in the keys themselves, which we've QUITE CAREFULLY
//set up to present a structured hierarchy. this lets you access it VERY VERY FAST by just saying [type][parent_key]
struct SEQGetSKKey
{
	uint64 operator()(const FSkeletonKey & p) const 
	{
		return p.Obj;
	}
};
using FCRKeyStruct = SeqSet64;
struct ARTILLERYRUNTIME_API FInventorySetKey
{
	constexpr static auto MasterKeyType = SFIX_SetOf;
	uint64_t MyKey = 0;
	FInventorySetKey(uint32 GetsSlicedTo28Bits, uint64_t ParentKey, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKey, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	//invalid
	FInventorySetKey()
	{
		MyKey = 0;
	}

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
	
	// in the core skeleton key class, we've had to really fiddle around to prevent some unwanted conversions that would
	// have been avoided if we'd just rolled a couple functions instead of using op overriding, casts, and constructors.
	static FInventorySetKey FromSK(FSkeletonKey From)
	{
		FInventorySetKey retval;
		(retval.MyKey=From);
		return retval;
	}
};

//item instances use the subtype nibble for a set of four bitflags.
//we only use two, leaving two to any other users.
struct ARTILLERYRUNTIME_API FSKItemInstance
{
	constexpr static auto MasterKeyType = SFIX_ItemInstance;
	uint64_t MyKey = 0;
	
    bool IsStackable()
    {
	    return MyKey & SFIX_ItemInstanceStackableMask;
    }
	
	bool IsPersistent()
    {
    	return MyKey & SFIX_ItemInstancePersistenceMask;
    }
	
	FSKItemInstance(uint32 GetsSlicedTo28Bits, uint64_t ParentKeyOfItemInstance, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKeyOfItemInstance, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	FSKItemInstance() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};

struct ARTILLERYRUNTIME_API FSKItemArchetypeKey
{
	constexpr static auto MasterKeyType = SFIX_ItemArchetype;
	uint64_t MyKey = 0;
	FSKItemArchetypeKey(uint32 GetsSlicedTo28Bits, SFIX_SubtypeSelector MySubtype, uint64_t ParentKey = 0)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKey, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	FSKItemArchetypeKey() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};


struct ARTILLERYRUNTIME_API FSKPlugKey
{
	constexpr static auto MasterKeyType = SFIX_Socket;
	uint64_t MyKey = 0;
	FSKPlugKey(FSKItemInstance GetsSlicedTo28Bits, SFIX_SubtypeSelector MySubtype, FInventorySetKey ParentSet)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentSet.MyKey, GetsSlicedTo28Bits.MyKey, MasterKeyType),
			MySubtype);
		
	}

	FSKPlugKey() = default;

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
};

struct ARTILLERYRUNTIME_API FSKSocketKey
{
	constexpr static auto MasterKeyType = SFIX_Socket;
	uint64_t MyKey = 0;
	FSKSocketKey(uint32 GetsSlicedTo28Bits, FInventorySetKey ParentSet)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentSet.MyKey, GetsSlicedTo28Bits, MasterKeyType),
			static_cast<SFIX_SubtypeSelector>(MasterKeyType >> 32));
		
	}
	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}

	FSKSocketKey() = default;
};

using FSKItemKey = FSKItemInstance;


struct ARTILLERYRUNTIME_API FResultSetInterstitial;
struct ARTILLERYRUNTIME_API FInventoryResultSet
{
	FLargeGate Bloomlike;
	FCRKeyStruct UnderlyingKeys;
	FInventorySetKey SetRepresentedIfAny;
	FSkeletonKey OwnerIfAny;// this makes a ton of stuff a million times less awkward.
	bool Dirtied_AllowsFalseFalses = false; //not everything that dirties a set touches this, and not everything should.
	FInventoryResultSet() : Bloomlike(), SetRepresentedIfAny()
	{
	}

	//copy
	FInventoryResultSet(const FInventoryResultSet& Other)
		: Bloomlike(Other.Bloomlike),
		  UnderlyingKeys(Other.UnderlyingKeys),
		  SetRepresentedIfAny(Other.SetRepresentedIfAny),
		  OwnerIfAny(Other.OwnerIfAny)
	{
	}

	//move
	FInventoryResultSet(FInventoryResultSet&& Other) noexcept
		: Bloomlike(std::move(Other.Bloomlike)),
		  UnderlyingKeys(std::move(Other.UnderlyingKeys)),
		  SetRepresentedIfAny(std::move(Other.SetRepresentedIfAny)),
		  OwnerIfAny(std::move(Other.OwnerIfAny))
	{
	}

	FInventoryResultSet(FInventorySetKey MyKey) : Bloomlike(), SetRepresentedIfAny(MyKey)
	{
	}

	explicit FInventoryResultSet(const FCRKeyStruct::iterator& Prefix)
	{
		UnderlyingKeys =  FCRKeyStruct(Prefix, FCRKeyStruct::iterator());
		for (auto i : UnderlyingKeys)
		{
			Bloomlike.Add(i);
		}
	}

	FInventoryResultSet& operator=(const FInventoryResultSet& Other)
	{
		if (this == &Other)
			return *this;
		Bloomlike = Other.Bloomlike;
		UnderlyingKeys = Other.UnderlyingKeys;
		SetRepresentedIfAny = Other.SetRepresentedIfAny;
		OwnerIfAny = Other.OwnerIfAny;
		return *this;
	}

	FInventoryResultSet& operator=(FInventoryResultSet&& Other) noexcept
	{
		if (this == &Other)
			return *this;
		Bloomlike = std::move(Other.Bloomlike);
		UnderlyingKeys = std::move(Other.UnderlyingKeys);
		SetRepresentedIfAny = std::move(Other.SetRepresentedIfAny);
		OwnerIfAny = std::move(Other.OwnerIfAny);
		return *this;
	}

	FResultSetInterstitial IntersectSets(FResultSetInterstitial& rhs);
	FResultSetInterstitial IntersectSets(FInventoryResultSet& rhs);
	bool Find(FSkeletonKey CheckPresenceOf);
	void Rebuild();
};


struct FResultSetInterstitial
{
	FResultSetInterstitial(FInventoryResultSet& Contained)
		: Contained(Contained)
	{
	}
	
	FResultSetInterstitial(FInventoryResultSet&& Contained)
	: Contained(Contained)
	{
	}

	FResultSetInterstitial(const FResultSetInterstitial& Other)
		: Contained(Other.Contained)
	{
	}

	FResultSetInterstitial(FResultSetInterstitial&& Other) noexcept
		: Contained(Other.Contained)
	{
	}

	FResultSetInterstitial& operator=(const FResultSetInterstitial& Other)
	{
		if (this == &Other)
			return *this;
		Contained = Other.Contained;
		return *this;
	}

	FResultSetInterstitial& operator=(FResultSetInterstitial&& Other) noexcept
	{
		if (this == &Other)
			return *this;
		Contained = Other.Contained;
		return *this;
	}

	explicit FResultSetInterstitial(FResultSetInterstitial* Result);

	FInventoryResultSet Finalize()
	{
		for (auto k : Contained.UnderlyingKeys)
		{
			if (Contained.Bloomlike.ApproxFind(k))
			{
				continue; //False positives aren't possible because we start with the origin set
			}
			else // included for clarity.
			{
				Contained.UnderlyingKeys.erase(k); //false negatives are prevented
			}
		}
		return FInventoryResultSet(std::move(Contained));
	}
	
	FResultSetInterstitial* IntersectSets(FInventoryResultSet& larger)
	{
		Contained.Bloomlike.IntersectInPlace(larger.Bloomlike);
		return this;
	}
	
	FResultSetInterstitial* IntersectSets(FResultSetInterstitial& larger)
	{
		Contained.Bloomlike.IntersectInPlace(larger.Contained.Bloomlike);
		return this;
	}
	
private:
	FInventoryResultSet& Contained;
	
	
	using  FCSResults = FInventoryResultSet;
};

inline FResultSetInterstitial FInventoryResultSet::IntersectSets(FResultSetInterstitial& rhs)
{
	FResultSetInterstitial Result(  std::move(FInventoryResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline FResultSetInterstitial FInventoryResultSet::IntersectSets(FInventoryResultSet& rhs)
{
	FResultSetInterstitial Result(  std::move(FInventoryResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline bool FInventoryResultSet::Find(FSkeletonKey CheckPresenceOf)
{
	if (Bloomlike.ApproxFind(CheckPresenceOf))
	{
		return UnderlyingKeys.contains(CheckPresenceOf);
	}
	return false;
}

inline void FInventoryResultSet::Rebuild()
{
}


//suitable for slow polled data presentation that refreshes rarely.
struct ARTILLERYRUNTIME_API FInventoryData : public FInventoryResultSet
{
};

//"push model" for Inventory data, emits listenable blueprint events for data change at a configurable K change interval
struct ARTILLERYRUNTIME_API FEventedInventoryData : public FInventoryData
{
};



	enum WhatMattersToThis //I don't know that we'll really use these as bitflags, but there are damn good reasons you might.
	{
		Player = 						0b1,
		Enemy = 						0b10,
		AnyMob =						0b11,
		MorePlayersThanEnemies = 		0b100,
		WeightedMorePlayers	 =			0b1000, //player weight 2 
		MorePlayersThanPeerEnemies = 	0b10000, //Chaff doesn't count
		SpecificKey = 					0b100000,	//Only some specific key in the world
		SpecificKeyInPlayerInventory = 	0b1000000, //quest item support
		SpecificTag = 					0b10000000,
		InteractNeeded = 				0b100000000
	};
	
	
	//these are used in conjunction with the trigger guns to fire for basic stuff. I'll make five or six simple trigger guns.
	//anything more complex than this is gonna need a state tree if we don't want to quickly go insane writing hundreds of bespoke conditional state machines.
	//this also covers about 80% of triggers, and if you added the "dies in area" trigger, that combined with our existing OnDeath hooks would cover 95%.
	struct  FTriggerInstance
	{
		FTriggerInstance(const FSKItemInstance& MyKey, const FGunKey& MyGunIfAny, const TOptional<FGameplayTag>& AddToInventoryEntitlements, const TOptional<FGameplayTag>& TagNeededIfAny,
			const FSkeletonKey& KeyNeededIfAny, const FBoneKey& MyStaticMeshIfAny, const FBarrageKey& MyTransformLinkIfAny, float Radius, int EntitiesInRadiusToPrime, int TimesTriggeredToSetOff,
			int TimesTriggered, int LastTickPrimed, int MaxAllowedDelayBetweenPrimedFrames, int TimesAllowedToTrigger, WhatMattersToThis CategoryChecked, const FVector& MyStartingLocation)
			: MyGun(MyGunIfAny), MyKey(MyKey),
			  AddToInventoryEntitlements(AddToInventoryEntitlements),
			  TagNeededIfAny(TagNeededIfAny),
			  KeyNeededIfAny(KeyNeededIfAny),
			  MyStaticMeshIfAny(MyStaticMeshIfAny),
			  MyTransformLinkIfAny(MyTransformLinkIfAny),
			  radius(Radius),
			  EntitiesInRadiusToPrime(EntitiesInRadiusToPrime),
			  TimesTriggeredToSetOff(TimesTriggeredToSetOff),
			  TimesTriggered(TimesTriggered),
			  LastTickPrimed(LastTickPrimed),
			  MaxAllowedDelayBetweenPrimedFrames(MaxAllowedDelayBetweenPrimedFrames),
			  TimesAllowedToTrigger(TimesAllowedToTrigger),
			  CategoryChecked(CategoryChecked),
			  MyStartingLocation(MyStartingLocation)
		{
		}

		FTriggerInstance() : CategoryChecked()
		{
		};
		FGunKey MyGun;
		FSKItemInstance MyKey; // you can extract the archetype from this!
		//the following two or three fields should probably be excised by creating guns that actually follow the logic.
		TOptional<FGameplayTag> AddToInventoryEntitlements; //If set, all entities in the radius that matter to this trigger will get this tag added to them as an entitlement when it goes off
		TOptional<FGameplayTag> TagNeededIfAny;
		FSkeletonKey KeyNeededIfAny;
		FBoneKey MyStaticMeshIfAny;
		//this is literally smaller than an optional. *sigh*
		//if set, this trigger will act like an aura around that transform's center. it does not perform a minkowsky sum. Just measures from the center. Crudely.
		FBarrageKey MyTransformLinkIfAny;
		FBox2D BoxIfInQuadTrie;
		float radius = 100;		//sane initial value
		int EntitiesInRadiusToPrime = 1;
		int TimesTriggeredToSetOff = 1;
		int TimesTriggered = 0;
		int LastTickPrimed = -1;
		int MaxAllowedDelayBetweenPrimedFrames = -1;//this just uses the triggered count as a smoothing or dejittering tool for "charging" triggers. add an allowed delay if you want the trigger to require continuous presence.
		int TimesAllowedToTrigger = 1; //if you need more than this, you are a somewhat bad person.
		WhatMattersToThis CategoryChecked;
		FVector MyStartingLocation = FVector::ZeroVector;

		bool CloseTrigger()
		{
			return true;
		}
	};



USTRUCT()
	struct FSimpleTriggerGun : public FArtilleryGun
	{
		
		GENERATED_BODY()
		FTriggerInstance MyTriggerInstance;
		UInventoryDispatch* MyInventoryDispatch;
	public:

		//This is the function to override to add triggering check logic. PrefireGun now contains the logic for the verified frame check.
		//triggers only go off on verified frames. Many many many things will work this way. 
		virtual bool Precheck()
		{
			return true;
		}
		
		virtual void PreFireGun(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		                        const EventBufferInfo FireAction = EventBufferInfo::Default(), const FGameplayEventData* TriggerEventData = nullptr, bool RerunDueToReconcile = false,
		                        int DallyFramesToOmit = 0, bool VerifiedFrame = false) override
		{
			if (VerifiedFrame || Inventory_VERIFIEDFRAMETESTMODE)
			{
				if (Precheck()){
					FireGun(Fired, 0, ActorInfo, ActivationInfo, false, TriggerEventData, Handle);
				}
			}
		}
		
		virtual void FireGun(FArtilleryStates OutcomeStates, int DallyFramesToOmit, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
							 bool RerunDueToReconcile, const FGameplayEventData* TriggerEventData, FGameplayAbilitySpecHandle Handle) override
		{
			FArtilleryGun::PostFireGun(OutcomeStates, DallyFramesToOmit, ActorInfo, ActivationInfo, RerunDueToReconcile, TriggerEventData, Handle);
		}

		friend uint32 GetTypeHash(const FSimpleTriggerGun& Arg)
		{
			return Arg.MyGunKey.GunInstanceID; //these are valid hashes, interestingly.
		}
		
		//unlike almost all types, for us, equality is the same as comparing hashes, because
		//our hash is our key and it's precomputed with a known min\non-collisive trick _somewhere_
		//in this case, the upper 32 bits are deterministic information necessary for not hating yourself forever
		//and the lower 32 bits are an instance hash. 
		virtual bool operator==(const FSimpleTriggerGun& other) const {
			return MyGunKey == other.MyGunKey; 
		}
	};



// Add specialization to the std namespace
namespace std {
	template <>
	struct hash<FSimpleTriggerGun> {
		std::size_t operator()(const FSimpleTriggerGun& u) const noexcept {
			return u.MyGunKey.GunInstanceID;
		}
	};
}