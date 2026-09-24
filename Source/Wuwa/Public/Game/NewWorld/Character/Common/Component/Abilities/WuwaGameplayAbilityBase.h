// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
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

	/**
	 * 在 GA 实例中读取当前 Avatar 的移动输入，无角色上下文时返回零值。
	 * 本地输入不自动复制。CanActivateAbility 中应通过其 ActorInfo 参数查询角色，
	 * 不依赖这个实例入口；需要持续输入时重新读取，不缓存返回值。
	 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;

	
	//已弃用
	/**
	 * 仅在移动取消已经获准时调用；窗口判定仍由 GA 负责。
	 * 有移动输入时，先禁用本 GA 正在播放的蒙太奇实例的根运动，再开始姿势混出。
	 * 成功返回 true 后由调用方结束 GA；无输入或播放归属不匹配时不作任何修改。
	 * 保留 Root Motion from Everything，其他动画（如 Run_End）不受影响。
	 * 当前供本地验证；额外的根运动禁用状态不自动参与网络预测/复制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement", meta = (ClampMin = "0.0", AdvancedDisplay = "BlendOutTime"))
	bool StopMontageForMovement(float BlendOutTime = 0.1f);

	//override
	virtual  bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
};
