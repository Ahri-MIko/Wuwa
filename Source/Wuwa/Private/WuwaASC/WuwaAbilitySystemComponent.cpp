// Fill out your copyright notice in the Description page of Project Settings.

#include "WuwaASC/WuwaAbilitySystemComponent.h"
#include "Tools/DebugHelper.h"

void UWuwaAbilitySystemComponent::InitAbilitySystemCompoent()
{
	//仅仅在服务器生效
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this,&UWuwaAbilitySystemComponent::EffectApplied);
}

void UWuwaAbilitySystemComponent::EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec,
	FActiveGameplayEffectHandle GameplayEffectHandle)
{
	
	AttributeEffectAppliedDelegate.Broadcast(EffectSpec);
}

void UWuwaAbilitySystemComponent::InputWithTagPressed(const FGameplayTag& tag)
{
	if (tag.IsValid())
	{
		for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(tag))
			{
				AbilitySpecInputPressed(AbilitySpec);
				if (!AbilitySpec.IsActive())
				{
					TryActivateAbility(AbilitySpec.Handle);
				}
			}
		}
	}
}

void UWuwaAbilitySystemComponent::InputWithTagReleased(const FGameplayTag& tag)
{
	
}


