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

/** 动画窗口采集的冲刺意图；不等同于当前步态或 Sprint 许可。 */
UENUM(BlueprintType)
enum class EWuwaSprintDesire : uint8
{
	None UMETA(DisplayName = "无冲刺需求"),
	Temporary UMETA(DisplayName = "暂时冲刺"),
	Sustained UMETA(DisplayName = "长期冲刺")
};
