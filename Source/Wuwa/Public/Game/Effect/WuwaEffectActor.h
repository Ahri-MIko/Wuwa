// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WuwaEffectActor.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UAttributeEffect;



UCLASS()
class WUWA_API AWuwaEffectActor : public AActor
{
	GENERATED_BODY()
	
public:
	//Grant Effect to Actors
	UFUNCTION(BlueprintCallable)
	void ApplyEffect(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass);
	
	//Grant What kind of Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> AttributeEffectClass;


};
