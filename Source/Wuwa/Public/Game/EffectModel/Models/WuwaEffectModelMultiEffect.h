#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "WuwaEffectModelMultiEffect.generated.h"

/** 原作脚本目前能确认的 MultiEffect 类型：Type == 0 创建 MultiEffectBuffBall。 */
UENUM(BlueprintType)
enum class EWuwaMultiEffectType : uint8
{
	BuffBall = 0 UMETA(DisplayName = "Buff Ball")
};

/**
 * 对应原作 EffectModelMultiEffect 的重复特效配置。
 * 和 Group 的固定子项列表不同，它用同一个 EffectData 创建多个实例，再由专门行为计算数量与位置。
 * 原作 BuffBall 的数量/旋转算法不在 Model 中执行；当前只保存其输入数据。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelMultiEffect : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multi Effect")
	EWuwaMultiEffectType Type = EWuwaMultiEffectType::BuffBall;

	/**
	 * 要重复播放的 Model；原脚本直接将 EffectData 对象交给 GetPathName，项目采用硬引用。
	 * Multi 使用 SpawnEffect 创建独立 root，因此这里允许 Group；编辑器拒绝直接或间接引用环。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multi Effect")
	TObjectPtr<UWuwaEffectModelBase> EffectData;

	/** 原作 BuffBall 以 ceil(BaseNum * 已播放秒数 - 0.01) 求期望数量；它不是固定总数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multi Effect|Buff Ball", meta = (ClampMin = "0"))
	float BaseNum = 1.f;

	/** 原作更新 BaseAngle -= DeltaSeconds * SpinSpeed，随后直接用于 sin/cos，单位为弧度/秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multi Effect|Buff Ball")
	float SpinSpeed = 1.f;

	/** 原作球体绕中心移动的半径。本项目默认 100 cm，不声称这是原作默认值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Multi Effect|Buff Ball", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 100.f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

protected:
	virtual void GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const override;
#endif
};
