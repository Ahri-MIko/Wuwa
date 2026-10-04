#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WuwaEffectModelBase.generated.h"

/**
 * 特效的共享配置资产，对应鸣潮 EffectModelBase。
 * 只描述要播放的内容；Actor、Component、计时器和播放状态由后续 Handle/Spec 持有。
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelBase : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 起始播放段的时长，不是生成延迟。负数保留原作的持续播放语义；默认 1 秒是本项目选择。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime", meta = (Units = "s"))
	float StartTime = 1.f;

	/** 大于 0 表示循环段长度，需外部停止后退出循环；0 表示没有此段。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0", Units = "s"))
	float LoopTime = 0.f;

	/** 结束段时长；不等同于 Niagara 粒子的寿命或自动淡出。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0", Units = "s"))
	float EndTime = 0.f;

	/** 是否忽略特效时间倍率。当前仅保存配置，后续由播放执行层解释。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Scale")
	bool IgnoreTimeDilation = false;

	/** 是否忽略全局时间倍率。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Scale")
	bool IgnoreGlobalTimeDilation = false;

	/** 以下策略字段由未来的执行层解释；默认值为本项目选择。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	bool AutoPlay = true;

	/** 跟随所属 Actor 的禁用/隐藏状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	bool NeedDisableWithActor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visibility")
	bool HideOnBurstSkill = false;

	/** 保留原作协议角色可见性开关，不在数据层判定本地或远端角色。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visibility")
	bool HideForProtoPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visibility")
	bool UiScenePrimitive = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	bool DisableOnMobile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	bool IgnoreDisable = false;

	/** 原作的持续循环优化策略标记，与 LoopTime 生命周期配置分开。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	bool ContinuousLoop = false;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

protected:
	/** 仅供编辑器检查资产引用图；运行时实例或播放状态不属于 Model。 */
	virtual void GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const;
#endif
};
