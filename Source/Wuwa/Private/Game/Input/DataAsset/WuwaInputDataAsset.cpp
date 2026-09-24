// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "GameplayTags.h"

const UInputAction* UWuwaInputDataAsset::MatchInputActionWithTag(FGameplayTag tag)
{	
	if (InputDataAssetMap.IsEmpty())
	{
		return nullptr;
	}
	
	for (FInputDataAsset Input : InputDataAssetMap)
	{
		if (Input.InputTag == tag)
		{
			if (Input.InputAction != nullptr)
			{
				return  Input.InputAction;
			}
		}
	}
	
	return  nullptr;
}

FGameplayTag UWuwaInputDataAsset::MatchTagWithInputAction(UInputAction* inputAction )
{
	if (InputDataAssetMap.IsEmpty())
	{
		return FGameplayTag::EmptyTag;
	}
	
	for (FInputDataAsset Input : InputDataAssetMap)
	{
		if (Input.InputAction == inputAction)
		{
			return  Input.InputTag;
		}
	}
	return  FGameplayTag::EmptyTag;
}
