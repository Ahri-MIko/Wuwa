// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WuwaCharactorBase.generated.h"

class UWuwaAbilitySystemComponent;
class UWuwaAttributeSet;
class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class WUWA_API AWuwaCharactorBase : public ACharacter
{
	GENERATED_BODY()

public:
	AWuwaCharactorBase(const FObjectInitializer& ObjectInitializer);

protected:
	
#pragma region ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;
#pragma endregion

#pragma region AS
	UPROPERTY()
	TObjectPtr<UWuwaAttributeSet> AttributeSet;

};
