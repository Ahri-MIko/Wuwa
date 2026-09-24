// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Effect/WuwaEffectActor.h"
#include "Core/Utilities/DebugHelper.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"




void AWuwaEffectActor::ApplyEffect(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass)
{
	checkf(EffectClass, TEXT("EffectClass cannot be null"));

	if (!IsValid(Target))
	{
		Debug::Print("Target AActor is invalid when granting Effect");
		return;
	}

	UAbilitySystemComponent* TargetASC = nullptr;
	if (IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Target))
	{
		TargetASC = AbilityInterface->GetAbilitySystemComponent();
	}

	if (TargetASC == nullptr)
	{
		Debug::Print("There is no ASC in Target AActor when granting Effect");
		return;
	}

	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	//Source Object 
	EffectContextHandle.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(
		EffectClass,
		1.0f,
		EffectContextHandle);

	if (!EffectSpecHandle.IsValid())
	{
		Debug::Print("Failed to create GameplayEffect spec");
		return;
	}
	//用target将效果赋给自己
	TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
}
