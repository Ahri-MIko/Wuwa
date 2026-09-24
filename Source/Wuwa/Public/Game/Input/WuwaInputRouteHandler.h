// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WuwaInputRouteHandler.generated.h"

struct FWuwaInputEvent;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWuwaInputRouteHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WUWA_API IWuwaInputRouteHandler
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	bool HandleWuwaInput(const FWuwaInputEvent& InputEvent);
};
