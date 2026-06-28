#pragma once
#include "UObject/WeakObjectPtrTemplates.h"

class UBarrageDispatch;

class BarrageDebugger
{
public:
	void Draw(float DeltaTime);
	void Initialize(UBarrageDispatch* InDispatchOwner);

private:
	void DrawImGuiDebug();
	void DrawAllBarrageImGui();
	void DrawBarrageToJoltKeyMapping();

private:
	TWeakObjectPtr<UBarrageDispatch> DispatchOwner;
};
