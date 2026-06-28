#pragma once
#include "SkeletonTypes.h"
THIRD_PARTY_INCLUDES_START
#include "seq/ordered_map.hpp"
#include "seq/concurrent_map.hpp"
#include "seq/radix_hash_map.hpp"
#include "seq/SeqU64Prefix.hpp"
#include "seq/flat_map.hpp"
#include "Structures/ApproximateMembership/FLargeGate.h"
THIRD_PARTY_INCLUDES_END

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
struct ARTILLERYRUNTIME_API FCloverSetKey
{
	constexpr static auto MasterKeyType = SFIX_SetOf;
	uint64_t MyKey = 0;
	FCloverSetKey(uint32 GetsSlicedTo28Bits, uint64_t ParentKey, SFIX_SubtypeSelector MySubtype)
	{
		MyKey = SFIX_DestructiveApplySubtype(
			SFIX_ImprintKeyDependency(ParentKey, GetsSlicedTo28Bits, MasterKeyType),
			MySubtype);
		
	}

	//invalid
	FCloverSetKey()
	{
		MyKey = 0;
	}

	FSkeletonKey GetSK()
	{
		return FSkeletonKey(MyKey);
	}
	
	// in the core skeleton key class, we've had to really fiddle around to prevent some unwanted conversions that would
	// have been avoided if we'd just rolled a couple functions instead of using op overriding, casts, and constructors.
	static FCloverSetKey FromSK(FSkeletonKey From)
	{
		FCloverSetKey retval;
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
	FSKPlugKey(FSKItemInstance GetsSlicedTo28Bits, SFIX_SubtypeSelector MySubtype, FCloverSetKey ParentSet)
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
	FSKSocketKey(uint32 GetsSlicedTo28Bits, FCloverSetKey ParentSet)
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
struct ARTILLERYRUNTIME_API FCloverResultSet
{
	FLargeGate Bloomlike;
	FCRKeyStruct UnderlyingKeys;
	FCloverSetKey SetRepresentedIfAny;
	FSkeletonKey OwnerIfAny;// this makes a ton of stuff a million times less awkward.
	bool Dirtied_AllowsFalseFalses = false; //not everything that dirties a set touches this, and not everything should.
	FCloverResultSet() : Bloomlike(), SetRepresentedIfAny()
	{
	}

	//copy
	FCloverResultSet(const FCloverResultSet& Other)
		: Bloomlike(Other.Bloomlike),
		  UnderlyingKeys(Other.UnderlyingKeys),
		  SetRepresentedIfAny(Other.SetRepresentedIfAny),
		  OwnerIfAny(Other.OwnerIfAny)
	{
	}

	//move
	FCloverResultSet(FCloverResultSet&& Other) noexcept
		: Bloomlike(std::move(Other.Bloomlike)),
		  UnderlyingKeys(std::move(Other.UnderlyingKeys)),
		  SetRepresentedIfAny(std::move(Other.SetRepresentedIfAny)),
		  OwnerIfAny(std::move(Other.OwnerIfAny))
	{
	}

	FCloverResultSet(FCloverSetKey MyKey) : Bloomlike(), SetRepresentedIfAny(MyKey)
	{
	}

	explicit FCloverResultSet(const FCRKeyStruct::iterator& Prefix)
	{
		UnderlyingKeys =  FCRKeyStruct(Prefix, FCRKeyStruct::iterator());
		for (auto i : UnderlyingKeys)
		{
			Bloomlike.Add(i);
		}
	}

	FCloverResultSet& operator=(const FCloverResultSet& Other)
	{
		if (this == &Other)
			return *this;
		Bloomlike = Other.Bloomlike;
		UnderlyingKeys = Other.UnderlyingKeys;
		SetRepresentedIfAny = Other.SetRepresentedIfAny;
		OwnerIfAny = Other.OwnerIfAny;
		return *this;
	}

	FCloverResultSet& operator=(FCloverResultSet&& Other) noexcept
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
	FResultSetInterstitial IntersectSets(FCloverResultSet& rhs);
	bool Find(FSkeletonKey CheckPresenceOf);
	void Rebuild();
};


struct FResultSetInterstitial
{
	FResultSetInterstitial(FCloverResultSet& Contained)
		: Contained(Contained)
	{
	}
	
	FResultSetInterstitial(FCloverResultSet&& Contained)
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

	FCloverResultSet Finalize()
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
		return FCloverResultSet(std::move(Contained));
	}
	
	FResultSetInterstitial* IntersectSets(FCloverResultSet& larger)
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
	FCloverResultSet& Contained;
	
	
	using  FCSResults = FCloverResultSet;
};

inline FResultSetInterstitial FCloverResultSet::IntersectSets(FResultSetInterstitial& rhs)
{
	FResultSetInterstitial Result(  std::move(FCloverResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline FResultSetInterstitial FCloverResultSet::IntersectSets(FCloverResultSet& rhs)
{
	FResultSetInterstitial Result(  std::move(FCloverResultSet(*this)));
	Result.IntersectSets(rhs);
	return std::move(Result);
}

inline bool FCloverResultSet::Find(FSkeletonKey CheckPresenceOf)
{
	if (Bloomlike.ApproxFind(CheckPresenceOf))
	{
		return UnderlyingKeys.contains(CheckPresenceOf);
	}
	return false;
}

inline void FCloverResultSet::Rebuild()
{
}


//suitable for slow polled data presentation that refreshes rarely.
struct ARTILLERYRUNTIME_API FCloverData : public FCloverResultSet
{
};

//"push model" for clover data, emits listenable blueprint events for data change at a configurable K change interval
struct ARTILLERYRUNTIME_API FEventedCloverData : public FCloverData
{
};