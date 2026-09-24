// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h" 
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
//添加GAS引用
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include"Game/NewWorld/Character/Common/WuwaCharactorBase.h"
//所有头文件放于之上
#include "WuwaCharacter.generated.h"

class UWuwaInputComponent;
class UWuwaMovementComponent;
class USpringArmComponent;
class UCameraComponent;
UCLASS()
class WUWA_API AWuwaCharacter : public AWuwaCharactorBase,
	public IAbilitySystemInterface //修改:继承接口
{
	GENERATED_BODY()

//Camera
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wuwa|Camera")
	float CameraArmLength = 400.f;


//PlayerInputs
public:
	/** 从已有输入缓存生成只读快照；需要最新输入时重新调用，不另外维护一份可写缓存。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;

	/** 判断是否有移动意图的阈值，作用于 Enhanced Input 已处理过的轴值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|PlayerInput", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveInputThreshold = 0.01f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MoveInput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MoveInputDir;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MouseMoveVec;

//Component
public:

	// Sets default values for this character's properties
	AWuwaCharacter(const FObjectInitializer& ObjectInitializer);

	//自定义输入组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Components")
	TObjectPtr<UWuwaInputComponent> WuwaInputComponent;

	//自定义移动组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Components")
	TObjectPtr<UWuwaMovementComponent> WuwaMovementComponent;

//ASC
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "GAS|Abilities")

	TArray<TSubclassOf<UGameplayAbility>> CharacterAbilities;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
#pragma region Tools
	//FORCEINLINE内联函数
	FORCEINLINE TObjectPtr<UWuwaMovementComponent> GetWuwaMovementComponent() const { return WuwaMovementComponent; }
	
	UFUNCTION()
	FVector2D Vector2ToCameraDirNormalized(const FVector2D InSource2D, const UCameraComponent* InCameraComp) const;

#pragma endregion
	
	
#pragma region AbilitySystem
	UFUNCTION(BlueprintCallable, Category = "Wuwa|ASC")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	void InitGasInfoandHUD();
	
	void InitInitialAbilities();
	
	
#pragma endregion
	
#pragma region Movement
	UFUNCTION()
	void HandleMoveInput(const FInputActionValue& Value);

	FVector2D  GetInputDir(FVector2D PlayerInput);

	UFUNCTION()
	void HandleLook(const FInputActionValue& Value);

	void HabdleClimbInput(const FInputActionValue& Value);

	void Move(const FInputActionValue& Value);
	
	bool CanApplyMove();

#pragma endregion

#pragma region ReplicateOverride
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
#pragma endregion

	
};
