#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "WuwaLocomotionTypes.generated.h"

/** 对应原资源中的分方向混合思路；归一化算法是本 Demo 的实现。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaVelocityBlend
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Direction")
	float Forward = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Direction")
	float Backward = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Direction")
	float Left = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Direction")
	float Right = 0.f;
};

/**
 * AnimInstance 每次更新时生成的只读快照，不是角色逻辑的第二份权威状态。
 * Local 使用角色 Actor 坐标系（X 前、Y 右、Z 上），不是骨骼 Mesh 坐标系。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaLocomotionAnimData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validity")
	bool bHasValidMovementData = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion")
	FVector LocalVelocity = FVector::ZeroVector;

	/** CMC 从输入计算的加速度，不是速度差分得到的物理加速度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion")
	FVector LocalAccel = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion", meta = (Units = "cm/s"))
	float GroundSpeed = 0.f;

	/** CMC 的有效速度上限；不同于角色此刻的实际速度 GroundSpeed。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion", meta = (Units = "cm/s"))
	float MaxSpeed = 0.f;

	/** 最近一次 Movement 消费的输入向量转到 Actor 局部空间，长度 0..1；不是速度或加速度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intent")
	FVector LocalMoveIntent = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Intent")
	bool bHasMoveInput = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	EWuwaGait DesiredGait = EWuwaGait::Run;

	/** 当前允许的步态；即使静止也可为 Run，不代表已经达到跑速。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	EWuwaGait AllowedGait = EWuwaGait::Run;

	/** 来自动画窗口的冲刺需求，不代表已经进入 Sprint。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	EWuwaSprintDesire SprintDesire = EWuwaSprintDesire::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	uint8 CustomMovementMode = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bStateGround = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bStateAir = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bStateClimb = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bIsCrouching = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	bool bStateGroundWalk = false;

	/** 地面跑步策略，包括静止时选择 Run；是否真的在动另看速度/输入。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	bool bStateGroundRun = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gait")
	bool bStateGroundSprint = false;

	/** 用进入/退出两个速度阈值避免低速抖动，不等同于有输入。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	bool bHasMovingSpeed = false;

	/** 起步候选条件；还需要 AnimBP 自己的转场/动作许可。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	bool bIsGoingToMove = false;

	/** 停步候选条件，不代表动画已进入 Stop，不负责取消 GA。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Transitions")
	bool bWantsToStop = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blend")
	FWuwaVelocityBlend VelocityBlend;
};
