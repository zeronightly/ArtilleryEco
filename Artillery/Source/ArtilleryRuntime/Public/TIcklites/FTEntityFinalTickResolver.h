
#pragma once
#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "ConditionTags.h"
#include "ModularGameplayTags.h"
#include "FArtilleryTicklitesThread.h"

//A ticklite's impl component(s) must provide:
//TICKLITE_StateReset on the memory block aspect
//TICKLITE_Calculate on the impl aspect
//TICKLITE_Apply on the impl aspect, consuming the memory block aspect's state
//TICKLITE_CoreReset on the impl aspect
//TICKLITE_CheckForExpiration on the impl aspect
//TICKLITE_OnExpiration
class TLEntityFinalTickResolver : public UArtilleryDispatch::TL_ThreadedImpl /*Facaded*/
{
public:
	FSkeletonKey EntityKey;
	TLEntityFinalTickResolver()
	{
	}

	TLEntityFinalTickResolver(
		FSkeletonKey Target
		) : TL_ThreadedImpl(), EntityKey(Target)
	{
	}
	void TICKLITE_StateReset()
	{
	}
	void TICKLITE_Calculate()
	{
	}
	
	void RechargeClamp(AttrMapPtr attribMap, AttribKey checkVal, AttribKey Max, AttribKey Current)
	{
		// We assume that nullptr check is done before calling this
		AttrPtr bindH = attribMap->FindRef(checkVal);
		if (bindH != nullptr && bindH->GetCurrentValue() > 0.f)
		{
			AttrPtr bindHMax = attribMap->FindRef(Max);
			AttrPtr bindHCur = attribMap->FindRef(Current);
			if(
				(bindHMax != nullptr && bindHMax->GetCurrentValue() > 0) &&
				(bindHCur != nullptr)) //note that current does not check 0. lmao. it used to.
			{
				float clamped = std::min(bindH->GetCurrentValue() + bindHCur->GetCurrentValue(), bindHMax->GetCurrentValue());
				bindHCur->SetCurrentValue(clamped);
			}
		}
	}

	//DurationAccumulator: holds value to set or add if, like damage, we are responsible in some way for managing the final value of the delta. With stuns, the abilities almost always set stun duration
	//though I think there's a good chance we may want to start changing that. we'll probably see a duration modifier added to allow resistances, but at the moment, I'm enjoying my simpler life.
	//DurationVar is the current canon duration. Condition tag is the condition name to set or unset.
	//fixed: used to handle things like the pinged state which should only ever refresh their duration, not stack.
	void HandleCondition(AttrMapPtr attrMap, Attr DurationVar, FGameplayTag ConditionTag, Attr DurationAccumulator = Attr::None, bool fixed = false)
	{
		AttrPtr ConditionDuration = attrMap->FindRef(DurationVar);
		
		if (ConditionDuration != nullptr && ConditionDuration->GetCurrentValue() > 0)
		{
			int Stunno = ConditionDuration->GetCurrentValue();
			int Safety = ConditionDuration->GetPriorValue();
			if (DurationAccumulator != Attr::None)
			{
				AttrPtr ConditionDurationAcc = attrMap->FindRef(DurationVar);
				if (ConditionDuration != nullptr)
				{
					double amount = fixed ? ConditionDurationAcc->GetBaseValue(): ConditionDuration->GetCurrentValue() + ConditionDurationAcc->GetCurrentValue();
					ConditionDuration->SetCurrentValue(amount);
				}
			}
			if (Stunno == 1 || (Safety > 0 && Stunno <= 0))
			{
				ADispatch->DispatchOwner->RemoveTagFromEntity(EntityKey, ConditionTag);
				ConditionDuration->SetCurrentValue(0.0);
			}
			else
			{
				if(Safety == 0 && Stunno > 0)
				{
					ADispatch->DispatchOwner->AddTagToEntity(EntityKey, ConditionTag);
					
				}
				else
				{
					ConditionDuration->SetCurrentValue(Stunno-1.0);
				}
			}
		}
		
	}

	//This can be set up to autowire, but I'm not sure we're keeping these mechanisms yet.
	//we can speed this up considerably by adding a get all attribs. not sure we wanna though until optimization demands it.
	void TICKLITE_Apply()
	{
		//factor the get attr down to the impl.
		bool TookDamage = false;
		float DamageTaken = 0.0;
		AttrMapPtr attrMap = ADispatch->GetAttribMap(EntityKey);
		if (attrMap == nullptr)
		{
			return;
		}
		AttrPtr PingedDuration = attrMap->FindRef(E_AttribKey::PingedDuration);
		this->RechargeClamp(attrMap, Attr::ManaRechargePerTick, Attr::MaxMana, Attr::Mana);
		if (PingedDuration && PingedDuration->GetCurrentValue() > 0.0)
		{
			//add any special effects while pinged here. we already set a tag, so don't handle audio here or stuff.
			//but if you need to manage some per tick state based on if the player IS pinged, here's good.
		}
		else
		{
			//if the player or enemy is NOT pinged, we'll recharge shields. Might be other stuff! few enemies have shields.
		this->RechargeClamp(attrMap, Attr::ShieldsRechargePerTick, Attr::MaxShields, Attr::Shields);
		}
		this->RechargeClamp(attrMap, Attr::HealthRechargePerTick, Attr::MaxHealth, Attr::Health);

		AttrPtr proposedDamage = attrMap->FindRef(Attr::ProposedDamage);
		if (proposedDamage != nullptr)
		{
			bool hadIFrames = false;
			AttrPtr iFrames = attrMap->FindRef(E_AttribKey::IFrames);
			if (iFrames != nullptr && iFrames->GetCurrentValue() > 0.f)
			{
				iFrames->SetCurrentValue(iFrames->GetCurrentValue() - 1);
				hadIFrames = true;
			}
			
			if (!hadIFrames)
			{
				float remainingDamageToApply = proposedDamage->GetCurrentValue();
				AttrPtr shieldAttrib = attrMap->FindRef(Attr::Shields);
				if (shieldAttrib != nullptr)
				{
					float originalShieldValue = shieldAttrib->GetCurrentValue();
					float newShieldsValue = std::max(0.f, originalShieldValue - remainingDamageToApply);
					shieldAttrib->SetCurrentValue(newShieldsValue);
					remainingDamageToApply = std::max(0.f, remainingDamageToApply - originalShieldValue);
				}
				
				AttrPtr healthAttr = attrMap->FindRef(Attr::Health);
				if (healthAttr != nullptr)
				{
					float originalHealth = healthAttr->GetCurrentValue();
					float newHealth = std::max(0.f, originalHealth - remainingDamageToApply);
					healthAttr->SetCurrentValue(newHealth);
					if (newHealth < originalHealth)
					{
						ADispatch->DispatchOwner->AddTagToEntity(EntityKey, GameplayEvent_Damaged);//change to a bounce tag effect? that's thistle dependent though...
						TookDamage = true;
						DamageTaken = originalHealth - newHealth;
					}
				}
			}
			proposedDamage->SetCurrentValue(0.f);
		}

		//stun handler - condition example
		HandleCondition(attrMap, Attr::StunDuration, TAG_Condition_Stun);
		HandleCondition(attrMap, Attr::PingedDuration, TAG_Condition_Pinged, TookDamage ? Attr::PerHitPingedDuration : Attr::None, true);
	}
	
	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		return false; //add check for aliveness of ya owner, factor that down.
	}

	void TICKLITE_OnExpiration()
	{
		//no op
	}
};

typedef Ticklites::Ticklite<TLEntityFinalTickResolver> EntityFinalTickResolver;
