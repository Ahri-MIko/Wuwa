#pragma once

#include "CoreMinimal.h"
#include "WuwaEffectHandle.generated.h"

/** 标识一次播放；不同类型共用此句柄，同一 Model 每次播放获得不同 Id。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectHandle
{
	GENERATED_BODY()

	// Id 非空不代表仍在播放；运行状态向 EffectSystem 查询。
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FGuid Id;
};
