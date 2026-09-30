#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "WuwaSlashFxPreset.generated.h"

class UNiagaraSystem;

/** One visual layer in a short, authored slash. Transforms are relative to the notify's placement. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSlashFxLayer
{
	GENERATED_BODY()
	//用哪个特效
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UNiagaraSystem> System;
	//播放的偏移量
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform Transform = FTransform::Identity;
	//延迟多少秒播放
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s")) float DelaySeconds = 0.f;
	/** Optional child-space yaw animation. Empty curves preserve the authored transform. Time is relative to DelaySeconds on the effect timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRuntimeFloatCurve LocalYawDegrees;

	FTransform GetTransformAtAge(float SecondsAfterSpawn) const
	{
		const FRichCurve* Curve = LocalYawDegrees.GetRichCurveConst();
		if (!Curve || Curve->GetNumKeys() == 0) return Transform;
		return FTransform(FRotator(0.f, Curve->Eval(FMath::Max(0.f, SecondsAfterSpawn)), 0.f)) * Transform;
	}
};

/** Shared art configuration; per-playback state lives in WuwaSlashFxComponent. */
UCLASS(BlueprintType)
class WUWA_API UWuwaSlashFxPreset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") TArray<FWuwaSlashFxLayer> Layers;
	/** Safety lifetime, including fading particles after skill interruption. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX", meta=(ClampMin="0.1", Units="s")) float MaximumLifetime = 2.f;
};
