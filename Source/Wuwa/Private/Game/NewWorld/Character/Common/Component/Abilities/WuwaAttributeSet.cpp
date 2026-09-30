// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
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
//客户端上的 GAS 系统正确感知并处理这个值
void UWuwaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Health, OldHealth);
}

void UWuwaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxHealth, OldMaxHealth);
}

/*举个例子，MaxHealth = 100，Health = 100：

受到 120 点伤害（Instant GE，Health −120）
BaseValue = −20，CurrentValue 被 Clamp 为 0。看起来一切正常。
治疗 50 点（Instant GE，Health +50）
BaseValue = −20 + 50 = 30，CurrentValue = 30。*/
//在真正设定数值之前会执行这个函数,上面的例子说明了可能会失效所以要在下面的函数改
void UWuwaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

/*SetHealth 是由 ATTRIBUTE_ACCESSORS 生成的，它会通过 ASC 去设置 BaseValue，所以这样就把根源修正了。PreAttributeChange 里的 Clamp 可以保留，用来处理 Duration/Infinite GE 通过 Modifier 影响 CurrentValue 的情况。*/
void UWuwaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
}
