// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"

#include "Game/Controller/WuwaPlayerController.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"

// Sets default values for this component's properties
UWuwaAbilityInputHandlerComponent::UWuwaAbilityInputHandlerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

bool UWuwaAbilityInputHandlerComponent::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetOwner());
	if (!PC)
	{
		return false;
	}
	
	UWuwaAbilitySystemComponent* ASC = PC->GetASC();
	if (!IsValid(ASC))
	{
		return false;
	}
	//后续会有专一化处理
	ASC->ProcessInputEvent(InputEvent);
	return IWuwaInputRouteHandler::HandleWuwaInput_Implementation(InputEvent);
}

