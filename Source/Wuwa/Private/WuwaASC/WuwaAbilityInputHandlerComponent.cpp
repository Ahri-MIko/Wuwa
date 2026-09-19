// Fill out your copyright notice in the Description page of Project Settings.


#include "WuwaASC/WuwaAbilityInputHandlerComponent.h"

#include "PlayerController/WuwaPlayerController.h"
#include "Tools/DebugHelper.h"
#include "WuwaASC/WuwaAbilitySystemComponent.h"

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
	//后续会有专一化处理
	Debug::Print("Successfully Called"+InputEvent.InputTag.ToString());
	return IWuwaInputRouteHandler::HandleWuwaInput_Implementation(InputEvent);
}

