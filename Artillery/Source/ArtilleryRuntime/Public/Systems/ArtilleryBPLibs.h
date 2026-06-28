// ReSharper disable CppRedundantParentheses
#pragma once

#include <GunOwner.h>

#include "ArtilleryDispatch.h"
#include "ArtilleryProjectileDispatch.h"
#include "CanonicalInputStreamECS.h"
#include "FTTimer.h"
#include "PhysicsTypes/BarragePlayerAgent.h"
#include "UArtilleryGameplayTagContainer.h"
#include "UFireControlMachine.h"
#include "Components/TimerTickliteHandlerComponent.h"
#include "ArtilleryBPLibs.generated.h"

UCLASS(meta=(ScriptName="InputSystemLibrary"))
class ARTILLERYRUNTIME_API UInputECSLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static void GetHistoricalInputs(UCanonicalInputStreamECS* ptr, TArray<FArtilleryShell>& Inputs, int Count)
	{
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> sptr = ptr->GetStream(streamkey);
			for(int InputIndex = 0; InputIndex <= Count; ++InputIndex)
			{
				std::optional<FArtilleryShell> input =  sptr.Get()->peek( sptr->GetHighestGuaranteedInput() - InputIndex);
				Inputs.Add(input.has_value() ? input.value() : FArtilleryShell());
			}
		}
	}
	
	UFUNCTION(BlueprintPure, meta = (ScriptName = "Get15PlayerInputs", DisplayName = "Get Last 15 of Local Player's Inputs", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Inputs")
	static void K2_Get15LocalHistoricalInputs(UObject* WorldContextObject, TArray<FArtilleryShell> &Inputs)
	{
		GetHistoricalInputs(UCanonicalInputStreamECS::Get(WorldContextObject), Inputs, 15);
	}
};

UCLASS(meta=(ScriptName="ArtillerySystemLibrary"))
class ARTILLERYRUNTIME_API UArtilleryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	
public:
	
	UArtilleryLibrary()
	{
		
	};
	
	//DEPRECATED
	//TODO: replace with something non-monotonic but still cadenced. aw jeez.
	static int32 GetTotalsTickCount(UObject* WorldContextObject)
	{
		return UArtilleryDispatch::Get(WorldContextObject) ? UArtilleryDispatch::Get(WorldContextObject)->ArtilleryAsyncWorldSim.SeqNumber / 4 : 0;
	}

	static int32 GetTotalsTickCount(UArtilleryDispatch* MyDispatch)
	{
		return MyDispatch ? MyDispatch->ArtilleryAsyncWorldSim.SeqNumber / 4 : 0;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetLocFromTransformDispatchIfAny", WorldContext = "WorldContextObject", HidePin = "WorldContextObject",  DisplayName = "Checks TransformDispatch For A Location", ExpandBoolAsExecs="bFound"), Category="Artillery|Attributes")
	static FVector K2_GetBarrageLocIfAny(UObject* WorldContextObject, FSkeletonKey Owner, bool& bFound)
	{
		bFound = false;
		
		return implK2_GetLocation(UArtilleryDispatch::Get(WorldContextObject), Owner, bFound);
	}
	
	static FVector implK2_GetLocation(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, bool& bFound)
	{
		bFound = false;
		if(Dispatch)
		{
			//welp. more like itty bitty living space actually.
			FBLet Fiblet = Dispatch->GetFBLetByObjectKey(Owner, Dispatch->GetShadowNow());
			if(Fiblet)
			{
				FVector3f PantsWettingTerror = FBarragePrimitive::GetPosition(Fiblet);
				//make sure they aren't a NANdere lol
				if (!PantsWettingTerror.ContainsNaN())
				{
					bFound = true;
					return FVector(PantsWettingTerror);
				}
			}
		}
		bFound = false;
		return FVector( NAN); // YOU SHOULD NOT HAVE COME HERE
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetAttribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Get Attribute Of", ExpandBoolAsExecs="bFound"), Category="Artillery|Attributes")
	static float K2_GetAttrib(UObject* WorldContextObject, FSkeletonKey Owner, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		return implK2_GetAttrib(UArtilleryDispatch::Get(WorldContextObject), Owner,Attrib, bFound);
	}
	
	static Attr3Ptr implK2_GetAttr3Ptr(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_VectorAttrib Attrib, bool& bFound)
	{
		bFound = false;
		if(Dispatch)
		{
			Attr3Ptr AttribVecPtr = Dispatch->GetVecAttr( Owner, Attrib);
			if(AttribVecPtr != nullptr)
			{
				bFound = true;
				return AttribVecPtr;
			}
		}
		return nullptr;
	}

	static void RequestUnboundGun(UArtilleryDispatch* Dispatch, FARelatedBy Relationship, const FSkeletonKey& Requester, const FGunKey& GunKey)
	{
		if(Dispatch)
		{
			TSharedPtr<F_INeedA> RouterHold = Dispatch->RequestRouter;
			if(RouterHold)
			{
				RouterHold->NewUnboundGun(Requester, GunKey, Relationship, Dispatch->GetShadowNow());	
			}
		}
	}

	//TODO: allow guns to fire in the past?
	static void RequestGunFire(UArtilleryDispatch* Dispatch, const FGunKey& GunKey)
	{
		if(Dispatch)
		{
			TSharedPtr<F_INeedA> RouterHold = Dispatch->RequestRouter;
			if(RouterHold)
			{
				RouterHold->GunFired(GunKey, Dispatch->GetShadowNow());
			}
		}
	}

	//TODO: This takes a key, gets the location from the ATTRIBUTE, and then shifts it up to represent a good centroid target point
	static FVector GetPlayerLocationAsEstTarget(UArtilleryDispatch* Dispatch, E_PlayerKEY Player)
	{
		if(Dispatch)
		{
			bool bFound  = false;
			FVector Value = FVector::ZeroVector;
			GetAnyPlayerVector(Dispatch->GetWorld(), E_VectorAttrib::Location, Player, bFound, Value);
			
			if (bFound && !Value.IsNearlyZero())
			{
				float height = 0.0;
				GetAnyPlayerAttrib(Dispatch->GetWorld(), E_AttribKey::Height, Player, bFound, height);
				if (bFound)
				{
					FVector Dir = FVector();
					K2_GetPlayerDirectionEstimator(Dispatch, Dir);
					return Value + Dir + (FVector::UpVector * height);
				}
			}
		}
		return FVector();
	}

	static float implK2_GetAttrib(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_AttribKey Attrib)
	{
		if(Dispatch)
		{
			AttrPtr Attribute = Dispatch->GetAttrib(Owner, Attrib);
			if(Attribute.IsValid())
			{
				return Attribute->GetCurrentValue();
			}
		}
		return NAN;
	}
	
	static float implK2_GetAttrib(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		if(Dispatch)
		{
			AttrPtr AttributePtr = Dispatch->GetAttrib( Owner, Attrib);
			if(AttributePtr != nullptr)
			{
				bFound = true;
				return AttributePtr->GetCurrentValue();
			}
		}
		return NAN;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetRelatedKey", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Get Related Key From", ExpandBoolAsExecs="bFound"), Category="Artillery|Keys")
	static FSkeletonKey K2_GetIdentity(UObject* WorldContextObject, FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		return implK2_GetIdentity(UArtilleryDispatch::Get(WorldContextObject), Owner, Attrib, bFound);
	}
	
	static FSkeletonKey implK2_GetIdentity(UArtilleryDispatch* Dispatch,  FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		if(Dispatch)
		{
			IdentPtr ident = Dispatch->GetIdent( Owner, Attrib);
			if(ident)
			{
				bFound = true;
				return ident->CurrentValue;
			}
		}
		return FSkeletonKey();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerRelatedKey", DisplayName = "Get Local Player's Related Key", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Keys")
	static FSkeletonKey K2_GetPlayerIdentity(UObject* WorldContextObject, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return  implK2_GetIdentity(UArtilleryDispatch::Get(WorldContextObject), key, Attrib, bFound);
			}
		}
		bFound = false;
		return FSkeletonKey();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetThisActorAttribute", DisplayName = "Get My Actor's Attribute", DefaultToSelf = "Actor", HidePin = "Actor", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static float K2_GetMyAttrib(AActor *Actor, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		UKeyCarry* ptr = Actor->GetComponentByClass<UKeyCarry>();
		if(ptr && ptr->GetWorld())
		{
			if(FSkeletonKey key = ptr->GetMyKey())
			{
				return implK2_GetAttrib(UArtilleryDispatch::Get(ptr->GetWorld()), key, Attrib, bFound);
			}
		}
		bFound = false;
		return NAN;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerVector", DisplayName = "Get Local Player's Vector Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static FVector K2_GetPlayerVector(UObject* WorldContextObject, E_VectorAttrib Attrib, bool& bFound)
	{
		return K2_GetAnyPlayerVector(WorldContextObject, Attrib, E_PlayerKEY::CABLE, bFound);
	}

	static bool GetAnyPlayerVector(UWorld* ZWorld, E_VectorAttrib Attrib, E_PlayerKEY Player, bool& bFound, FVector& Value)
	{
		if (ZWorld)
		{
			UCanonicalInputStreamECS* ptr = ZWorld->GetSubsystem<UCanonicalInputStreamECS>();
			if(ptr)
			{
				InputStreamKey streamkey = ptr->GetStreamForPlayer(Player);
				ActorKey key = ptr->ActorByStream(streamkey);
				if(key)
				{
					Attr3Ptr attr3p = implK2_GetAttr3Ptr(UArtilleryDispatch::Get(ZWorld), key, Attrib, bFound);
					if(attr3p)
					{
						Value = attr3p->CurrentValue;
						return true;
					}
				}
			}
		}
		bFound = false;
		return false;
	}

	static bool GetAnyPlayerAttrib(UWorld* ZWorld, E_AttribKey Attrib, E_PlayerKEY Player, bool& bFound, float& Value)
	{
		if (ZWorld)
		{
			UCanonicalInputStreamECS* ptr = ZWorld->GetSubsystem<UCanonicalInputStreamECS>();
			if(ptr)
			{
				InputStreamKey streamkey = ptr->GetStreamForPlayer(Player);
				ActorKey key = ptr->ActorByStream(streamkey);
				if(key)
				{
					Value = implK2_GetAttrib(UArtilleryDispatch::Get(ZWorld),key, Attrib, bFound);
					return true;
					}
				}
			}
		bFound = false;
		Value = NAN;
		return false;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetAnyPlayerVector", DisplayName = "Get Any Player's Vector Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static FVector K2_GetAnyPlayerVector(UObject* WorldContextObject, E_VectorAttrib Attrib, E_PlayerKEY Player, bool& bFound)
	{
		bFound = false;
		FVector Value;
		return GetAnyPlayerVector(WorldContextObject->GetWorld(), Attrib, Player, bFound, Value) ? Value : FVector();
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerAttribute", DisplayName = "Get Local Player's Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Attributes")
	static float K2_GetPlayerAttrib(UObject* WorldContextObject, E_AttribKey Attrib)
	{
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return implK2_GetAttrib(UArtilleryDispatch::Get(WorldContextObject), key, Attrib);
			}
		}
		return NAN;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyDamage", DisplayName = "Apply Damage", ExpandBoolAsExecs="bSuccess", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Attributes")
	static void K2_ApplyDamage(UObject* WorldContextObject, FSkeletonKey Target, float DamageToApply, bool& bSuccess)
	{
		bSuccess = ApplyDamage(UArtilleryDispatch::Get(WorldContextObject), Target, DamageToApply);
	}

	static bool ApplyDamage(UArtilleryDispatch* Dispatch, FSkeletonKey Target, float DamageToApply, FVector SourceLocation = FVector::ZeroVector)
	{
		bool bSuccess = false;
		if(Dispatch)
		{
			if(AttrPtr TargetProposedDamageAtr = Dispatch->GetAttrib(Target, E_AttribKey::ProposedDamage))
			{
				bSuccess = true;
				TargetProposedDamageAtr->AddToCurrentValue(DamageToApply);
			}
			if (bSuccess && !SourceLocation.IsZero())
			{
				bool bVecFound = false;
				if (Attr3Ptr SourceAttr = implK2_GetAttr3Ptr(Dispatch, Target, E_VectorAttrib::TargetLocation, bVecFound))
				{
					SourceAttr->SetCurrentValue(SourceLocation);
				}
			}
		}
		return bSuccess;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetGameplayTagsByKey", DisplayName = "Get Gameplay Tags for Key", ExpandBoolAsExecs="bFound", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Tags")
	static UArtilleryGameplayTagContainer* K2_GetTagsByKey(UObject* WorldContextObject, FSkeletonKey Key, bool& bFound)
	{
		FConservedTags Tags =  UArtilleryDispatch::Get(WorldContextObject)->GetExistingConservedTags(Key);
		if (Tags.IsValid())
		{
			bFound = true;
			UArtilleryGameplayTagContainer* TagRef = NewObject<UArtilleryGameplayTagContainer>();
			TagRef->Initialize( Key, UArtilleryDispatch::Get(WorldContextObject), true);
			return TagRef;
		}
		bFound = false;
		return nullptr;
	}
	
	static FConservedTags InternalTagsByKey(UArtilleryDispatch* Dispatch, FSkeletonKey Key, bool& bFound)
	{
		bool Found = false;
		FConservedTags ret = Dispatch->GetOrRegisterConservedTags(Key, Found);
		bFound = ret.IsValid();
		return ret;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerGameplayTags", DisplayName = "Get Local Player's Gameplay Tags", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Tags")
	static UArtilleryGameplayTagContainer* K2_GetPlayerTags(UObject* WorldContextObject, bool& bFound)
	{
		bFound = false;
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return K2_GetTagsByKey(UArtilleryDispatch::Get(WorldContextObject), key, bFound);
			}
		}
		bFound = false;
		return nullptr;
	}
	
	
	//This is bad practice but no longer unsafe. because it is now WORLD AWARE, it will select the local player for the
	//current world. you really really should not use it, but it now does the correct thing.
	static TObjectPtr<UBarragePlayerAgent> GetLocalPlayerBarrageAgent(UArtilleryDispatch* Dispatch)
	{
		auto TransformDispatch = UTransformDispatch::Get(Dispatch->GetWorld());
		auto InputECS = UCanonicalInputStreamECS::Get(Dispatch->GetWorld());
		if (TransformDispatch && InputECS)
		{
			InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				ActorKey playerkey = InputECS->ActorByStream(local);
				AActor* SecretName = TransformDispatch->GetAActorByObjectKey(playerkey).Get();
				if (SecretName && SecretName->GetComponentByClass<UBarragePlayerAgent>()->IsReady)
				{
					return SecretName->GetComponentByClass<UBarragePlayerAgent>();
				}
			}
		}
		return nullptr;
	}
	
	static TObjectPtr<UBarragePlayerAgent> GetLocalPlayerBarrageAgent(UCanonicalInputStreamECS* InputDispatch)
	{
		auto TransformDispatch = UTransformDispatch::Get(InputDispatch->GetWorld());
		if (TransformDispatch && InputDispatch)
		{
			InputStreamKey local = InputDispatch->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				ActorKey playerkey = InputDispatch->ActorByStream(local);
				AActor* SecretName = TransformDispatch->GetAActorByObjectKey(playerkey).Get();
				if (SecretName && SecretName->GetComponentByClass<UBarragePlayerAgent>()->IsReady)
				{
					return SecretName->GetComponentByClass<UBarragePlayerAgent>();
				}
			}
		}
		return nullptr;
	}
	
	static FSkeletonKey GetLocalPlayerKey_LOW_SAFETY(UArtilleryDispatch* Dispatch)
	{
		auto InputECS = UCanonicalInputStreamECS::Get(Dispatch->GetWorld());
		if (InputECS)
		{
			InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				return InputECS->ActorByStream(local);
			}
		}
		return FSkeletonKey();
	}

	//we need a better and less dangerous idiom than this, you were right, @Maslab.
	static AActor* GetLocalPlayer_UNSAFE(UArtilleryDispatch* Dispatch)
	{
		
		auto TransformDispatch = UTransformDispatch::Get(Dispatch->GetWorld());
		auto InputECS = UCanonicalInputStreamECS::Get(Dispatch->GetWorld());
		if (TransformDispatch && InputECS)
		{
			InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				ActorKey playerkey = InputECS->ActorByStream(local);
				return TransformDispatch->GetAActorByObjectKey(playerkey).Get();
			}
		}
		return nullptr;
	}
	
	//DEPRECATED
	//TODO: WARNING SERIOUS DETERMINISM RISK. THIS REFERENCES CHAOS AND IS CALLED ALL OVER THE PLACE.
	static void GetLocalPlayerVectors(UArtilleryDispatch* Dispatch, FVector& Forward, FVector& Right)
	{
		TObjectPtr<UBarragePlayerAgent> local = GetLocalPlayerBarrageAgent(Dispatch);
		if(local && local->IsReady)
		{
			Forward = local->Chaos_LastGameFrameForwardVector();
			Right = local->Chaos_LastGameFrameRightVector();
		}
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerVectors", DisplayName = "Get Local Player's Attribute"),  Category="Artillery|Character")
	static void K2_GetLocalPlayerVectors(UArtilleryDispatch* Dispatch, FVector& Forward, FVector& Right)
	{
		GetLocalPlayerVectors(Dispatch, Forward, Right);
	}

	static void GetLocalPlayerVelocity(UArtilleryDispatch* Dispatch, FVector& Velocity)
	{
		TObjectPtr<UBarragePlayerAgent> local = GetLocalPlayerBarrageAgent(Dispatch);
		if(local && local->IsReady)
		{
			Velocity = FVector(local->GetVelocity());
		}
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerVelocity", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Get Local Player's Velocity"),  Category="Artillery|Character")
	static void K2_GetLocalPlayerVelocity(UObject* WorldContextObject, FVector& Velocity)
	{
		GetLocalPlayerVelocity(UArtilleryDispatch::Get(WorldContextObject), Velocity);
	}

	static void SimpleEstimator(UCanonicalInputStreamECS* ptr, FVector& Forwardish, double Counter = 15)
	{
		FVector Right;
		auto Dispatch = ptr->GetWorld()->GetSubsystem<UArtilleryDispatch>();
		GetLocalPlayerVectors(Dispatch, Forwardish, Right);
		TArray<FArtilleryShell> In;
		UInputECSLibrary::GetHistoricalInputs(ptr, In, Counter);
		double accumulateX = 0;
		double accumulateY = 0;
		for(FArtilleryShell& shell : In)
		{
			accumulateX += shell.GetStickLeftX();
			accumulateY += shell.GetStickLeftY();
		}
		accumulateX = accumulateX/Counter;
		accumulateY = accumulateY/Counter;
		//for serious work, replace this.
		accumulateX += In[0].GetStickLeftX();
		accumulateX += In[0].GetStickLeftX();
		accumulateX += In[0].GetStickLeftX();
		accumulateY += In[0].GetStickLeftY();
		accumulateY += In[0].GetStickLeftY();
		accumulateY += In[0].GetStickLeftY();
		accumulateX = accumulateX/4.0;
		accumulateY = accumulateY/4.0;
		TObjectPtr<UBarragePlayerAgent> bind = GetLocalPlayerBarrageAgent(Dispatch);
		if(bind)
		{
			FVector moveX = accumulateX * bind->Acceleration * Right;
			FVector moveY = accumulateY * bind->Acceleration * Forwardish;
			Forwardish = moveX + moveY;
		}
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerDirectionEstimator", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Get Local Player's Direction Estimator"),  Category="Artillery|Character")
	static void K2_GetPlayerDirectionEstimator(UObject* WorldContextObject, FVector& Forward)
	{
		 SimpleEstimator(UCanonicalInputStreamECS::Get(WorldContextObject), Forward, 15);
	}

	//differs from tombstone primitive ONLY in that it will NOT kill non-projectiles.
	static bool SafelyDeleteProjectile(UArtilleryDispatch* Dispatch, FSkeletonKey Target)
	{
		auto BulletDispatch = Dispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
		if(Dispatch && BulletDispatch && BulletDispatch->IsArtilleryProjectile(Target))
		{
			TombstonePrimitive(Dispatch, Target);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "DeleteArtilleryProjectile", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Kills a projectile without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool K2_DeleteProjectile(UObject* WorldContextObject, FSkeletonKey Target)
	{
		return SafelyDeleteProjectile(UArtilleryDispatch::Get(WorldContextObject), Target);
	}

	static bool TombstonePrimitive(UArtilleryDispatch* Dispatch, FSkeletonKey Target)
	{
		// If a projectile, handle tombstone/delete differently through the projectile manager.
		auto Barrage = Dispatch->GetWorld()->GetSubsystem<UBarrageDispatch>();
		auto BulletDispatch = Dispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
		if(Dispatch && Barrage)
		{
			ArtilleryTime Now = Dispatch->GetShadowNow();
			FBLet Prim = Dispatch->GetFBLetByObjectKey(Target, Now);
			Dispatch->DeregisterGameplayTags(Target); //release tags if any.
			if (Prim && Prim->Me == FBShape::Projectile && BulletDispatch)
			{
				BulletDispatch->DeleteProjectile(Target); // quite a bit extra has to happen, but it does all happen.
			}
			return Barrage->SuggestTombstone(Prim) != 1;
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "TombstoneAPrimitive", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Kills a primitive without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool K2_TombstonePrimitive(UObject* WorldContextObject, FSkeletonKey Target)
	{
		return TombstonePrimitive(UArtilleryDispatch::Get(WorldContextObject), Target);
	}

	/**
	 * Spawn and Dispatch a timer ticklite on the current script owner.
	 * @return True if ticklite was successfully dispatched
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext="WorldContextObject", HidePin = "WorldContextObject", ScriptName = "SpawnAndDispatchTimerTicklite", DisplayName = "SpawnAndDispatchTimerTicklite", DefaultToSelf = "Owner", Hide), Category = "Artillery|Ticklites")
	static UTimerTickliteHandlerComponent* K2_SpawnAndDispatchTimerTickliteOnObject(UObject* WorldContextObject, AActor* Owner, int LifetimeInTicks)
	{
		UActorComponent* NewTimerTickliteComponent = Owner->AddComponentByClass(UTimerTickliteHandlerComponent::StaticClass(), false, Owner->GetTransform(), false);
		if (NewTimerTickliteComponent == nullptr)
		{
			return nullptr;
		}

		UTimerTickliteHandlerComponent* TimerComponent = Cast<UTimerTickliteHandlerComponent>(NewTimerTickliteComponent);
		
		auto Dispatch = UArtilleryDispatch::Get(WorldContextObject);
		StructureFullTL_Dispatch(Dispatch, TimerTicklite, TL_Timer, FTTimer, TimerComponent, LifetimeInTicks);
		Dispatch->RequestAddTicklite(TimerTicklite, Early);
		return TimerComponent;
	}

	UFUNCTION(BlueprintPure, Category = "SkeletonTypes")
	static void BreakSkeletonKey(const FSkeletonKey& Key, int& KeyValue)
	{
		KeyValue = Key.Obj;
	}

	static PlayerKey GetLocalPlayerKey(UWorld* World) { return PlayerKey::CABLE; }

	static FSkeletonKey GetLocalSkeletonKey(UWorld* World)
	{
		UCanonicalInputStreamECS* InputECS = World->GetSubsystem<UCanonicalInputStreamECS>();
		if (InputECS)
		{
			InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				return InputECS->ActorByStream(local);
			}
		}
		return FSkeletonKey();
	}

	static PlayerKey GetPlayerKeyFromSkeletonKey(UWorld* World, const FSkeletonKey& Key)
	{
		UCanonicalInputStreamECS* InputECS = World->GetSubsystem<UCanonicalInputStreamECS>();
		if (InputECS)
		{
			for (auto& Pair : InputECS->SessionPlayerToStreamMapping)
			{
				ActorKey actor = InputECS->ActorByStream(Pair.Value);
				if (actor == Key)
				{
					return Pair.Key;
				}
			}
		}
		return PlayerKey::CABLE;
	}
};
