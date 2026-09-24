// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputActionValue.h"     
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "WuwaMovementComponent.generated.h"

struct FWuwaInputCommand;

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

	// 命令执行入口。这里拥有步态状态；输入和动画都不另存一份“是否走路”。
	bool ExecuteInputCommand(const FWuwaInputCommand& Command);

	/** Sprint 许可由移动/动作规则提供，不能仅凭一个按键绕过规则。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion")
	void SetSprintAllowed(bool bAllowed);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetDesiredGait() const { return DesiredGait; }

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetAllowedGait() const;

	/** 窗口只登记意图，不修改 DesiredGait、速度或 Sprint 许可。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void BeginSprintDesireWindow(UObject* WindowSource);

	/** InputHeldSeconds 来自语义输入层（游戏秒）；只计算与窗口重叠的部分。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void UpdateSprintDesireWindow(UObject* WindowSource, float InputHeldSeconds, float HoldThresholdSeconds);

	/** 只撤销这个通知持有的需求，保留其他仍然有效的窗口。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint")
	void EndSprintDesireWindow(UObject* WindowSource);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion|Sprint")
	EWuwaSprintDesire GetSprintDesire() const;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Run 继续使用已有 MaxWalkSpeed，保留项目当前默认移动速度。 */
	
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

	struct FSprintDesireWindow
	{
		double BeginTimeSeconds = 0.0;
		EWuwaSprintDesire Desire = EWuwaSprintDesire::Temporary;
	};

	// NotifyState 是共享资产；按角色保存窗口计时，不在通知对象上保存运行状态。
	TMap<TWeakObjectPtr<UObject>, FSprintDesireWindow> SprintDesireWindows;

	// 本阶段仅实现单机策略接口；联网预测/同步需单独接入 CMC saved move。
	UPROPERTY(Transient)
	bool bSprintAllowed = false;

};
