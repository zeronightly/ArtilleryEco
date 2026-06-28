#include "CloverDispatch.h"


#include "ArtilleryBPLibs.h"

class UThistleDispatch;

bool UCloverDispatch::RegistrationImplementation()
{
	return true;
}

void UCloverDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	ContingentPhysicsLinkage = Collection.InitializeDependency<UBarrageDispatch>();
	ArtilleryDispatch = Collection.InitializeDependency<UArtilleryDispatch>();
	MyShadowCopies.Reserve(1024);
	MyTrackingSet.reserve(1024);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UCloverDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	if ([[maybe_unused]] const UWorld* World = InWorld.GetWorld()) {
		UE_LOG(LogTemp, Warning, TEXT("ThistleDispatch:Subsystem: World beginning play"));
	}
}

void UCloverDispatch::Deinitialize()
{
	Super::Deinitialize();
}

TPair<FSKSocketKey, FSkeletonKey> UCloverDispatch::PlugToSocket(FSkeletonKey A, FSKSocketKey Into,
	FSkeletonKey SocketOwnedBy)
{
	return {};
}

TPair<FSKSocketKey, FSkeletonKey> UCloverDispatch::PlugToSocket(FSkeletonKey A, FSKSocketKey Into,
	PlayerKey SocketOwnedBy)
{
	return {};
}


FCloverSetKey UCloverDispatch::GiveShiny(FSkeletonKey A, FSKItemArchetypeKey B)
{
	return {};
}

FSKItemKey UCloverDispatch::InstanceItem(FSkeletonKey OptionalOwner, FSKItemArchetypeKey A)
{
	return {};
}

FCloverSetKey UCloverDispatch::GiveSpecificShiny(FSkeletonKey A, FSKItemKey B)
{
	return {};
}


FSKSocketKey UCloverDispatch::GrantSocketToSet(FCloverSetKey Set)
{
	return {};
}

FCloverSetKey UCloverDispatch::GrantSetToKey(FSkeletonKey Owner)
{
	return {};
}

///this could be quite slow. look out for it in the profiler.
FCloverResultSet UCloverDispatch::GetAllEntitiesWithinDistanceOfPoint(FVector2d& Point, double Radius)
{
	FBox2D bound = FBox2D(Point - Radius, Point + Radius);
	
	FCloverResultSet ret;
	TArray<TPair<FBarrageKey, FVector2d>> Entities;
	auto localbind = QuadTreeForDistance;
	if (localbind)
	{
		localbind.Get()->GetElements(bound, Entities);
		for (auto pair : Entities)
		{
			ret.UnderlyingKeys.insert(pair.Key.KeyIntoBarrage);
		}
		ret.Rebuild();
	}
	return ret;
}

FSKPlugKey UCloverDispatch::BindAbilityToSocket(FGunKey Instance, FSKSocketKey BindTo)
{
	return {};
}

FSKItemKey UCloverDispatch::AddItemInstanceToSet(FSKItemKey Place, FCloverSetKey Into)
{
	return {};
}

bool UCloverDispatch::TrackGameplayTagSet_NotRetroactive(FGameplayTag Tag)
{
	FCloverResultSet empty;
	SetsByTag.emplace(Tag, empty);
	return true;
}

FSKPlugKey UCloverDispatch::BindArbitraryKeyToSocket(FSkeletonKey NoWarranty, FSKSocketKey BindingTo)
{
	//this should either use the dependent key mechanism to bind to the socket or to the set, and I just can't decide which
	//I think to the socket, since the socket connects to the set? so that ends up being
	//do we use the socket key mechanism? I'm honestly not sure. we might condense sockets into an item type and free 
	// a whole keytype. I'll come back to this.
	return {};//should this be item instance or plug key? 
}

void UCloverDispatch::ArtilleryTick(uint64_t TicksSoFar)
{
	
	//k-2: accumulate entities
	if (TicksSoFar-2 % CLOVER_MAPPINGCADENCE)
	{
		MyTrackingSet.clear();
		auto HoldOpen = ContingentPhysicsLinkage->JoltGameSim;
		HoldOpen->GetBodiesList(MyTrackingSet);
	}
	//k-1: accumulate data
	if (TicksSoFar-1 % CLOVER_MAPPINGCADENCE)
	{
		auto HoldOpen = ContingentPhysicsLinkage->JoltGameSim;
		if (HoldOpen)
		{
			PhysicsStateMaintence = true;
			MyShadowCopies.Empty();
			FrozenPhysicsState.clear();
			for (auto result : MyTrackingSet)
			{
				if (!result.IsInvalid())
				{
					auto flat = HoldOpen->physics_system.Get()->GetBodyLockInterface().TryGetBody(result);
					MyShadowCopies.Emplace(*flat);
					
					auto id = ContingentPhysicsLinkage->GenerateBarrageKeyFromBodyId(result);
					FrozenPhysicsState.emplace(id, *flat);
				}
			}
		}
	}
	std::atomic_signal_fence(std::memory_order_acq_rel);
	PhysicsStateMaintence = false;
	//build distance map
	if (TicksSoFar % CLOVER_MAPPINGCADENCE == 0)
	{
		QuadTreeMaintenance = true;
		TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> HoldOpen = QuadTreeForDistance; // retain the ref to the old map until our tick is finished.
		TSharedPtr<TQuadTree<TPair<FBarrageKey, FVector2d>>> QuadTreeCandidate = MakeShareable(new TQuadTree<TPair<FBarrageKey, FVector2d>>(FBox2d(FVector2d::ZeroVector - 200000, FVector2d::ZeroVector + 200000)));  //swap now.
		for (int i = 0; i < MyTrackingSet.size(); ++i)
		{
			auto& body = MyShadowCopies[i];
			auto id = ContingentPhysicsLinkage->GenerateBarrageKeyFromBodyId(MyTrackingSet[i]);
			auto layer = body.GetObjectLayer();
			if (uint8(layer) != Layers::EJoltPhysicsLayer::NON_MOVING 
				&& layer != Layers::EJoltPhysicsLayer::HITBOX
				&& layer != Layers::EJoltPhysicsLayer::DEBRIS
				&& layer != Layers::ENEMYHITBOX) // TODO: CHECK THIS CHECK THIS CHECK THIS SERIOUSLY WE MIGHT WANT THESE
			{
				auto location = MyShadowCopies[i].GetCenter();
				auto box = body.GetWorldSpaceBounds();
				auto mini = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(box.mMin));
				auto maxi = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(box.mMax));
				auto UEBox = FBox2d(mini, maxi);
				FVector2d TwoDLoc = FVector2d(CoordinateUtils::FromJoltCoordinatesFakePrecision(location));
				auto pair = TPair<FBarrageKey, FVector2d>(id, TwoDLoc);
				QuadTreeCandidate->Insert(pair,UEBox);
			}
		}
		QuadTreeForDistance = QuadTreeCandidate;
		QuadTreeMaintenance = false;
	}
	if (TicksSoFar+1 % CLOVER_MAPPINGCADENCE == 0)
	{
		SetsByTag.visit_all([](std::pair<FGameplayTag, FCloverResultSet>& Pair)
		{
			if (Pair.second.Dirtied_AllowsFalseFalses)
			{
				Pair.second.Rebuild();
			}
		});
	}
}



void UCloverDispatch::Tick(float DeltaTime)
{
}

TStatId UCloverDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCloverDispatch, STATGROUP_Tickables);
}


bool UCloverDispatch::GetSetKeysByOwner(FSkeletonKey Owner, UCloverDispatch::OwnedSets& OutParam)
{
	bool found = bool(SetsByOwner.visit(Owner, [&OutParam](auto& a) { OutParam = a.second; }));
	return found;
}



FCloverResultSet UCloverDispatch::IntersectSets(FCloverResultSet smaller, FCloverResultSet larger)
{
	return smaller.IntersectSets(larger).Finalize();
}

FCloverResultSet UCloverDispatch::IntersectSetsByKey(FCloverSetKey AKey, FCloverSetKey BKey)
{
	UCloverDispatch::OwnedSets ASet, BSet;
	bool found = bool(SetsByOwner.visit(AKey.GetSK(), [&ASet](auto& a) { ASet = a.second; }));
	if (found)
	{
		found = bool(SetsByOwner.visit(BKey.GetSK(), [&BSet](auto& a) { BSet = a.second; }));
		if (found)
		{
			auto& smaller = ASet.UnderlyingKeys.size() < BSet.UnderlyingKeys.size() ? ASet : BSet;
			auto& larger = ASet.UnderlyingKeys.size() < BSet.UnderlyingKeys.size() ? BSet : ASet;
			return smaller.IntersectSets(larger).Finalize();
		}
	}
	return {};
}

bool UCloverDispatch::GetPlugsForSocketKey(FSKSocketKey ASocket, OwnedSets& OutParamOptionallyLiveMemory)
{
	return GetSetKeysByOwner(ASocket.GetSK(), OutParamOptionallyLiveMemory);
}

FSkeletonKey UCloverDispatch::AddToSet(FSkeletonKey Place, FCloverSetKey Into)
{
	UCloverDispatch::OwnedSets ASet;
	bool found = bool(SetsByOwner.visit(Into.GetSK(), [&ASet](auto& a) { ASet = a.second; }));
	if (found)
	{
		ASet.UnderlyingKeys.insert(Place);
		std::atomic_signal_fence(std::memory_order_acq_rel);
		ASet.Bloomlike.Add(Place); 
		return Place;
	}
	return FSkeletonKey();
}

FCloverResultSet UCloverDispatch::GetPlayerInventories(PlayerKey TargetPlayer)
{
	UCloverDispatch::OwnedSets ASet;
	bool found = bool(SetsByPlayer.visit(TargetPlayer, [&ASet](auto& a) { ASet = a.second; }));
	return ASet;//we may want to filter by the Inventory type and subtype just to be real sure.
}

FCloverResultSet UCloverDispatch::GetNPInventories(FSkeletonKey Target)
{
	UCloverDispatch::OwnedSets ASet;
	bool found = bool(SetsByOwner.visit(Target, [&ASet](auto& a) { ASet = a.second; }));
	return ASet;//we may want to filter by the Inventory type and subtype just to be real sure.
}

std::optional<FCloverSetKey> UCloverDispatch::GetMainPlayerInventory(PlayerKey TargetPlayer)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::MainInventory);
	return FCloverSetKey().FromSK(FSkeletonKey(key));
}

std::optional<FCloverSetKey> UCloverDispatch::GetPlayerQuests(PlayerKey TargetPlayer)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::Quests);
	return FCloverSetKey().FromSK(FSkeletonKey(key));
}

std::optional<FCloverSetKey> UCloverDispatch::GetPlayerConditions(PlayerKey TargetPlayer)
{
	UCloverDispatch::OwnedSets ASet = GetPlayerInventories(TargetPlayer);
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(TargetPlayer), SpecialPlugKeySlices::Conditions);
	
	return FCloverSetKey().FromSK(FSkeletonKey(key));
}

std::optional<FCloverSetKey> UCloverDispatch::GetNPConditions(FSkeletonKey Target)
{
	auto key = BuildKnownKey(SKELLY::SFIX_InventoryOf, uint32(Target), SpecialPlugKeySlices::Conditions);
	return FCloverSetKey().FromSK(FSkeletonKey(key));
}

FName UCloverDispatch::GetHumanReadable(FSKPlugKey A)
{
	
	return {};
}


FCloverResultSet UCloverDispatch::GetSubsetByTag(FGameplayTag Tag, FCloverResultSet B)
{
	//TODO: add check for "tracked tag" which would allow the extremely fast set intersect.
	FCloverResultSet TagSet;
	if (SetsByTag.contains(Tag))
	{
		auto Set = SetsByTag.visit(Tag, [&TagSet](auto& a) { TagSet = a.second; });
		IntersectSets(B, TagSet);
		return B;
	}
	else
	{
		for (auto Test : B.UnderlyingKeys)
		{
			bool TagPresent = ArtilleryDispatch->DoesEntityHaveTag(FSkeletonKey(Test), Tag);
			if (TagPresent)
			{
				//since we didn't use the visit, we don't map to a tracked set and we can just reuse our dec'd var since construction isn't free.
				TagSet.UnderlyingKeys.insert(Test);
			}
		}
		TagSet.Rebuild();
	}
	return TagSet;
}

seq::radix_set<unsigned long long>::const_iterator UCloverDispatch::GetInventoriesOfKey(FSkeletonKey Owner)
{
	auto K = GetSubtypedPrefix(SKELLY::SFIX_InventoryOf, Owner);
	FCloverResultSet OutParam;
	bool found = bool(SetsByOwner.visit(Owner, [&OutParam](auto& a) { OutParam = a.second; }));
	return SeqU64Prefix(OutParam.UnderlyingKeys, K << 32 , 36); //double check this line...
}

TArray<FCloverSetKey> UCloverDispatch::GetKeysFromResultSet(FCloverResultSet ToUnpack)
{
	TArray<FCloverSetKey> Ret;
	for (auto key : ToUnpack.UnderlyingKeys)
	{
		Ret.Add(FCloverSetKey::FromSK(FSkeletonKey(key)));
	}
	return Ret;
}
