// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "WuwaGameplayAbilityBase.generated.h"

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|DynamicTag")
	FGameplayTag OriginalTag;
};
