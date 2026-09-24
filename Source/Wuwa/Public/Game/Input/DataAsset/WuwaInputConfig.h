// WuwaInputConfig.h
// 输入配置 DataAsset:以 UInputAction 资产指针为键的长按配置表。
// 类在 C++ 定义,资产实例在编辑器创建并填数:
// Content Browser 右键 → Miscellaneous → Data Asset → 选 WuwaInputConfig

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Input/WuwaInputTypes.h"
#include "WuwaInputConfig.generated.h"

class UInputAction;

UCLASS(BlueprintType)
class WUWA_API UWuwaInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 每个动作的长按配置。键用 IA 资产指针而不是枚举:
	 * C++ 读不了蓝图枚举,而 IA 指针在 Triggered/Started 回调里天然拿得到,查表零转换。
	 * 不在表里的 IA 视为无长按行为。
	 */
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	TMap<TObjectPtr<const UInputAction>, FWuwaHoldConfig> HoldConfigs;

	/** 查某个动作的长按配置,查不到返回 nullptr(= 无长按行为) */
	const FWuwaHoldConfig* FindHoldConfig(const UInputAction* Action) const
	{
		return Action ? HoldConfigs.Find(Action) : nullptr;
	}
};