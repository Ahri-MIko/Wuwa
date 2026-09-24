// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Input/WuwaInputRouteHandler.h"
#include "WuwaAbilityInputHandlerComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaAbilityInputHandlerComponent : public UActorComponent,public IWuwaInputRouteHandler
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWuwaAbilityInputHandlerComponent();

public:
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
};
