// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "WuwaCharactorBase.generated.h"

class UWuwaMovementComponent;
class UGameplayAbility;
class UWuwaAbilitySystemComponent;
class UWuwaAttributeSet;
class UAbilitySystemComponent;
class UAttributeSet;

UCLASS()
class WUWA_API AWuwaCharactorBase : public ACharacter,public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AWuwaCharactorBase(const FObjectInitializer& ObjectInitializer);

	
#pragma region ReplicateOverride
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
#pragma endregion
	
#pragma region ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UWuwaAttributeSet> AttributeSet;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "GAS|Abilities")

	TArray<TSubclassOf<UGameplayAbility>> CharacterAbilities;

	UFUNCTION(BlueprintCallable, Category = "Wuwa|ASC")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
#pragma endregion
	
	
#pragma region Init
	virtual  void InitGasInfoandHUD();
	
	virtual void InitInitialAbilities();
#pragma endregion
	
#pragma region

	//自定义移动组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Components")
	TObjectPtr<UWuwaMovementComponent> WuwaMovementComponent;
	
	FORCEINLINE TObjectPtr<UWuwaMovementComponent> GetWuwaMovementComponent() const { return WuwaMovementComponent; }
#pragma endregion

protected:
	
	
};
