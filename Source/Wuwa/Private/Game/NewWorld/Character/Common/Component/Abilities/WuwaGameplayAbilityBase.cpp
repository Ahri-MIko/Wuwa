// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

FWuwaPlayerInputState UWuwaGameplayAbilityBase::GetPlayerInputState() const
{
	// 不缓存角色指针，也不使用 GetPlayerCharacter(0)：始终读取这个 GA 自己的 Avatar。
	const AWuwaCharacter* Character = CurrentActorInfo
		? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	return IsValid(Character) ? Character->GetPlayerInputState() : FWuwaPlayerInputState{};
}

bool UWuwaGameplayAbilityBase::StopMontageForMovement(float BlendOutTime)
{
	if (!CurrentActorInfo || !FMath::IsFinite(BlendOutTime) || BlendOutTime < 0.f
		|| !GetPlayerInputState().bHasMoveInput)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance();
	UAnimMontage* Montage = GetCurrentMontage();
	if (!IsValid(ASC) || !IsValid(AnimInstance) || !IsValid(Montage)
		|| ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != Montage)
	{
		return false;
	}

	FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
	if (!Instance)
	{
		return false;
	}

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
bool UWuwaGameplayAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}
