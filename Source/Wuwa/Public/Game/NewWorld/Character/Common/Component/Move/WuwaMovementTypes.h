#pragma once

#include "CoreMinimal.h"
#include "WuwaMovementTypes.generated.h"

/** 脚本 RoleGait 选择步态，UnifiedState 提交结果，Movement/动画消费结果。 */
UENUM(BlueprintType)
enum class EWuwaGait : uint8
{
	Walk,
	Run,
	Sprint
};

/** 窗口采集、角色保留的冲刺意图；不等同于当前步态或 Sprint 许可。 */
UENUM(BlueprintType)
enum class EWuwaSprintDesire : uint8
{
	None UMETA(DisplayName = "无冲刺需求"),
	Temporary UMETA(DisplayName = "暂时冲刺"),
	Sustained UMETA(DisplayName = "长期冲刺")
};
