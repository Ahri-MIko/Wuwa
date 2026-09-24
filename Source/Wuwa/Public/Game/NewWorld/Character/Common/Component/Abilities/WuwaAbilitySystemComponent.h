// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "WuwaAbilitySystemComponent.generated.h"

/**
 * 
 */

struct FWuwaInputEvent;
class UWuwaGameplayAbilityBase;
DECLARE_MULTICAST_DELEGATE_OneParam(FAttributeEffectApplied, const FGameplayEffectSpec&);

UCLASS()
class WUWA_API UWuwaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	/** 当前负责 ASC 蒙太奇播放的 GA 实例；没有播放归属时返回 nullptr。供脚本通知查询。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation")
	UWuwaGameplayAbilityBase* GetAnimatingWuwaAbility() const;

	void InitAbilitySystemCompoent();
	
	void EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle GameplayEffectHandle);
	
	FAttributeEffectApplied AttributeEffectAppliedDelegate;
	
	//处理输入事件
	void ProcessInputEvent(const FWuwaInputEvent& InputEvent);
	void InputWithTagPressed(const FGameplayTag& tag);
	void InputWithTagReleased(const FGameplayTag& tag);
	
	
	
};
