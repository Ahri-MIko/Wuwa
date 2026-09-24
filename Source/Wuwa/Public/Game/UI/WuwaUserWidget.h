// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WuwaUserWidget.generated.h"

/**
 * 
 */
 
 
 
UCLASS()
class WUWA_API UWuwaUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public: 
	
	UFUNCTION(BlueprintCallable)
	void InitWidgetController(UObject* widgetcontroller);
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;
	
	UFUNCTION(BlueprintImplementableEvent)
	void AfterSetWidgetController();
	
};
