#include "Game/Render/Effect/WuwaSlashFxComponent.h"

#include "Game/Render/Effect/WuwaSlashFxPreset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UWuwaSlashFxComponent::UWuwaSlashFxComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    bTickInEditor = true;
}

void UWuwaSlashFxComponent::PlayEffect(UWuwaSlashFxPreset* Preset, const FTransform& Placement,
    UWuwaGameplayAbilityBase* SourceAbility, int32 SkillHandle)
{
    if (!IsValid(Preset) || !GetWorld() || GetNetMode() == NM_DedicatedServer || Preset->Layers.IsEmpty()) return;
    if (SourceAbility && (!SourceAbility->IsSkillExecutionActive() || SourceAbility->GetSkillHandle() != SkillHandle)) return;

    // Bound the work even when a montage has accidentally duplicated effect events.
    if (Playbacks.Num() >= 16)
    {
        StopPlayback(Playbacks[0], true);
        Playbacks.RemoveAt(0);
    }
    FWuwaSlashFxPlayback& Playback = Playbacks.AddDefaulted_GetRef();
    Playback.Preset = Preset;
    Playback.Placement = Placement;
    Playback.Ability = SourceAbility;
    Playback.SkillHandle = SkillHandle;
    Playback.bTrackSkill = SourceAbility != nullptr;
    SpawnDueLayers(Playback);
    SetComponentTickEnabled(true);
}

void UWuwaSlashFxComponent::SpawnDueLayers(FWuwaSlashFxPlayback& Playback)
{
    if (Playback.bStopped || !IsValid(Playback.Preset)) return;
    for (int32 Index = 0; Index < Playback.Preset->Layers.Num(); ++Index)
    {
        const FWuwaSlashFxLayer& Layer = Playback.Preset->Layers[Index];
        if (Playback.SpawnedLayers.Contains(Index) || Playback.Elapsed < Layer.DelaySeconds) continue;
        Playback.SpawnedLayers.Add(Index);
        if (!IsValid(Layer.System)) continue;
        const FTransform Transform = Layer.GetTransformAtAge(Playback.Elapsed - Layer.DelaySeconds) * Playback.Placement;
        UNiagaraComponent* Particle = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, Layer.System, Transform.GetLocation(), Transform.Rotator(), Transform.GetScale3D(),
            true, true, ENCPoolMethod::None, false);
        if (Particle)
        {
            Particle->SetCastShadow(false);
            Playback.Particles.Add(Particle);
            if (Layer.LocalYawDegrees.GetRichCurveConst()->GetNumKeys() > 0)
                Playback.AnimatedLayers.Add(Index, Particle);
        }
    }
}

void UWuwaSlashFxComponent::StopPlayback(FWuwaSlashFxPlayback& Playback, bool bDestroy)
{
    Playback.bStopped = true;
    for (UNiagaraComponent* Particle : Playback.Particles)
    {
        if (!IsValid(Particle)) continue;
        if (bDestroy) Particle->DestroyComponent();
        else Particle->Deactivate(); // Finish the existing short-lived particles; stop any further emission.
    }
}

void UWuwaSlashFxComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    for (int32 Index = Playbacks.Num() - 1; Index >= 0; --Index)
    {
        FWuwaSlashFxPlayback& Playback = Playbacks[Index];
        Playback.Elapsed += FMath::Max(0.f, DeltaTime);
        if (!IsValid(Playback.Preset) || Playback.Elapsed >= Playback.Preset->MaximumLifetime)
        {
            StopPlayback(Playback, true);
            Playbacks.RemoveAtSwap(Index);
            continue;
        }

        if (!Playback.bStopped && Playback.bTrackSkill)
        {
            const UWuwaGameplayAbilityBase* Ability = Playback.Ability.Get();
            if (!Ability || !Ability->IsSkillExecutionActive() || Ability->GetSkillHandle() != Playback.SkillHandle)
                StopPlayback(Playback, false);
        }
        SpawnDueLayers(Playback);
        for (auto It = Playback.AnimatedLayers.CreateIterator(); It; ++It)
        {
            UNiagaraComponent* Particle = It.Value().Get();
            if (!IsValid(Particle) || Particle->IsComplete() || !Playback.Preset->Layers.IsValidIndex(It.Key()))
            {
                It.RemoveCurrent();
                continue;
            }
            const FWuwaSlashFxLayer& Layer = Playback.Preset->Layers[It.Key()];
            Particle->SetWorldTransform(Layer.GetTransformAtAge(Playback.Elapsed - Layer.DelaySeconds) * Playback.Placement);
        }
        Playback.Particles.RemoveAll([](const TObjectPtr<UNiagaraComponent>& Particle)
        {
            return !IsValid(Particle) || Particle->IsComplete();
        });
        if (Playback.Particles.IsEmpty() && (Playback.bStopped || Playback.SpawnedLayers.Num() >= Playback.Preset->Layers.Num()))
            Playbacks.RemoveAtSwap(Index);
    }
    SetComponentTickEnabled(!Playbacks.IsEmpty());
}

void UWuwaSlashFxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    for (FWuwaSlashFxPlayback& Playback : Playbacks) StopPlayback(Playback, true);
    Playbacks.Empty();
    Super::EndPlay(EndPlayReason);
}
