#pragma once

#include "Engine/Scene.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelPostProcess.generated.h"

class UMaterialInterface;
class UTexture;

/**
 * 对应 EffectModelPostProcess 的配置；原作通过 KuroPostProcessComponent 执行。
 * FPostProcessSettings 是本项目的 UE 适配入口；TOD、径向模糊等 Kuro 专用行为只保留配置，
 * 需未来 Spec/材质适配，不会因创建此资产就自动获得原作渲染能力。
 * 除注释指出的原作类默认值外，下列初始值都是本项目选择。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelPostProcess : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UWuwaEffectModelPostProcess()
	{
		// 原作 TypeScriptGeneratedClass 默认对象明确覆盖此值。
		IgnoreTimeDilation = true;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	/** 原作控制是否覆盖 Time Of Day；需后续环境系统参与。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process")
	bool OverrideTOD = false;

	/** 原作用于决定其他玩家角色的特效是否影响本地后处理。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process")
	bool VisibleForProtoPlayer = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process")
	float WeatherPriority = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Volume")
	bool EnableVolume = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Volume", meta = (EditCondition = "EnableVolume", ClampMin = "0", Units = "cm"))
	float VolumeRadius = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Volume")
	bool UseVolumeHardnessCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Volume", meta = (EditCondition = "!UseVolumeHardnessCurve", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float VolumeHardness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Volume", meta = (EditCondition = "UseVolumeHardnessCurve", EditConditionHides))
	FWuwaEffectFloatCurve VolumeHardnessCurve{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Material")
	bool bEnablePostprocessMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Material", meta = (EditCondition = "bEnablePostprocessMaterial"))
	TObjectPtr<UMaterialInterface> PostprocessMaterial;

	/** 原作可将角色世界位置投影为屏幕中心；不是任意 UObject 的直接引用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur")
	bool UseWorldPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur", meta = (EditCondition = "!UseWorldPosition", ClampMin = "0", ClampMax = "1"))
	FVector2D ScreenPosition = FVector2D(0.5, 0.5);

	/** 原作传入其扩展后处理设置；UE 标准后处理不自带同名功能。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur")
	TObjectPtr<UTexture> RadialBlurMask;

	/** 原作类默认对象明确保存 (5, 5)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur")
	FVector2D RadialBlurMaskScale = FVector2D(5.0, 5.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur")
	FWuwaEffectFloatCurve RadialBlurHardness{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Radial Blur")
	FWuwaEffectFloatCurve RadialBlurRadius{1.f};

	/** 本项目适配字段：曝光、景深、调色等 UE 原生配置，勾选所需的 Override 项。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Post Process|Unreal Adapter")
	FPostProcessSettings Settings;
};
