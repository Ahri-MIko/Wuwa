// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WuwaEffectActor.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UAttributeEffect;


//能施加GE的物品
UCLASS()
class WUWA_API AWuwaEffectActor : public AActor
{
	GENERATED_BODY()
	
public:
	//给Targte效果
	UFUNCTION(BlueprintCallable)
	void ApplyEffect(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass);
	
	//Grant What kind of Effect
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> AttributeEffectClass;


};
