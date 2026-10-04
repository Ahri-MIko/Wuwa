#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "UObject/SoftObjectPath.h"
#include "WuwaEffectModelGpuParticle.generated.h"

/**
 * 对应原作 EffectModelGpuParticle，不是 Niagara 的 GPU 模拟开关。
 * 原作由 KuroGPUParticleComponent 消费 Data；当前项目没有这个专有组件，只建立配置数据。
 * 字段来自 UsedInfo 与 EffectModelGpuParticleSpec；默认值是本项目选择，并非原作 CDO 的还原。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelGpuParticle : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	/** 传给原作 KuroGPUParticleComponent.SetGPUData 的资源；导出未公开其具体资源类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle")
	FSoftObjectPath Data;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Local Transform")
	FWuwaEffectVectorCurve Location;

	/** 与现有 Niagara Model 一致：X/Y/Z 分别表示 Roll/Pitch/Yaw，单位为度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Local Transform")
	FWuwaEffectVectorCurve Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Local Transform")
	FWuwaEffectVectorCurve Scale = FWuwaEffectVectorCurve(FVector::OneVector);

	/** 粒子后端自身的循环开关，独立于 ModelBase 的 LoopTime。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle|Playback")
	bool Loop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle|Playback")
	bool ReversePlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle|Playback")
	bool EnablePingPong = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle|Playback", meta = (ClampMin = "0", Units = "s", EditCondition = "EnablePingPong"))
	float PingPongTime = 0.f;

	/** 原作仅在 bUseCurve 为 true 时启用后端自定义时间倍率曲线。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GPU Particle|Playback")
	FWuwaEffectFloatCurve TimeScaler = FWuwaEffectFloatCurve(1.f);
};
