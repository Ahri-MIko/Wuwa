#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelGroup.generated.h"

/** 一组独立 Model 资产及各自的播放延迟；对应鸣潮 EffectModelGroup。 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelGroup : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	/**
	 * Key 是子 Model 的硬引用；Value 是相对组开始播放的延迟秒数。
	 * 同一个子资产只能出现一次，Map 顺序不表示播放顺序。原作不允许 Group 作为子项。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Group")
	TMap<TObjectPtr<UWuwaEffectModelBase>, float> EffectData;

	/** 整组相对于外部播放位置的局部位移，单位厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	/** 欧拉角，单位度；原作约定 X=Roll、Y=Pitch、Z=Yaw。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Scale{FVector::OneVector};

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

protected:
	virtual void GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const override;
#endif
};
