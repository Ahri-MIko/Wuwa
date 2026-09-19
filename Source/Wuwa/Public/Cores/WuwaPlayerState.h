// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"     
#include "WuwaPlayerState.generated.h"
/**
 * 
 */
class UWuwaAbilitySystemComponent;
class UWuwaAttributeSet;

UCLASS()
class WUWA_API AWuwaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:

	AWuwaPlayerState();

#pragma region ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> WuwaAbilitySystemComponent;
#pragma endregion

#pragma region AS
	UPROPERTY()
	TObjectPtr<UWuwaAttributeSet> WuwaAttributeSet;
#pragma endregion

#pragma region Override function
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UWuwaAttributeSet* GetAttributeSet() const { return WuwaAttributeSet; }
	
#pragma endregion



};
