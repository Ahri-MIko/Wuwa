// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaGameplayAbilityBase.generated.h"

class UWuwaUnifiedStateBridgeComponent;
class UWuwaSkillBridgeComponent;

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	//初始标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|DynamicTag")
	FGameplayTag OriginalTag;

	/** 本项目主动动作默认属于主动技能；被动/辅助 GA 应关闭。与移动占用独立。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill")
	bool bIsMainSkill = true;

	//打断等级
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill", meta = (EditCondition = "bIsMainSkill", ClampMin = "0", ClampMax = "255"))
	int32 InterruptLevel = 100;

	//被打断等级
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill", meta = (EditCondition = "bIsMainSkill"))
	EWuwaSkillOverrideType SkillOverrideType = EWuwaSkillOverrideType::None;

	//申请为主技能时返回的句柄
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	int32 GetSkillHandle() const { return SkillLeaseHandle; }

	/** 只作用于本次 GA；动画通知也通过带归属的技能接口更新权限。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Skill")
	bool SetSkillAcceptInput(bool bAcceptInput);
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Skill")
	bool SetSkillReadyEnd(bool bReadyEnd);

	/** 只检查缓存，不修改同级接招/让位权限；返回值表示请求被接受。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Input")
	bool CallAnimBreakPoint();
	/** 空 Tag 清空预输入；非空 Tag 只清对应指令，不重置物理/语义按键状态。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Input")
	bool ClearBufferedInput(FGameplayTag InputTag);

	
	/** 脚本管理器使用的 GAS 执行接口；不包含技能优先级规则。 */
	//是否正在被执行
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool IsSkillExecutionActive() const;
	//是否正在为输入的Avatar执行
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool IsSkillExecutionFor(AActor* ExpectedAvatar) const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	//是否可以现在就结束,前提是当前的GA正在执行,并且没有被其他的程序上锁
	bool CanEndSkillExecutionNow() const;
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Skill")
	bool TryEndSkillExecution(int32 ExpectedHandle);

	/** 显式配置的动作才取得统一移动状态；普通能力不会自动覆盖步态。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|State")
	bool bOverridesMoveState = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|State", meta=(EditCondition="bOverridesMoveState"))
	EWuwaMoveState ActionMoveState = EWuwaMoveState::Other;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|State", meta=(EditCondition="bOverridesMoveState"))
	int32 ActionMoveStatePriority = 100;

	/**
	 * 在 GA 实例中读取当前 Avatar 的移动输入，无角色上下文时返回零值。
	 * 本地输入不自动复制。CanActivateAbility 中应通过其 ActorInfo 参数查询角色，
	 * 不依赖这个实例入口；需要持续输入时重新读取，不缓存返回值。
	 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;
	
	/**
	 * 仅在移动取消已经获准时调用；窗口判定仍由 GA 负责。
	 * 有移动输入时，先禁用本 GA 正在播放的蒙太奇实例的根运动，再开始姿势混出。
	 * 成功返回 true 后由调用方结束 GA；无输入或播放归属不匹配时不作任何修改。
	 * 保留 Root Motion from Everything，其他动画（如 Run_End）不受影响。
	 * 当前供本地验证；额外的根运动禁用状态不自动参与网络预测/复制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement", meta = (ClampMin = "0.0", AdvancedDisplay = "BlendOutTime"))
	bool StopMontageForMovement(float BlendOutTime = 0.1f);

	
	/** 先纯查询当前 Avatar 的动作占用规则，成功后保留 GAS 自身的激活检查。 */
	virtual  bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	TWeakObjectPtr<UWuwaSkillBridgeComponent> SkillLeaseOwner;
	int32 SkillLeaseHandle = 0;
	bool bSkillLeaseRequiredForActivation = false;
	TWeakObjectPtr<UWuwaUnifiedStateBridgeComponent> MoveStateLeaseOwner;
	int32 MoveStateLeaseHandle = 0;
	uint64 MoveStateActivationSerial = 0;
	bool bMoveStateLeaseRequiredForActivation = false;
};
