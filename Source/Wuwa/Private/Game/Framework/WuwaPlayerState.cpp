// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Framework/WuwaPlayerState.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"

AWuwaPlayerState::AWuwaPlayerState()
{
	//初始化
	WuwaAbilitySystemComponent = CreateDefaultSubobject<UWuwaAbilitySystemComponent>(TEXT("WuwaAbilitySystemComponent"));
	WuwaAbilitySystemComponent->SetIsReplicated(true);//设置网络同步
	WuwaAbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	WuwaAttributeSet = CreateDefaultSubobject<UWuwaAttributeSet>(TEXT("WuwaAttributeSet"));
	//设置网络同步速率,PS的默认比较长所以要自己设置
	SetNetUpdateFrequency(100.f);

}

UAbilitySystemComponent* AWuwaPlayerState::GetAbilitySystemComponent() const
{
	return WuwaAbilitySystemComponent;
}
