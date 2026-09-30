// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputActionValue.h"     
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementSettings.h"
#include "WuwaMovementComponent.generated.h"

struct FWuwaInputCommand;
class UWuwaRoleGaitBridgeComponent;

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

	// 语义化移动策略。调用方可以是输入路由、蓝图或 AI，不接受 Alt/Shift 键名。
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion")
	void SetDesiredGait(EWuwaGait NewGait);

	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion")
	void ToggleWalkRun();

	/** 当前 Demo 规则：仅站立地面状态可切换；不是原作 CanWalkPress 的完整还原。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	bool CanSwitchWalk() const;

	// 兼容现有蓝图/输入路由的转发入口；规则和运行状态属于 C# RoleGait。
	bool ExecuteInputCommand(const FWuwaInputCommand& Command);

	/** Sprint 许可由移动/动作规则提供，不能仅凭一个按键绕过规则。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion")
	void SetSprintAllowed(bool bAllowed);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetDesiredGait() const;
	EWuwaGait GetInitialDesiredGait() const { return DesiredGait; }

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetAllowedGait() const;

	/** 窗口登记冲刺意图，再由脚本规则决定是否提交 Sprint；不覆盖走跑偏好。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void BeginSprintDesireWindow(UObject* WindowSource);

	/** InputHeldSeconds 来自语义输入层（游戏秒）；只计算与窗口重叠的部分。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void UpdateSprintDesireWindow(UObject* WindowSource, float InputHeldSeconds, float HoldThresholdSeconds);

	/** 提交窗口最后采样的需求，再移除窗口；Temporary 从此刻开始计时。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void EndSprintDesireWindow(UObject* WindowSource);

	/** 退出地面移动、失去控制或动作规则要求重置时调用；迟到的 Tick/End 不会恢复需求。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void ClearSprintDesire();

	/** 来自角色语义移动输入；结束移动时清除已经提交的长期冲刺。 */
	void NotifyMoveInputChanged(bool bHasMoveInput);

	/** 窗口结束后暂时冲刺保留的游戏秒数，不包含窗口自身的时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Sprint", meta = (ClampMin = "0", Units = "s"))
	float TemporarySprintDuration = 1.f;

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion|Sprint")
	EWuwaSprintDesire GetSprintDesire() const;

	/** 物理模式事实，由脚本转换为角色位置状态。 */
	UFUNCTION(BlueprintPure, Category="Wuwa|State") EWuwaPositionState ReadPositionState() const;
	UFUNCTION(BlueprintPure, Category="Wuwa|State") FWuwaUnifiedStateData GetUnifiedStateData() const;
	UFUNCTION(BlueprintPure, Category="Wuwa|State") EWuwaGait GetStopGait() const;
	UFUNCTION() void HandleUnifiedStateChanged(const FWuwaUnifiedStateData& OldState, const FWuwaUnifiedStateData& NewState);
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
	/** 仅为出生时的走跑偏好配置；运行值由脚本组件维护。 */
	
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
	UWuwaRoleGaitBridgeComponent* ResolveGaitComponent() const;
	bool bCapturedDefaultMovementSettings = false;
	FWuwaGaitMovementSettings DefaultMovementSettings;

};
