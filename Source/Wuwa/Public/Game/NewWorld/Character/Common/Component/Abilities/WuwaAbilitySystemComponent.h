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
class AWuwaCharacter;
DECLARE_MULTICAST_DELEGATE_OneParam(FAttributeEffectApplied, const FGameplayEffectSpec&);

UENUM(BlueprintType)
enum class EWuwaAbilityRequestResult : uint8
{
	InvalidHandle,        // 这个技能已不存在或正在移除
	AlreadyActive,        // 这个技能已经在执行
	ActivationRequested,  // GAS 接受了激活请求
	Rejected              // GAS 拒绝了激活请求
};

UCLASS()
class WUWA_API UWuwaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	
	/** 当前负责 ASC 蒙太奇播放的 GA 实例；没有播放归属时返回 nullptr。供脚本通知查询。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation")
	UWuwaGameplayAbilityBase* GetAnimatingWuwaAbility() const;

	#pragma region EffectApplied
	//当有 GameplayEffect 被成功应用到自己身上时执行这个函数
	void InitAbilitySystemCompoent();
	//收到影响触发的回调
	void EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle GameplayEffectHandle);
	//广播函数
	FAttributeEffectApplied AttributeEffectAppliedDelegate;
	#pragma endregion
	
	//C++层面基本不作逻辑判断,具体业务逻辑都交给热更脚本书写
	#pragma region GA
	/** 只查找候选，不激活，也不决定连段。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	TArray<FGameplayAbilitySpecHandle> FindAbilityHandlesByInputTag(FGameplayTag InputTag);//其实查询出来应该只有一个因为当前一个GA对应一个标签

	/** 按技能身份精确查找；输入语义到技能身份的转换由脚本完成。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Input")
	TArray<FGameplayAbilitySpecHandle> FindAbilityHandlesByAbilityTag(FGameplayTag AbilityTag);
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Input")
	bool HasActiveSkillAbilityTag(UWuwaGameplayAbilityBase* Ability, FGameplayTag AbilityTag) const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Input")
	bool MatchesOwnedTagQuery(const FGameplayTagQuery& Query) const;
	/** 无效/未注册的属性返回 false，不能当作数值零参与条件判断。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Attribute")
	bool TryGetAttributeValue(FGameplayAttribute Attribute, float& Value) const;

	/** 输入解析所需的配置；只返回仍有效的已授予能力，不创建执行实例。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat")
	UWuwaGameplayAbilityBase* GetAbilityForInput(FGameplayAbilitySpecHandle AbilityHandle) const;
	/** 只检查 Spec/实例策略。技能门槛由脚本判断，GAS 条件在提交时检查。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat")
	bool CanRequestAbilityFromInput(FGameplayAbilitySpecHandle AbilityHandle) const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	AWuwaCharacter* GetInputAvatar() const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	double GetInputTimeSeconds() const;

	/** 请求执行。主技能的 InstancedPerExecution 允许同一 Spec 在接招窗口重开，由 Skill 结束旧实例。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	EWuwaAbilityRequestResult RequestAbilityActivation(FGameplayAbilitySpecHandle AbilityHandle);
	#pragma endregion
};
