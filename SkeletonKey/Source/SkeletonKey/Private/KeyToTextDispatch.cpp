#include "KeyToTextDispatch.h"

#include "TransformDispatch.h"

UKeyToTextDispatch::UKeyToTextDispatch()
{
}

UKeyToTextDispatch::~UKeyToTextDispatch()
{
}




void UKeyToTextDispatch::DeferDelete(FSkeletonKey Target) 
{
	OverkillFront.emplace(Target);
}

void UKeyToTextDispatch::ProcessDeletes()
{
	if (OverkillFront.size() > 0)
	{
		OverkillFront.visit_all(
		[this](auto& a) { this->DeregisterKey(a); }
		);
	}
}

bool UKeyToTextDispatch::RegisterConstBlock(FSkeletonKey Target, TSharedPtr<FString> Block) 
{
	if (!Blocks.contains(Target))
	{
		Blocks.emplace(Target, Block);
		return true;
	}
	return false;
}

bool UKeyToTextDispatch::RegisterFName(FSkeletonKey Target, FName Name) 
{
	if (!Names.contains(Target))
	{
		Names.emplace(Target, Name);
		return true;
	}
	return false;
}

const FName UKeyToTextDispatch::GetFName(FSkeletonKey Target) const
{
	if (Names.contains(Target))
	{
		Names.visit(Target, [](auto& a) { return a.second; });
		
	}
	return FName();
}


//didn't really wanna use shared ptrs here but this is how we do it everywhere else.
const TSharedPtr<FString> UKeyToTextDispatch::GetConstBlock(FSkeletonKey Target) const
{
	if (Blocks.contains(Target))
	{
		Blocks.visit(Target, [](auto& a) { return a.second; });
		
	}
	return nullptr;
}

void UKeyToTextDispatch::DeregisterKey(FSkeletonKey Target)
{
	Blocks.erase(Target);
	Names.erase(Target);
}

//Hi! You might notice something REALLY ODD! We don't clear the blocks or names until initialization! 
//This means that after a world deinitialized but before it is destroyed, you can still look up values stored here
//for debug purposes. it's hard to know if this'll be useful, but it's something we can do.
void UKeyToTextDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TransformDispatch = Collection.InitializeDependency<UTransformDispatch>();
	Blocks.clear();
	Names.clear();
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UKeyToTextDispatch::Deinitialize()
{
	Super::Deinitialize();
}

TStatId UKeyToTextDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKeyToTextDispatch, STATGROUP_Tickables);
}

bool UKeyToTextDispatch::RegistrationImplementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Names Subsystem: Online"));
	return true;
}

void UKeyToTextDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UKeyToTextDispatch::PostInitialize()
{
	Super::PostInitialize();
}

void UKeyToTextDispatch::PostLoad()
{
	Super::PostLoad();
}

void UKeyToTextDispatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
