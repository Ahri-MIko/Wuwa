// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "WuwaEnhancedInputComponent.generated.h"

/**
 * 
 */

class UWuwaInputDataAsset;
UCLASS()
class WUWA_API UWuwaEnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	template<class Object,typename PressedFunc,typename HoldFunc,typename ReleaseFunc>
	void BindInputTagMapToAction(UWuwaInputDataAsset* InputDataAsset,Object* Obj,PressedFunc Pressed,HoldFunc Hold,ReleaseFunc Release);
};

template <class Object, typename PressedFunc, typename HoldFunc, typename ReleaseFunc>
void UWuwaEnhancedInputComponent::BindInputTagMapToAction(UWuwaInputDataAsset* InputDataAsset, Object* Obj,
	PressedFunc Pressed, HoldFunc Hold, ReleaseFunc Release)
{
	for (FInputDataAsset Input : InputDataAsset->InputDataAssetMap)
	{
		if (Input.InputTag.IsValid() && Input.InputAction)
		{
			if (Pressed)
			BindAction(Input.InputAction,ETriggerEvent::Started, Obj,Pressed,Input.InputTag);
			
			if (Hold)
			BindAction(Input.InputAction,ETriggerEvent::Completed, Obj,Release,Input.InputTag);
			
		}
	}
}
