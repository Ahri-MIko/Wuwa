#pragma once

#include "CoreMinimal.h"
#include "WuwaInputCommand.generated.h"

// 输入事件回答“按了什么”；命令回答“这次请求做什么”。
// 本阶段只实现 SwitchWalk，不提前放入尚未使用的技能参数。
UENUM(BlueprintType)
enum class EWuwaInputCommandType : uint8
{
	None,
	SwitchWalk
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommand
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	EWuwaInputCommandType Type = EWuwaInputCommandType::None;
};
