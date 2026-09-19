// Fill out your copyright notice in the Description page of Project Settings.


#include "WuwaASC/WuwaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"




UWuwaAttributeSet::UWuwaAttributeSet()
{
	InitHealth(100);
	InitMaxHealth(100);
}

//这里面的参数是写死的,不要随便改名字
void UWuwaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//注册才能被网络同步
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	

}

void UWuwaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Health, OldHealth);
}

void UWuwaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxHealth, OldMaxHealth);
}

void UWuwaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UWuwaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
}
