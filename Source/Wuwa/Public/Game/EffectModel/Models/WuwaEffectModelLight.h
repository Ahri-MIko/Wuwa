#pragma once

#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelLight.generated.h"

/**
 * 对应 EffectModelLight：原作 Spec 创建 PointLightComponent，Model 只提供配置。
 * 导出能确认 Location 和两个 Character 字段；UE Adapter 下的点光字段是本项目适配，
 * 不宣称还原了未导出的 Kuro 原生灯光参数或 Toon 渲染实现。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelLight : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	/** 原作类默认对象明确保存 5；普通 UE 点光没有等价的角色专用衰减参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Kuro Character Settings", meta = (ClampMin = "0"))
	float CharacterFalloffExponent = 5.f;

	/** 原作类默认对象为黑色；仅保留数据，需专门的角色材质/渲染适配。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Kuro Character Settings")
	FLinearColor CharacterHardShadowColor = FLinearColor::Black;

	/** 以下使用本项目默认值，未来由 Light Spec 应用到 UE 点光组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Unreal Adapter")
	FWuwaEffectFloatCurve Intensity{1000.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Unreal Adapter")
	FWuwaEffectColorCurve LightColor{FLinearColor::White};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Unreal Adapter")
	FWuwaEffectFloatCurve AttenuationRadius{500.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light|Unreal Adapter")
	bool CastShadows = false;
};
