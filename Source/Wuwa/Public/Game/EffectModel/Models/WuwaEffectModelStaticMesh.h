#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelStaticMesh.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * 对应原作 EffectModelStaticMesh：网格、材质和变换曲线的共享配置。
 * 字段依据 EffectModelStaticMeshSpec 与 UsedInfo；导出未给出的原生默认值采用项目默认值。
 * 动态材质、StaticMeshComponent 及曲线采样状态由后续 Spec 持有。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelStaticMesh : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UStaticMesh> StaticMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	bool UseMultipleMaterialSlots = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (EditCondition = "!UseMultipleMaterialSlots", EditConditionHides))
	TObjectPtr<UMaterialInterface> MaterialOverrideRef;

	/** 按材质槽下标覆盖；不是一组需要同时播放的子 Model。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (EditCondition = "UseMultipleMaterialSlots", EditConditionHides))
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrideArrayRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TMap<FName, FWuwaEffectFloatCurve> MaterialFloatParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	TMap<FName, FWuwaEffectColorCurve> MaterialColorParameters;

	/** 原作允许外部 Niagara 参数包覆盖材质参数；此处只保存是否接受的配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	bool AcceptExternalNiagaraParameter = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	bool ReceiveDecal = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	bool CastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	int32 TranslucencySortPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	bool EnableCollision = false;

	/** 对应库洛扩展渲染字段；UE 原生 StaticMeshComponent 没有同名接口。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Kuro Extension")
	bool EnableScreenSizeCullRatioOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Kuro Extension", meta = (EditCondition = "EnableScreenSizeCullRatioOverride", ClampMin = "0"))
	float ScreenSizeCullRatio = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	/** 欧拉角，单位度；X=Roll、Y=Pitch、Z=Yaw。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Scale{FVector::OneVector};
};
