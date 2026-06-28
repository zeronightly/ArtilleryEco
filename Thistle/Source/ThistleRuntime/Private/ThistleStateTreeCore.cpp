#include "ThistleStateTreeCore.h"

#include "StateTreeConditionBase.h"
#include "ThistleBehavioralist.h"
#include "ThistleDispatch.h"
#include "Components/StateTreeComponentSchema.h"

EStateTreeRunStatus FStoreRelationship::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	auto ArtilleryDispatch = Context.GetWorld()->GetSubsystem<UArtilleryDispatch>();
	const IdentPtr V = ArtilleryDispatch->GetOrAddIdent(InstanceData.SourceKey, InstanceData.Relationship);
	if (V)
	{
		V->SetCurrentValue(InstanceData.UpdateToRelatedKey);
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FSetTagOfKey::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto ArtilleryDispatch = Context.GetWorld()->GetSubsystem<UArtilleryDispatch>();
	//This seems unsafe.
	ArtilleryDispatch->AddTagToEntity(InstanceData.KeyOf, InstanceData.Tag);
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FRemoveTagFromKey::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto ArtilleryDispatch = Context.GetWorld()->GetSubsystem<UArtilleryDispatch>();
	//This seems unsafe.
	ArtilleryDispatch->RemoveTagFromEntity(InstanceData.KeyOf, InstanceData.Tag);
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FStoreToAttribute::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	auto ArtilleryDispatch = Context.GetWorld()->GetSubsystem<UArtilleryDispatch>();
	AttrPtr V = ArtilleryDispatch->GetAttrib(InstanceData.KeyOf, InstanceData.AttributeName);
	if (V)
	{
		V->SetCurrentValue(InstanceData.Value);
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}
