#pragma once

#include "CoreMinimal.h"
#include "WuwaMovementTypes.generated.h"

/** 游戏侧移动策略。Movement 拥有它，动画只能读取它。 */
UENUM(BlueprintType)
enum class EWuwaGait : uint8
{
	Walk,
	Run,
	Sprint
};
