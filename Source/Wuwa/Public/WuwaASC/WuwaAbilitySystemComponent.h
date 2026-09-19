// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "WuwaAbilitySystemComponent.generated.h"

/**
 * 
 */

DECLARE_MULTICAST_DELEGATE_OneParam(FAttributeEffectApplied, const FGameplayEffectSpec&);

UCLASS()
class WUWA_API UWuwaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	void InitAbilitySystemCompoent();
	
	void EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle GameplayEffectHandle);
	
	FAttributeEffectApplied AttributeEffectAppliedDelegate;
	
	void InputWithTagPressed(const FGameplayTag& tag);
	void InputWithTagReleased(const FGameplayTag& tag);
	
};
