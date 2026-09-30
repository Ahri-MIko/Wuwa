// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Framework/WuwaPlayerState.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/UI/WuwaHUD.h"
#include "Game/UI/WuwaWidgetController.h"

class AWuwaPlayerController;
class AWuwaPlayerState;
class UWuwaGameplayAbilityBase;
// Sets default values
AWuwaCharactorBase::AWuwaCharactorBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	WuwaMovementComponent = Cast<UWuwaMovementComponent>(GetCharacterMovement());//重置MovementComponent组件
}


#pragma region Replicate
//初始化ASCInfo用的
void AWuwaCharactorBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitGasInfoandHUD();
	InitInitialAbilities();
}

void AWuwaCharactorBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitGasInfoandHUD();
}

void AWuwaCharactorBase::InitGasInfoandHUD()
{
	AWuwaPlayerState* WuwaPlayerState = GetPlayerState<AWuwaPlayerState>();
	check(WuwaPlayerState);
	WuwaPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(WuwaPlayerState, this);
	AbilitySystemComponent = Cast<UWuwaAbilitySystemComponent>(WuwaPlayerState->GetAbilitySystemComponent());
	AttributeSet = WuwaPlayerState->GetAttributeSet();
	AbilitySystemComponent->InitAbilitySystemCompoent();
	// 只有本地玩家才有 HUD，服务端上的远程玩家和客户端上的别人的角色都没有
	if (AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetController()))
	{
		if (AWuwaHUD* HUD = Cast<AWuwaHUD>(PC->GetHUD()))
		{
			HUD->InitCtrAndWidget(AbilitySystemComponent, AttributeSet, PC, WuwaPlayerState);
			HUD->WuwaWidgetController->BroadInitialValues();
			HUD->WuwaWidgetController->BindCallBackDependencies();
		}
	}
}

#pragma endregion


#pragma region ASC

//在服务端初始化
void AWuwaCharactorBase::InitInitialAbilities()
{
	if (!HasAuthority()) return;
    
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!ASC)
	{
		return;
	}
    
	for (const TSubclassOf<UGameplayAbility> Ability : CharacterAbilities)
	{
		check(Ability);
		FGameplayAbilitySpec AbilitySpec(
		Ability,
		1,
		INDEX_NONE,
		this
		);
		if (UWuwaGameplayAbilityBase* WuwaAbility = Cast<UWuwaGameplayAbilityBase>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(WuwaAbility->OriginalTag);
			ASC->GiveAbility(AbilitySpec);
		}
        
	}
}

UAbilitySystemComponent* AWuwaCharactorBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
#pragma endregion

