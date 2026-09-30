// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h" 
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include"Game/NewWorld/Character/Common/WuwaCharactorBase.h"
//所有头文件放于之上
#include "WuwaCharacter.generated.h"

class UWuwaMovementComponent;
class UCameraComponent;
class UWuwaUnifiedStateBridgeComponent;
class UWuwaRoleGaitBridgeComponent;
class UWuwaFightStateBridgeComponent;
class UWuwaSkillBridgeComponent;
class UWuwaInputCommandConfig;
UCLASS()
class WUWA_API AWuwaCharacter : public AWuwaCharactorBase
{
	GENERATED_BODY()

#pragma region LifeCycle
	
public: 
	
	//Before BeginPlay
	virtual void PostInitializeComponents() override;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	
#pragma endregion
	
#pragma region PlayerInput
public:
	/** 从已有输入缓存生成只读快照；需要最新输入时重新调用，不另外维护一份可写缓存。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;

	/** 输入被清空或切换控制角色时同步清理角色侧意图。 */
	void ResetPlayerInputState();

	/** 判断是否有移动意图的阈值，作用于 Enhanced Input 已处理过的轴值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|PlayerInput", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveInputThreshold = 0.01f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MoveInput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MoveInputDir;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MouseMoveVec;

#pragma  endregion
	
#pragma  region PlayerMovement
	
	UFUNCTION()
	void HandleMoveInput(const FInputActionValue& Value);

	void Move(const FInputActionValue& Value);
	
	void HabdleClimbInput(const FInputActionValue& Value);
	
	bool CanApplyMove();
	
	FVector2D GetInputDir(FVector2D PlayerInput) const {return  MoveInputDir;};
#pragma endregion	
//Component
public:

	// Sets default values for this character's properties
	AWuwaCharacter(const FObjectInitializer& ObjectInitializer);


#pragma region C sharp桥接
	
	//运动状态
	/** 从托管类创建，基类只提供反射/快照桥。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State")
	TObjectPtr<UWuwaUnifiedStateBridgeComponent> UnifiedStateComponent;//运动状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State")
	TObjectPtr<UWuwaRoleGaitBridgeComponent> RoleGaitComponent;
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|State") 
	TSoftClassPtr<UWuwaUnifiedStateBridgeComponent> UnifiedStateClass;
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|State") 
	TSoftClassPtr<UWuwaRoleGaitBridgeComponent> RoleGaitClass;
	
	//战斗状态
	/** 独立的战斗状态裁决；不与位置/步态状态共用占用记录。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Combat")
	TObjectPtr<UWuwaFightStateBridgeComponent> FightStateComponent;//战斗状态
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat")
	TSoftClassPtr<UWuwaFightStateBridgeComponent> FightStateClass;
	/** 可重复调用；已存在的组件及其战斗占用不会被重置。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	bool EnsureFightStateSystem();
	
	/** 主技能管理，与 FightState 一起自动装配；不从移动状态推导当前技能。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Combat")
	TObjectPtr<UWuwaSkillBridgeComponent> SkillComponent;
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat")
	TSoftClassPtr<UWuwaSkillBridgeComponent> SkillClass;
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	bool EnsureSkillSystem();
	
	//比如同一个输入会有不同的效果,典型的例子就是普攻分段
	/** 同一个输入根据角色条件转换成具体技能；策略在 C# 输入层执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Input")
	TObjectPtr<UWuwaInputCommandConfig> InputCommandConfig;

	
#pragma endregion
	
	UFUNCTION(BlueprintCallable, Category="Wuwa|State") bool EnsureMovementStateSystem();
	bool IsMovementStateEnding() const { return bMovementStateEnding; }

public:	
	
#pragma region Tools
	
	/** 移动与 Dash 共用的视角方向；不依赖角色上的相机组件。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FVector2D GetCameraRelativeMoveDirection(FVector2D InputAxis) const;

	/** 兼容旧调用；新逻辑应使用 GetCameraRelativeMoveDirection。 */
	UFUNCTION(meta = (DeprecatedFunction, DeprecationMessage = "Use GetCameraRelativeMoveDirection instead."))
	FVector2D Vector2ToCameraDirNormalized(const FVector2D InSource2D, const UCameraComponent* InCameraComp) const;

#pragma endregion

private:
	//初始化保护句柄
	bool bInitializingMovementState = false;
	bool bInitializingSkill = false;
	bool bInitializingFightState = false;
	
	bool bMovementStateReady = false;
	bool bMovementStateEnding = false;
	bool bFightStateEnding = false;
};
