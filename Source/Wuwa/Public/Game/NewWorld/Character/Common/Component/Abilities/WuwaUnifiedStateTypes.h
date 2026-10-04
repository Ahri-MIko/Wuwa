#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "WuwaUnifiedStateTypes.generated.h"

// 与原作一样区分位置、移动和朝向维度；这些是本项目的枚举，不复用原作数值。
UENUM(BlueprintType)
enum class EWuwaPositionState : uint8 { None, Ground, Air, Climb, Water };

UENUM(BlueprintType)
enum class EWuwaMoveState : uint8
{
	Other, Stand, Walk, WalkStop, Run, RunStop, Sprint, SprintStop,
	Dodge, Jump, Fall, NormalClimb, FastClimb, NormalSwim, FastSwim, Glide, Flying
};

UENUM(BlueprintType)
enum class EWuwaDirectionState : uint8 { FaceDirection, LockDirection, AimDirection };

/** 脚本提交的只读结果桥；物理和 AnimBP 读取同一份状态。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaUnifiedStateData
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EWuwaPositionState PositionState = EWuwaPositionState::Ground;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EWuwaMoveState MoveState = EWuwaMoveState::Stand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EWuwaDirectionState DirectionState = EWuwaDirectionState::FaceDirection;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EWuwaGait Gait = EWuwaGait::Run;
	/** 当前 MoveState 是 GA 写入的动作状态（Dodge）。由 MoveState 推导，供动画兼容读取；不是占用，普通移动可以覆盖它。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHasActionOverride = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Revision = 0;
};

/** 游戏线程采集的运动事实；决定状态的规则在 C#。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMovementStateContext
{
	GENERATED_BODY()
	/** 运动状态组件当前的位置（由移动模式事件同步），与原作 RoleGait 读取 UnifiedState.PositionState 一致。 */
	UPROPERTY(BlueprintReadOnly) EWuwaPositionState PositionState = EWuwaPositionState::None;
	UPROPERTY(BlueprintReadOnly) bool bHasMoveInput = false;
	UPROPERTY(BlueprintReadOnly) bool bIsCrouching = false;
	UPROPERTY(BlueprintReadOnly) bool bCanDriveState = false;
	UPROPERTY(BlueprintReadOnly) bool bIsFlying = false;
	UPROPERTY(BlueprintReadOnly) float GroundSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly) float VerticalSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly) double GameTimeSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly) float TemporarySprintDuration = 1.f;
};
