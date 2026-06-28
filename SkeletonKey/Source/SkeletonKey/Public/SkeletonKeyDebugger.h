#pragma once
#include "TransformDispatch.h"

class SkeletonKeyDebugger
{
public:
	void Initialize(UTransformDispatch* InDispatchOwner);
	void Draw(float DeltaTime);
	void DrawAllKeyCarriersImGui();

private:
	void DrawImGuiDebug();
	void DrawAllTransformsImGui();

private:
	TWeakObjectPtr<UTransformDispatch> DispatchOwner;
};
