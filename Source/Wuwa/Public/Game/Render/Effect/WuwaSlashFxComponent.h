#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WuwaSlashFxComponent.generated.h"

class UNiagaraComponent;
class UWuwaGameplayAbilityBase;
class UWuwaSlashFxPreset;

USTRUCT()
struct FWuwaSlashFxPlayback
{
    GENERATED_BODY()

    //一个总的特效的设定
    UPROPERTY() TObjectPtr<UWuwaSlashFxPreset> Preset;
    //这个相当于把特效在场景中播放出来的实例
    UPROPERTY() TArray<TObjectPtr<UNiagaraComponent>> Particles;
    //GA
    TWeakObjectPtr<UWuwaGameplayAbilityBase> Ability;
    FTransform Placement;
    //从播放到此已经过去了多久
    float Elapsed = 0.f;
    int32 SkillHandle = 0;
    //那些层已经生成过了
    TSet<int32> SpawnedLayers;
    // Only layers with an authored transform curve need per-frame transform updates.
    TMap<int32, TWeakObjectPtr<UNiagaraComponent>> AnimatedLayers;
    bool bTrackSkill = false;
    //有没有被打断
    bool bStopped = false;
};

/** Owns only effect playback. It never starts, ends or otherwise changes a skill. */
UCLASS(ClassGroup=(Effects), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaSlashFxComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UWuwaSlashFxComponent();

    void PlayEffect(UWuwaSlashFxPreset* Preset, const FTransform& Placement,UWuwaGameplayAbilityBase* SourceAbility = nullptr, int32 SkillHandle = 0);

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    int32 GetActivePlaybackCount() const { return Playbacks.Num(); }

private:
    UPROPERTY(Transient) TArray<FWuwaSlashFxPlayback> Playbacks;
    void SpawnDueLayers(FWuwaSlashFxPlayback& Playback);
    static void StopPlayback(FWuwaSlashFxPlayback& Playback, bool bDestroy);
};
