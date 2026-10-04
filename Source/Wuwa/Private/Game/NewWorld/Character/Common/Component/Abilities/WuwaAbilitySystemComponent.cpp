// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"

#include "Game/Input/WuwaInputTypes.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

UWuwaGameplayAbilityBase* UWuwaAbilitySystemComponent::GetAnimatingWuwaAbility() const
{
	return Cast<UWuwaGameplayAbilityBase>(GetAnimatingAbility());
}

//当有 GameplayEffect 被成功应用到自己身上时，就调用 EffectApplied
void UWuwaAbilitySystemComponent::InitAbilitySystemCompoent()
{
	//仅仅在服务器生效
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this,&UWuwaAbilitySystemComponent::EffectApplied);
}

void UWuwaAbilitySystemComponent::EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec,FActiveGameplayEffectHandle GameplayEffectHandle)
{
	AttributeEffectAppliedDelegate.Broadcast(EffectSpec);
}

#pragma region GA

//根据这个InputTag返回ASC身上的可用来调用的GA句柄
TArray<FGameplayAbilitySpecHandle>UWuwaAbilitySystemComponent::FindAbilityHandlesByInputTag(FGameplayTag InputTag)
{
	return FindAbilityHandlesByAbilityTag(InputTag);
}

TArray<FGameplayAbilitySpecHandle> UWuwaAbilitySystemComponent::FindAbilityHandlesByAbilityTag(FGameplayTag InputTag)
{
	//每当基于了ASC能力的时候都会创建这样的一个编号,表示在当前的ASC里面的这个GA,可以通过这个Handle调用这个GA
	TArray<FGameplayAbilitySpecHandle> Handles;

	if (!InputTag.IsValid()){return Handles;}

	//锁定防止出现异常,因为在此期间可能有的能力已经发生变动了
	FScopedAbilityListLock AbilityListLock(*this);

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability&& !Spec.PendingRemove&& !Spec.RemoveAfterActivation&& Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			Handles.Add(Spec.Handle);
		}
	}

	return Handles;
}

//一个ASC里面是否有这样一个GA并且判断是否这个GA有有对应传入的Tag
bool UWuwaAbilitySystemComponent::HasActiveSkillAbilityTag(UWuwaGameplayAbilityBase* Ability, FGameplayTag AbilityTag) const
{
	if (!IsValid(Ability) || !AbilityTag.IsValid() || !Ability->IsSkillExecutionFor(GetAvatarActor()) || Ability->GetAbilitySystemComponentFromActorInfo() != this) return false;
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle());
	return Spec && !Spec->PendingRemove && !Spec->RemoveAfterActivation && Spec->IsActive()
		&& Spec->GetDynamicSpecSourceTags().HasTagExact(AbilityTag);
}

//判断这个 ASC 当前身上拥有的 GameplayTag，是否满足给定的 Tag 查询条件
bool UWuwaAbilitySystemComponent::MatchesOwnedTagQuery(const FGameplayTagQuery& Query) const
{
	if (Query.IsEmpty()) return true;
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);
	return Query.Matches(OwnedTags);
}

//检查当前技能是否激活,是不是可激活的技能
bool UWuwaAbilitySystemComponent::IsSpecAvailableForActivation(FGameplayAbilitySpecHandle AbilityHandle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	const UWuwaGameplayAbilityBase* Ability = GetAbilityForInput(AbilityHandle);
	return Spec && IsValid(Ability) && (!Spec->IsActive()|| (Ability->bIsMainSkill && Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution));
}

UWuwaGameplayAbilityBase* UWuwaAbilitySystemComponent::GetAbilityForInput(FGameplayAbilitySpecHandle AbilityHandle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	return Spec && !Spec->PendingRemove && !Spec->RemoveAfterActivation ? Cast<UWuwaGameplayAbilityBase>(Spec->Ability) : nullptr;
}


//其实就是返回角色的Avatar
AWuwaCharacter* UWuwaAbilitySystemComponent::GetInputAvatar() const
{
	return Cast<AWuwaCharacter>(GetAvatarActor());
}

//其实就是返回世界时间
double UWuwaAbilitySystemComponent::GetInputTimeSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
}

//请求激活AbilityHandle的这个GA
EWuwaAbilityRequestResult UWuwaAbilitySystemComponent::RequestAbilityActivation(FGameplayAbilitySpecHandle AbilityHandle)
{
	FScopedAbilityListLock AbilityListLock(*this);
	FGameplayAbilitySpec* Spec =FindAbilitySpecFromHandle(AbilityHandle);

	if (!Spec|| !Spec->Ability|| Spec->PendingRemove|| Spec->RemoveAfterActivation){return EWuwaAbilityRequestResult::InvalidHandle;}
	
	if (Spec->IsActive() && !IsSpecAvailableForActivation(AbilityHandle)){return EWuwaAbilityRequestResult::AlreadyActive;}

	return TryActivateAbility(AbilityHandle)? EWuwaAbilityRequestResult::ActivationRequested: EWuwaAbilityRequestResult::Rejected;
}

#pragma endregion

