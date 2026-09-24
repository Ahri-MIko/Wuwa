// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"

#include "Game/Input/WuwaInputTypes.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"

UWuwaGameplayAbilityBase* UWuwaAbilitySystemComponent::GetAnimatingWuwaAbility() const
{
	return Cast<UWuwaGameplayAbilityBase>(GetAnimatingAbility());
}

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

void UWuwaAbilitySystemComponent::ProcessInputEvent(const FWuwaInputEvent& InputEvent)
{		
	if (InputEvent.Phase == EWuwaInputPhase::Pressed)
		InputWithTagPressed(InputEvent.InputTag);
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


