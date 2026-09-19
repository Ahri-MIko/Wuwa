// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "WuwaInputDataAsset.generated.h"


USTRUCT(BlueprintType)
struct FInputDataAsset
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	const class UInputAction* InputAction = nullptr;
	
	// 玩家想做什么
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	FGameplayTag InputTag;

	// 交给哪个系统
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	FGameplayTag RouteTag;
	
	bool IsConfigured() const
	{
		return InputAction != nullptr
			&& InputTag.IsValid()
			&& RouteTag.IsValid();
	}
};

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaInputDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	const UInputAction* MatchInputActionWithTag( FGameplayTag tag );
	
	FGameplayTag MatchTagWithInputAction( UInputAction* inputAction );
	
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	TArray<FInputDataAsset> InputDataAssetMap;
};
