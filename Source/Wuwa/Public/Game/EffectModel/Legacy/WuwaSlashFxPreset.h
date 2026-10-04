#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "WuwaSlashFxPreset.generated.h"

class UNiagaraSystem;

/** 特效层生成后放在哪里。 */
UENUM(BlueprintType)
enum class EWuwaFxFollowMode : uint8
{
	/** 留在生成时的世界位置，角色移动不影响它（刀光默认）。 */
	StayInWorld UMETA(DisplayName = "留在原地"),
	/** 挂到通知指定的骨骼插槽上，随角色移动和转身。 */
	FollowSocket UMETA(DisplayName = "跟随插槽")
};

/** Legacy slash layer data retained only for existing asset compatibility and future migration. */
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
	//生成后留在原地还是跟随角色
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EWuwaFxFollowMode FollowMode = EWuwaFxFollowMode::StayInWorld;
	/** Optional child-space yaw animation. Empty curves preserve the authored transform. Time is relative to DelaySeconds on the effect timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRuntimeFloatCurve LocalYawDegrees;
};

/** Legacy preset schema only. The old playback system is removed; assets await migration to the next framework. */
UCLASS(BlueprintType)
class WUWA_API UWuwaSlashFxPreset : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") TArray<FWuwaSlashFxLayer> Layers;
	/** Former playback lifetime, retained as serialized data for migration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX", meta=(ClampMin="0.1", Units="s")) float MaximumLifetime = 2.f;
};
