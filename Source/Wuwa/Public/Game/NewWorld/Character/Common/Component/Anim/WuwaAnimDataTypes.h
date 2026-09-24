#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "WuwaAnimDataTypes.generated.h"

/** 游戏线程采集的运动事实；不包含键名，也不决定动画状态。向量均为世界空间。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAnimMoveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move")
	FQuat ActorRotation = FQuat::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move")
	FVector Velocity = FVector::ZeroVector;

	/** CMC 控制加速度，不能代替 InputVector。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move")
	FVector Acceleration = FVector::ZeroVector;

	/** CMC 最近一次消费的移动输入；不等于键盘原始轴，也不保证产生位移。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move")
	FVector InputVector = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Move", meta = (Units = "cm/s"))
	float MaxSpeed = 0.f;
};

/** 游戏侧状态的只读副本。权威值仍在 Movement/Character 中。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaAnimStateData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	TEnumAsByte<EMovementMode> MovementMode = MOVE_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	uint8 CustomMovementMode = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EWuwaGait DesiredGait = EWuwaGait::Run;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EWuwaGait AllowedGait = EWuwaGait::Run;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EWuwaSprintDesire SprintDesire = EWuwaSprintDesire::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bStateGround = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bStateAir = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bStateClimb = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsCrouching = false;
};
