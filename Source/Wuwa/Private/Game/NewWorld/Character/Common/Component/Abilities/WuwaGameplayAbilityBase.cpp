// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

bool UWuwaGameplayAbilityBase::IsSkillExecutionActive() const
{
	return IsInstantiated() && IsActive() && !bIsAbilityEnding;
}

bool UWuwaGameplayAbilityBase::IsSkillExecutionFor(AActor* ExpectedAvatar) const
{
	return IsValid(ExpectedAvatar) && IsSkillExecutionActive() && CurrentActorInfo
		&& CurrentActorInfo->AvatarActor.Get() == ExpectedAvatar;
}

bool UWuwaGameplayAbilityBase::CanEndSkillExecutionNow() const
{
	return IsSkillExecutionActive() && ScopeLockCount == 0;
}

bool UWuwaGameplayAbilityBase::TryEndSkillExecution(int32 ExpectedHandle)
{
	if (ExpectedHandle <= 0 || SkillLeaseHandle != ExpectedHandle || !CanEndSkillExecutionNow()) return false;
	// 与原作 K2_EndAbility 的让位语义一致，不依赖 CancelAbilitiesWithTag 或 CanBeCanceled。
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	return SkillLeaseHandle != ExpectedHandle;
}

bool UWuwaGameplayAbilityBase::SetSkillAcceptInput(bool bAcceptInput)
{
	return SkillLeaseOwner.IsValid() && SkillLeaseHandle > 0
		&& SkillLeaseOwner->SetSkillAcceptInput(SkillLeaseHandle, bAcceptInput);
}

bool UWuwaGameplayAbilityBase::SetSkillReadyEnd(bool bReadyEnd)
{
	return SkillLeaseOwner.IsValid() && SkillLeaseHandle > 0
		&& SkillLeaseOwner->SetMainSkillReadyEnd(SkillLeaseHandle, bReadyEnd);
}

bool UWuwaGameplayAbilityBase::CallAnimBreakPoint()
{
	return IsSkillExecutionActive() && SkillLeaseOwner.IsValid() && SkillLeaseHandle > 0
		&& SkillLeaseOwner->CallAnimBreakPoint(SkillLeaseHandle);
}

bool UWuwaGameplayAbilityBase::ClearBufferedInput(FGameplayTag InputTag)
{
	return IsSkillExecutionActive() && SkillLeaseOwner.IsValid() && SkillLeaseHandle > 0
		&& SkillLeaseOwner->RequestInputCacheClear(SkillLeaseHandle, InputTag);
}

FWuwaPlayerInputState UWuwaGameplayAbilityBase::GetPlayerInputState() const
{
	// 不缓存角色指针，也不使用 GetPlayerCharacter(0)：始终读取这个 GA 自己的 Avatar。
	const AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	return IsValid(Character) ? Character->GetPlayerInputState() : FWuwaPlayerInputState{};
}

//停止当前正在执行的蒙太奇动画并先停止根运动防止在混出的时候占据CMC的移动计算
bool UWuwaGameplayAbilityBase::StopMontageForMovement(float BlendOutTime)
{
	if (!CurrentActorInfo || !FMath::IsFinite(BlendOutTime) || BlendOutTime < 0.f|| !GetPlayerInputState().bHasMoveInput){return false;}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance();
	UAnimMontage* Montage = GetCurrentMontage();
	if (!IsValid(ASC) || !IsValid(AnimInstance) || !IsValid(Montage)|| ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != Montage){return false;}

	FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
	if (!Instance){return false;}

	// 必须在 Stop 前取得实例并禁用：Everything 模式混出末帧的零权重根运动
	// 仍可能覆盖 CMC 的速度。只停止这次播放的提取，不清空其他动画的根运动。
	Instance->PushDisableRootMotion();
	// 此实例已确定退出，禁用保持到实例销毁；GA End 不应提前 Pop 恢复提取。
	// 经由 ASC 停止以保留 GAS 的播放归属及停止同步流程。
	// Stop 可同步触发回调并更换播放实例，因此之后不再访问 Instance。
	ASC->CurrentMontageStop(BlendOutTime);
	return true;
}


//检查是否能够激活
bool UWuwaGameplayAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaSkillBridgeComponent* Skills = IsValid(Character) ? Character->SkillComponent.Get() : nullptr;
	if (bIsMainSkill && !IsValid(Skills) || !Skills->CanBeginSkill(const_cast<UWuwaGameplayAbilityBase*>(this)))
	{
		return false;
	}

	if (bOverridesMoveState)
	{
		// CDO 查询使用本次传入的 Avatar，不依赖实例 CurrentActorInfo。
		// 在 GAS PreActivate 的标签/取消其他能力等副作用前拒绝已知不可接受的动作。
		const UWuwaUnifiedStateBridgeComponent* State = IsValid(Character) ? Character->UnifiedStateComponent.Get() : nullptr;
		UObject* ReleasingSource = bIsMainSkill ? Skills->GetCurrentSkillData().ActiveAbility.Get() : nullptr;
		if (!IsValid(State)|| !State->CanAcquireMoveStateAfterRelease(ActionMoveState, ActionMoveStatePriority, ReleasingSource))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UWuwaGameplayAbilityBase::PreActivate(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	if (!IsInstantiated())
	{
		Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
		return;
	}

	const uint64 ActivationSerial = ++MoveStateActivationSerial;
	bMoveStateLeaseRequiredForActivation = bOverridesMoveState;
	bSkillLeaseRequiredForActivation = bIsMainSkill;
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	// Super 建立 GAS 上下文/活动计数；其 Tag 和激活通知可以重入并结束本次能力。
	if (ActivationSerial != MoveStateActivationSerial || !IsActive() || bIsAbilityEnding)
	{
		return;
	}

	if (bSkillLeaseRequiredForActivation)
	{
		const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		UWuwaSkillBridgeComponent* Skills = IsValid(Character) ? Character->SkillComponent.Get() : nullptr;
		if (!IsValid(Skills)) return;
		const TWeakObjectPtr<UWuwaSkillBridgeComponent> RequestedSkills(Skills);
		const int32 NewSkillHandle = Skills->TryBeginSkill(this);
		if (ActivationSerial != MoveStateActivationSerial || !IsSkillExecutionActive())
		{
			if (NewSkillHandle != 0 && RequestedSkills.IsValid()) RequestedSkills->EndSkill(NewSkillHandle);
			return;
		}
		SkillLeaseOwner = RequestedSkills;
		SkillLeaseHandle = NewSkillHandle;
		if (NewSkillHandle == 0) return;
	}

	const TWeakObjectPtr<UWuwaUnifiedStateBridgeComponent> PreviousOwner = MoveStateLeaseOwner;
	const int32 PreviousHandle = MoveStateLeaseHandle;
	MoveStateLeaseOwner.Reset();
	MoveStateLeaseHandle = 0;
	if (PreviousHandle != 0 && PreviousOwner.IsValid())
	{
		PreviousOwner->ReleaseMoveState(PreviousHandle);
	}
	if (ActivationSerial != MoveStateActivationSerial || !IsActive() || bIsAbilityEnding|| !bMoveStateLeaseRequiredForActivation)
	{
		return;
	}

	const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	UWuwaUnifiedStateBridgeComponent* State = IsValid(Character) ? Character->UnifiedStateComponent.Get() : nullptr;
	if (!IsValid(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Action state requested without an initialized UnifiedState component."), *GetName());
		return;
	}

	const TWeakObjectPtr<UWuwaUnifiedStateBridgeComponent> RequestedOwner(State);
	// CanActivate 的纯查询不是预留；此处由同一脚本规则复检竞争后的实际状态。
	const int32 NewHandle = State->AcquireMoveState(this, ActionMoveState, ActionMoveStatePriority);
	if (ActivationSerial != MoveStateActivationSerial || !IsActive() || bIsAbilityEnding)
	{
		// Acquire 广播时本 GA 可能已经结束/重新激活；不能把旧结果写入新一轮字段。
		if (NewHandle != 0 && RequestedOwner.IsValid())
		{
			RequestedOwner->ReleaseMoveState(NewHandle);
		}
		return;
	}

	MoveStateLeaseOwner = RequestedOwner;
	MoveStateLeaseHandle = NewHandle;
}

void UWuwaGameplayAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// CallActivateAbility 总会在 PreActivate 之后调用 ActivateAbility。
	// 只在这里拦截本次占用失败，避免已拒绝的 Dash 仍进入蓝图播放蒙太奇。
	if ((bIsMainSkill && (!IsInstantiated() || (bSkillLeaseRequiredForActivation
		&& (SkillLeaseHandle == 0 || !SkillLeaseOwner.IsValid() || !IsActive()))))
		|| (bOverridesMoveState && (!IsInstantiated() || (bMoveStateLeaseRequiredForActivation
		&& (MoveStateLeaseHandle == 0 || !MoveStateLeaseOwner.IsValid() || !IsActive())))))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UWuwaGameplayAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsInstantiated() || (MoveStateLeaseHandle == 0 && SkillLeaseHandle == 0))
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		// 保持 GAS 的延迟结束语义；真正退出时才转交并释放句柄。
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	const TWeakObjectPtr<UWuwaUnifiedStateBridgeComponent> EndingOwner = MoveStateLeaseOwner;
	const int32 EndingHandle = MoveStateLeaseHandle;
	const TWeakObjectPtr<UWuwaSkillBridgeComponent> EndingSkills = SkillLeaseOwner;
	const int32 EndingSkillHandle = SkillLeaseHandle;
	MoveStateLeaseOwner.Reset();
	MoveStateLeaseHandle = 0;
	SkillLeaseOwner.Reset();
	SkillLeaseHandle = 0;

	const TWeakObjectPtr<UAbilitySystemComponent> ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const TWeakObjectPtr<UAnimInstance> AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	const TWeakObjectPtr<UAnimMontage> EndingMontage = GetCurrentMontage();
	int32 EndingMontageInstanceId = INDEX_NONE;
	if (ASC.IsValid() && AnimInstance.IsValid() && EndingMontage.IsValid()
		&& ASC->GetAnimatingAbility() == this && ASC->GetCurrentMontage() == EndingMontage.Get())
	{
		if (const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(EndingMontage.Get()))
		{
			EndingMontageInstanceId = Instance->GetInstanceID();
		}
	}

	// PlayMontageAndWait 等任务在 Super 的清理中请求停止，Notify/Ended 可同步重入。
	// 原句柄已移出成员，回调中新激活的动作只会保存/释放它自己的句柄。
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 若未来某个显式配置的动作没有 StopWhenAbilityEnds，补停仍属本次的播放。
	// 新能力/同资产的新播放实例均不能被旧能力的结束逻辑停止。
	if (EndingMontageInstanceId != INDEX_NONE && ASC.IsValid() && AnimInstance.IsValid() && EndingMontage.IsValid()
		&& ASC->GetAnimatingAbility() == nullptr && ASC->GetCurrentMontage() == EndingMontage.Get())
	{
		const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(EndingMontage.Get());
		if (Instance && Instance->GetInstanceID() == EndingMontageInstanceId)
		{
			ASC->CurrentMontageStop();
		}
	}

	// 这里只保证停止请求/同步清理先于状态交接；Queued NotifyEnd 仍可能稍后执行。
	// 不撤销 StopMontageForMovement 对旧实例的根运动禁用，也不改写角色速度。
	if (EndingOwner.IsValid())
	{
		EndingOwner->ReleaseMoveState(EndingHandle);
	}
	if (EndingSkills.IsValid())
	{
		EndingSkills->EndSkill(EndingSkillHandle);
	}
}
