// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementSettings.h"
#include "WuwaMovementComponent.generated.h"

class UWuwaRoleGaitBridgeComponent;
class UWuwaUnifiedStateBridgeComponent;
class UWuwaInputIntentComponent;

/**
 *
 */
//可以在蓝图中看到这个ENUM
UENUM(BlueprintType)
namespace ECustomMoveMode
{
	enum Type
	{
		MOVE_Climb UMETA(DisplayName = "Climb Mode")
	};
}



UCLASS()
class WUWA_API UWuwaMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	/** 原作 CanWalkPress：运动状态的位置为 Ground 才能切换走跑偏好；本项目另外要求未蹲伏且由本端驱动状态。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	bool CanToggleWalkPreference() const;

	/** 运动状态组件保存的走跑偏好，以步态表示（Walk / Run）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetDesiredGait() const;
	/** 出生时的走跑偏好配置，运动状态组件初始化时读取。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetInitialDesiredGait() const { return DesiredGait; }

	/** 运动状态当前采用的步态（决定速度配置）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetAllowedGait() const;

	/** 由角色组装时注入；CMC 每帧物理更新前从这里读取移动意图，不去找角色或控制器。 */
	void BindInputIntent(UWuwaInputIntentComponent* InInputIntent);

	/** 由角色组装时注入运动状态与步态组件；RoleGait 传空表示不再驱动步态（角色结束）。 */
	void BindMovementState(UWuwaUnifiedStateBridgeComponent* InUnifiedState, UWuwaRoleGaitBridgeComponent* InRoleGait);

	/** 物理模式事实：移动模式变化时交给运动状态组件同步位置。 */
	UFUNCTION(BlueprintPure, Category="Wuwa|State") EWuwaPositionState ReadPositionState() const;
	UFUNCTION(BlueprintPure, Category="Wuwa|State") FWuwaUnifiedStateData GetUnifiedStateData() const;
	/** 原作 CMC 监听 CharOnUnifiedMoveStateChanged 更新速度配置；步态维度是本项目扩展。 */
	UFUNCTION() void HandleMoveStateChanged(EWuwaMoveState OldState, EWuwaMoveState NewState);
	UFUNCTION() void HandleGaitChanged(EWuwaGait OldGait, EWuwaGait NewGait);
	UFUNCTION(BlueprintCallable, Category="Wuwa|Movement") void RefreshMovementSettings();
	/** 可选统一配置；未指定时沿用现有 Walk/Run/Sprint 速度与 CMC 原有加速度/摩擦。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Movement") TObjectPtr<UWuwaMovementSettings> MovementSettings;

#pragma region Override Function
	virtual void OnMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
#pragma endregion


#pragma region Common
	bool PlayerisInputing();
#pragma endregion


#pragma region Climb
	bool IsClimbing() const;
	bool CanEnterClimbState();
	bool ProcessClimbSurfaces();
	bool IsFacingClimbableSurface();
	FQuat GetClimbRotation(float Deltatime);
	void SnapMovementToClimbableSurface(float Deltatime);
	bool CheckShouldStopClimbing();
	void StopClimbing();
#pragma endregion

#pragma region ClimbTraces

	bool ClimbableSurfacesTrace();
	bool ClimbableEyeSiteTrace();

	TArray<FHitResult> DoCapsuleMultipleforObjectTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide);
	FHitResult DoEyeSideTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide);
#pragma endregion

#pragma region ClimbVaribles
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbCapsuleTraceRadius = 50.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbCapsuleTraceHeight = 72.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbTraceDistance = 55.f;  

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MinClimbSurfaceAngle = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbSurfaceAngle = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbEntryAngle = 30;

	TArray<FHitResult> ClimbableSurfaceResults;
	FHitResult ClimbableEyesiteResult;

	FVector ProcessedSurfaceResults;
	FVector ProcessedSurfaceNomal;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbBrakingDeceleration = 500.f;

#pragma endregion

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void PhysCustom(float deltaTime, int32 Iterations) override;

	void PhysClimbing(float deltaTime, int32 Iterations);

protected:
	/** 仅为出生时的走跑偏好配置；运行值由 C# 运动状态组件维护。 */
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion")
	EWuwaGait DesiredGait = EWuwaGait::Run;

	/** 可调 Demo 默认值，不是原作速度参数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 900.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float RunSpeed = 500.f;

private:
	/** 原作 MoveComponent 每帧读取输入方向：地面按世界方向移动，攀爬沿墙面移动。 */
	void ApplyMoveIntent();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaInputIntentComponent> InputIntent;
	UPROPERTY(Transient)
	TObjectPtr<UWuwaUnifiedStateBridgeComponent> UnifiedState;
	UPROPERTY(Transient)
	TObjectPtr<UWuwaRoleGaitBridgeComponent> RoleGait;

	UWuwaRoleGaitBridgeComponent* ResolveGaitComponent() const;
	UWuwaUnifiedStateBridgeComponent* ResolveUnifiedState() const;
	bool bCapturedDefaultMovementSettings = false;
	FWuwaGaitMovementSettings DefaultMovementSettings;

};
