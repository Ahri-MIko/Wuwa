#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "WuwaMovementSettings.generated.h"

USTRUCT(BlueprintType)
struct WUWA_API FWuwaGaitMovementSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="cm/s")) float MaxSpeed = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float MaxAcceleration = 2048.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float GroundFriction = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float BrakingDeceleration = 2048.f;
};

/** 状态改变时统一应用的地面移动配置。根运动/碰撞依然由 CMC 处理。 */
UCLASS(BlueprintType)
class WUWA_API UWuwaMovementSettings : public UDataAsset
{
	GENERATED_BODY()
public:
	UWuwaMovementSettings() { Walk.MaxSpeed = 200.f; Run.MaxSpeed = 500.f; Sprint.MaxSpeed = 900.f; }
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FWuwaGaitMovementSettings Walk;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FWuwaGaitMovementSettings Run;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FWuwaGaitMovementSettings Sprint;
	const FWuwaGaitMovementSettings& ForGait(EWuwaGait Gait) const
	{
		return Gait == EWuwaGait::Walk ? Walk : Gait == EWuwaGait::Sprint ? Sprint : Run;
	}
};
