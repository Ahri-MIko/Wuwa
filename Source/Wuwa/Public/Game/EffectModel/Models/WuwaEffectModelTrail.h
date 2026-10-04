#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelTrail.generated.h"

class UMaterialInterface;

/**
 * 对应原作 EffectModelTrail：通过多个采样点形成带状网格，不是 Niagara Ribbon 配置。
 * 原作执行依赖 KuroBezierMeshComponent；本层只保存数据，不伪造该组件的运行行为。
 * 字段来自 EffectModelTrailSpec 与 UsedInfo；初始化值采用项目默认值。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelTrail : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Points")
	bool AttachToBones = true;

	/** 开启骨骼绑定时至少需要两个骨骼/插槽；实际组件由播放 Context 提供。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Points", meta = (EditCondition = "AttachToBones", EditConditionHides))
	TArray<FName> AttachBoneNames;

	/** 骨骼模式：逐个插槽的局部偏移；非骨骼模式：相对目标 SkeletalMeshComponent 的点位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Points")
	TArray<FVector> RelativeLocations;

	/** Key 是采样点下标；曲线提供该点附加的局部位移，按总播放时间采样。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Points")
	TMap<int32, FWuwaEffectVectorCurve> LocationsCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Material")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Material")
	TMap<FName, FWuwaEffectFloatCurve> FloatParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Material")
	TMap<FName, FWuwaEffectColorCurve> ColorParameters;

	/** 传给原作带状网格 Setup 的单位长度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Shape", meta = (ClampMin = "0.001"))
	float UnitLength = 1.f;

	/** 每次新增一层带状网格时的透明度，按总播放时间采样。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Shape")
	FWuwaEffectFloatCurve Alpha{1.f};

	/** 原作每次更新消退的网格量；不能直接当作厘米/秒的角色移动速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Dissipation")
	FWuwaEffectFloatCurve DissipateSpeed{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Dissipation", meta = (ClampMin = "0"))
	float DissipateSpeedAfterDead = 1.f;

	/** 原作 ShouldDestroy 的选项：进入停止生成阶段后是否立即结束，不等待拖尾消散。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Dissipation")
	bool DestroyAtOnce = false;
};
