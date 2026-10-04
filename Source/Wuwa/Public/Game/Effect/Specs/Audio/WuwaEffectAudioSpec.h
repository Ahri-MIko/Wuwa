#pragma once

#include "Components/AudioComponent.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectAudioSpec.generated.h"

/** 一次 UE 声音播放；组件、停止进度属于 Spec，不写回共享 Model。 */
UCLASS()
class WUWA_API UWuwaEffectAudioSpec : public UWuwaEffectSpec
{
	GENERATED_BODY()

public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override;
	virtual void Stop_Implementation(bool bImmediately) override;

	/** Audio 池归还/淘汰入口；不通知播放结束，也不改变共享 Model。 */
	void ResetForPool();
	void ReleaseAudioComponent();

private:
	void HandlePlayStateChanged(const UAudioComponent* Component, EAudioComponentPlayState PlayState);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComponent;

	float FadeOutSeconds = 0.f;
	EAudioFaderCurve FadeOutCurve = EAudioFaderCurve::Linear;
	bool bStopping = false;
};
