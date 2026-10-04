#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "WuwaEffectModelNiagara.generated.h"

class UNiagaraSystem;

/** 一个 Niagara 内容模型；将来由 Niagara Spec 创建并控制组件。 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelNiagara : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TObjectPtr<UNiagaraSystem> NiagaraRef;

	/** 对应原作 OnPreStop 的 Deactivate 开关；不表示立刻销毁已有粒子。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	bool DeactivateOnStop = true;

	/** 参数名与 Niagara 用户参数对应，值可以是常量或曲线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Parameters")
	TMap<FName, FWuwaEffectFloatCurve> FloatParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Parameters")
	TMap<FName, FWuwaEffectVectorCurve> VectorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Parameters")
	TMap<FName, FWuwaEffectColorCurve> ColorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Rendering")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Rendering")
	bool ReceiveDecal = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Rendering")
	int32 TranslucencySortPriority = 0;

	/** 保留原作移动端仿真优化开关，UE 适配逻辑尚未接入。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Backend")
	bool IgnoreMobileSimulationOptimize = false;

	/** 库洛点云 Niagara 标记；不会自动给当前引擎增加点云后端。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara|Backend")
	bool bPointCloudNiagara = false;

	/** 相对于父 Group 的局部位移；独立播放时相对于外部播放位置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	/** 欧拉角，单位度；X=Roll、Y=Pitch、Z=Yaw。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Scale{FVector::OneVector};

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
