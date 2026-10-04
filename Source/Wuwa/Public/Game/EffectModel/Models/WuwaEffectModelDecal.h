#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelDecal.generated.h"

class UMaterialInterface;

/** 原作 EffectModelDecal 的贴花材质、参数和变换配置；以下初始化值是项目默认值。 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelDecal : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal")
	TObjectPtr<UMaterialInterface> DecalMaterialRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TMap<FName, FWuwaEffectFloatCurve> MaterialFloatParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TMap<FName, FWuwaEffectColorCurve> MaterialColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Rotation;

	/** 原作 Spec 使用固定 DecalSize=(100,100,100)，再应用这里的缩放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Scale{FVector::OneVector};

	/** 对应库洛 DecalComponent.ZFadingFactor；原生 UE 贴花没有该同名字段。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Kuro Extension")
	float ZfadingFactor = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Decal|Kuro Extension")
	float ZfadingPower = 1.f;
};
