#pragma once

#include <stdint.h>

#ifndef LORDLY_SKELETON_H
#define LORDLY_SKELETON_H
#define LORDLY_SKELETON_VER 0


// This used to be 'namespace SKELLY' it's useful to name this an enum, it will probably change names  
enum SKELLY : uint64_t
{///											  4bits for type, 28 for read only meta, 32 for hash.
	// One hex digit is one nibble, so our binary pattern is this:
	//							[TTTT][MMMM MMMM MMMM MMMM MMMM MMMM MMMM][HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH]
	SFIX_MaskForTypeErasure =		0x0FFFFFFFFFFFFFFF,
	SFIX_MaskForPrimaryType =		0xF000000000000000,
	SFIX_MaskForCoreHash =			0x00000000FFFFFFFF,
	
	SFIX_MaskForMetaBits =			0x0FFFFFFF00000000,
	SFIX_NONE	  =					0x0000000000000000,
	SFIX_NotchKeyForMetaUse=		0xFFFFFFFF0FFFFFFF, // this puts a 4-bit notch in the hash so that it can be used as meta value.
	
	//An archetype or shared instance gets one of these...
	SFIX_GunOrAbilityPrototypeKey = 0x1000000000000000,
//An individual _instance_ gets one of these...
	SFIX_GunOrAbilityInstance =		0x2000000000000000,
	SFIX_ProjectileOrDeployable =	0x3000000000000000,
	SFIX_BarrageKey =				0x4000000000000000,
	SFIX_ActorOrActorlike =			0x5000000000000000, //generally built off a pointer right now, that's likely gonna change.
	
	//HIGHER GAMEPLAY CONSTRUCTS, THIS TEXT IS FOR YOU ALONE.
	//These keys represent some pretty subtle concepts, and in the case of the socket key, they work due to the binary shape of the key.
	//ALL item related keys use notching. This allows us to effectively create a "dynamo-like" master key composition on the fly to define some pretty powerful hierarchies.
	//Sets are almost always of items. But not necessarily. They do still use notching, though, so you may be best off using them in conjunction with other keys that make
	//that easier and more functional.
	SFIX_SetOf		=				0x6000000000000000,
	SFIX_VendorOf	=				0x6000000010000000,
	SFIX_InventoryOf	=			0x6000000020000000,//add as needed. that subtype in the middle there means we could have 13 more of these.
	SFIX_MainInventoryOf	=		0x6000000030000000,
	SFIX_OrderedSetOf	=			0x6000000040000000,
	SFIX_ItemArchetype =			0x7000000000000000, 
	SFIX_ItemSubtype1 =				0x7000000010000000,// subtypes're pretty nice for making extremely fast filters. we use them with radix tries elsewhere.
	SFIX_Quest	=					0x7000000020000000, //Quests are just ordered sets held as an item. you can get a set from them.
	SFIX_ItemInstance =				0x8000000000000000,//Oh boy.
	SFIX_ItemInstanceStackableMask   =		0b0000000000000000000000000000000000010000000000000000000000000000,
	SFIX_ItemInstancePersistenceMask =		0b0000000000000000000000000000000000100000000000000000000000000000,
	SFIX_ItemInstanceFlag3Mask =			0b0000000000000000000000000000000001000000000000000000000000000000,

	//these are particularly complex, to be honest, and are mostly provided because ex-Bungie designers tend to "like" them.
	SFIX_NonActorActive =			0x9000000000000000, //used for binding things to scene components or similar. often used to bind to bones, often called bone keys.
	SFIX_Socket =					0xB0000000B0000000,
	SFIX_InvalidSocket =			0xB000000000000000,
	//SFIX_Plug =					0xB0000000_0000000, where the blank digit is the ORIGINAL TYPE of the hash's source. 
	// mass interop key planned for the future
	SFIX_MASSIDP =					0xA000000000000000,
	SFIX_UNUSEDC =					0xC000000000000000,
	SFIX_UNUSEDD =					0xD000000000000000,
	SFIX_UNUSEDE =					0xE000000000000000,
	SFIX_SK_LORD =					0xF000000000000000,
};
	static inline bool IS_OF_SK_TYPE(uint64_t MY_HASH,uint64_t MY_MASK) {return (MY_HASH & SKELLY::SFIX_MaskForPrimaryType) == MY_MASK;};
	static inline uint64_t GET_SK_TYPE(uint64_t MY_HASH) {return (MY_HASH & SKELLY::SFIX_MaskForPrimaryType);};
	static inline uint64_t FORGE_SKELETON_KEY(uint64_t MY_HASH,uint64_t MY_MASK) {return (MY_HASH & SKELLY::SFIX_MaskForTypeErasure) | MY_MASK;};
	constexpr uint64_t BoneKey_Infix = SKELLY::SFIX_NonActorActive;



#endif
enum SFIX_SubtypeSelector
{
	SFIX_SubtypeMaskOut =				   SFIX_NotchKeyForMetaUse,
	SFIX_ZeroST		=							0x0000000000000000,
	SFIX_ST_ONE		=							0x0000000010000000,
	SFIX_ST_TWO		=							0x0000000020000000,
	SFIX_ST_THREE	=							0x0000000030000000,
	SFIX_ST_FOUR	=							0x0000000040000000,
	PLUG			=							0x00000000F0000000
};

inline uint64_t SFIX_DestructiveApplySubtype(uint64_t Bitstring, SFIX_SubtypeSelector Subtype)
{
	return  (Bitstring & SFIX_SubtypeMaskOut) | Subtype;
}

static inline uint64_t SFIX_ImprintKeyDependency(uint64_t parent, uint32_t localunique,
													uint64_t MY_MASK = SKELLY::SFIX_ItemArchetype) //by default dependent keys are facts.
{
	auto ret = parent & SKELLY::SFIX_NotchKeyForMetaUse;
	ret = (ret << 32) | localunique;
	ret = FORGE_SKELETON_KEY(ret, MY_MASK); // I forgot this and lost rather a lot of time.| Update: I fucked it up after forgetting it. It's right now.
	return ret;
};

//TODO: replace with and standardize on fast hash if needed.
#define MAKE_BONEKEY(turn_into_key) FBoneKey(PointerHash(turn_into_key))
#define MAKE_ACTORKEY(turn_into_key) ActorKey(PointerHash(turn_into_key))

#define MYSTIC_STANDARDIZED_OFFSET 17
