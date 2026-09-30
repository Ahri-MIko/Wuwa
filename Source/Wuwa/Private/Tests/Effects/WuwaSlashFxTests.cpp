#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "FXSystem.h"
#include "MeshDescription.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "StaticMeshAttributes.h"
#include "UObject/Script.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SlashFx.h"
#include "Game/Render/Effect/WuwaSlashFxComponent.h"
#include "Game/Render/Effect/WuwaSlashFxPreset.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "../Combat/WuwaSkillTestAbility.h"

namespace WuwaSlashFxTests
{
    constexpr const TCHAR* SlashSystemPath =
        TEXT("/Game/Effects/ChangliSlash/Niagara/NS_ChangliSlash.NS_ChangliSlash");

    // These are animation positions, not stopwatch delays. The imported full-length
    // sequences are 1.25 times the original timeline; Attack05 is original Attack04_1.
    const TArray<TArray<float>> ExpectedNotifyTimes = {
        {0.244116f}, {0.220606f},
        {0.203034f, 0.465602f, 0.537434f, 0.632975f},
        {0.296237f}, {0.500000f, 0.750000f, 0.833333f, 0.916667f}
    };

    void CheckOneShotState(FAutomationTestBase& Test, const FNiagaraEmitterHandle& Handle)
    {
        if (Handle.GetEmitterMode() != ENiagaraEmitterMode::Stateless) return;
        // Inspect public serialized data without depending on Niagara's internal headers.
        const FObjectPropertyBase* EmitterProperty = FindFProperty<FObjectPropertyBase>(
            FNiagaraEmitterHandle::StaticStruct(), TEXT("StatelessEmitter"));
        UObject* Emitter = EmitterProperty
            ? EmitterProperty->GetObjectPropertyValue_InContainer(&Handle) : nullptr;
        if (!Test.TestNotNull(TEXT("Stateless handle owns its emitter"), Emitter)) return;
        const FStructProperty* StateProperty = FindFProperty<FStructProperty>(Emitter->GetClass(), TEXT("EmitterState"));
        if (!Test.TestNotNull(TEXT("Stateless emitter exposes its serialized lifecycle"), StateProperty)) return;
        const void* State = StateProperty->ContainerPtrToValuePtr<void>(Emitter);
        for (const TPair<FName, FString>& Expected : {
            TPair<FName, FString>(TEXT("LoopBehavior"), TEXT("Once")),
            TPair<FName, FString>(TEXT("LoopDurationMode"), TEXT("Fixed"))})
        {
            const FEnumProperty* Property = FindFProperty<FEnumProperty>(StateProperty->Struct, Expected.Key);
            if (!Test.TestNotNull(*FString::Printf(TEXT("Lifecycle exposes %s"), *Expected.Key.ToString()), Property)) continue;
            const void* Value = Property->ContainerPtrToValuePtr<void>(State);
            const int64 RawValue = Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
            Test.TestEqual(*FString::Printf(TEXT("%s has bounded %s"), *Handle.GetName().ToString(), *Expected.Key.ToString()),
                Property->GetEnum()->GetNameStringByValue(RawValue), Expected.Value);
        }
    }

    int32 CountNiagaraComponents(const UWorld* World)
    {
        int32 Count = 0;
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World && It->IsRegistered()) ++Count;
        }
        return Count;
    }

    struct FFixture
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        AActor* Owner = nullptr;
        AWuwaCharacter* Character = nullptr;
        UWuwaSlashFxComponent* Effects = nullptr;
        UWuwaAbilitySystemComponent* ASC = nullptr;
        UWuwaSkillTestAbility* Defaults = GetMutableDefault<UWuwaSkillTestAbility>();
        const bool OriginalMainSkill = Defaults->bIsMainSkill;
        const bool OriginalMoveOverride = Defaults->bOverridesMoveState;
        const int32 OriginalPriority = Defaults->InterruptLevel;
        const EWuwaSkillOverrideType OriginalOverrideType = Defaults->SkillOverrideType;

        ~FFixture()
        {
            if (ASC)
            {
                ASC->CancelAllAbilities();
                ASC->ClearActorInfo();
            }
            if (IsValid(Effects) && Effects->HasBegunPlay()) Effects->EndPlay(EEndPlayReason::Destroyed);
            if (World) World->DestroyWorld(false);
            Defaults->bIsMainSkill = OriginalMainSkill;
            Defaults->bOverridesMoveState = OriginalMoveOverride;
            Defaults->InterruptLevel = OriginalPriority;
            Defaults->SkillOverrideType = OriginalOverrideType;
        }

        bool Initialize(FAutomationTestBase& Test, bool bWithSkill = false)
        {
            if (!Test.TestNotNull(TEXT("Transient world exists"), World)) return false;
            FActorSpawnParameters Spawn;
            Spawn.ObjectFlags |= RF_Transient;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (bWithSkill)
            {
                Character = World->SpawnActor<AWuwaCharacter>(Spawn);
                Owner = Character;
            }
            else Owner = World->SpawnActor<AActor>(Spawn);
            if (!Test.TestNotNull(TEXT("Effect owner exists"), Owner)) return false;
            Effects = NewObject<UWuwaSlashFxComponent>(Owner);
            Owner->AddInstanceComponent(Effects);
            Effects->RegisterComponent();
            Effects->RegisterAllComponentTickFunctions(true);
            Effects->BeginPlay();

            if (bWithSkill)
            {
                Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
                if (!Test.TestTrue(TEXT("Movement policy assembles"), Character->EnsureMovementStateSystem())
                    || !Test.TestTrue(TEXT("Real managed skill policy assembles"), Character->EnsureSkillSystem())) return false;
                Character->RoleGaitComponent->RefreshPolicy();
                Defaults->bIsMainSkill = true;
                Defaults->bOverridesMoveState = false;
                Defaults->InterruptLevel = 100;
                Defaults->SkillOverrideType = EWuwaSkillOverrideType::None;
                ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
                ASC->RegisterComponent();
                ASC->InitAbilityActorInfo(Character, Character);
            }
            return true;
        }

        void Tick(float DeltaSeconds) const
        {
            World->TimeSeconds += DeltaSeconds;
            Effects->TickComponent(DeltaSeconds, LEVELTICK_All, &Effects->PrimaryComponentTick);
        }

        UWuwaGameplayAbilityBase* StartSkill(FAutomationTestBase& Test) const
        {
            const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UWuwaSkillTestAbility::StaticClass(), 1));
            if (!Test.TestTrue(TEXT("Skill activates through GAS"), ASC->TryActivateAbility(Handle, false))) return nullptr;
            const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
            if (Spec) for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
            {
                UWuwaGameplayAbilityBase* Ability = Cast<UWuwaGameplayAbilityBase>(Instance);
                if (IsValid(Ability) && Ability->IsSkillExecutionActive()) return Ability;
            }
            Test.AddError(TEXT("GAS did not retain the active skill instance."));
            return nullptr;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSlashFxAssetsTest,
    "Wuwa.Effects.Slash.AssetsAndAttackNotifyTiming",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSlashFxAssetsTest::RunTest(const FString& Parameters)
{
    using namespace WuwaSlashFxTests;
    TSet<UNiagaraSystem*> CheckedSystems;
    int32 TotalNotifies = 0;
    for (int32 Attack = 1; Attack <= 5; ++Attack)
    {
        const FString MontagePath = FString::Printf(
            TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), Attack, Attack);
        const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
        if (!TestNotNull(*MontagePath, Montage)) continue;
        TArray<const FAnimNotifyEvent*> SlashEvents;
        bool bHasReadyEnd = false;
        bool bHasMovementCancel = false;
        for (const FAnimNotifyEvent& Event : Montage->Notifies)
        {
            if (Cast<UWuwaAnimNotify_SlashFx>(Event.Notify)) SlashEvents.Add(&Event);
            bHasReadyEnd |= Cast<UWuwaAnimNotify_SkillReadyEndBridge>(Event.Notify) != nullptr;
            bHasMovementCancel |= Event.NotifyStateClass
                && Event.NotifyStateClass->GetClass()->GetFName() == TEXT("AnimNotifyState_MovementCancelWindow_C");
        }
        TestTrue(*FString::Printf(TEXT("Attack%02d retains SkillReadyEnd"), Attack), bHasReadyEnd);
        TestTrue(*FString::Printf(TEXT("Attack%02d retains MovementCancelWindow"), Attack), bHasMovementCancel);
        const TArray<float>& Expected = ExpectedNotifyTimes[Attack - 1];
        TestEqual(*FString::Printf(TEXT("Attack%02d has exactly the authored slash bursts"), Attack), SlashEvents.Num(), Expected.Num());
        SlashEvents.Sort([](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B) { return A.GetTime() < B.GetTime(); });
        for (int32 Index = 0; Index < SlashEvents.Num(); ++Index)
        {
            const FAnimNotifyEvent& Event = *SlashEvents[Index];
            const UWuwaAnimNotify_SlashFx* Notify = CastChecked<UWuwaAnimNotify_SlashFx>(Event.Notify);
            ++TotalNotifies;
            if (Expected.IsValidIndex(Index))
                TestTrue(TEXT("Slash placement matches the mapped animation moment"), FMath::IsNearlyEqual(Event.GetTime(), Expected[Index], 0.002f));
            TestEqual(TEXT("Slash notify always triggers"), Event.NotifyTriggerChance, 1.f);
            TestTrue(TEXT("Slash point is inside its montage"), Event.GetTime() > 0.f && Event.GetTime() < Montage->GetPlayLength());
            TestFalse(TEXT("Slash placement is finite"), Notify->LocationOffset.ContainsNaN() || Notify->RotationOffset.ContainsNaN() || Notify->Scale.ContainsNaN());
            if (!TestNotNull(TEXT("Slash notify has a generated preset"), Notify->Preset.Get())) continue;
            const FString ExpectedPreset = FString::Printf(
                TEXT("/Game/Effects/ChangliSlash/Presets/DA_Attack%02d_%02d.DA_Attack%02d_%02d"), Attack, Index + 1, Attack, Index + 1);
            TestEqual(TEXT("Each burst uses its own authored preset"), Notify->Preset->GetPathName(), ExpectedPreset);
            TestFalse(TEXT("Preset contains effect layers"), Notify->Preset->Layers.IsEmpty());
            TestTrue(TEXT("Preset has a finite safety lifetime"), FMath::IsFinite(Notify->Preset->MaximumLifetime) && Notify->Preset->MaximumLifetime > 0.f);
            for (const FWuwaSlashFxLayer& Layer : Notify->Preset->Layers)
            {
                TestTrue(TEXT("Layer delay can occur within the safety lifetime"), FMath::IsFinite(Layer.DelaySeconds)
                    && Layer.DelaySeconds >= 0.f && Layer.DelaySeconds < Notify->Preset->MaximumLifetime);
                TestFalse(TEXT("Layer transform is finite"), Layer.Transform.ContainsNaN());
                UNiagaraSystem* System = Layer.System;
                if (!TestNotNull(TEXT("Layer references an existing Niagara system"), System) || CheckedSystems.Contains(System)) continue;
                CheckedSystems.Add(System);
                System->WaitForCompilationComplete(false, false);
                TestTrue(*FString::Printf(TEXT("%s contains valid compiled scripts"), *System->GetName()), System->IsValid());
                // UE intentionally returns false from IsReadyToRun in NullRHI.
                // Actual particle simulation is verified by the RHI preview tool.
                if (FApp::CanEverRender())
                    TestTrue(*FString::Printf(TEXT("%s is ready to render"), *System->GetName()), System->IsReadyToRun());
                int32 EnabledRenderers = 0;
                int32 EnabledEmitters = 0;
                for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
                {
                    if (!Handle.GetIsEnabled()) continue;
                    ++EnabledEmitters;
                    Handle.ForEachEnabledRendererWithIndex([&EnabledRenderers](const UNiagaraRendererProperties*, int32) { ++EnabledRenderers; });
                    CheckOneShotState(*this, Handle);
                }
                TestTrue(TEXT("Generated system has an enabled emitter"), EnabledEmitters > 0);
                TestTrue(TEXT("Generated system has a visible renderer"), EnabledRenderers > 0);
            }
        }
    }
    TestEqual(TEXT("Five attacks expose eleven authored slash bursts"), TotalNotifies, 11);
    TestTrue(TEXT("Both arc and ember systems are exercised by the presets"), CheckedSystems.Num() >= 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSlashFxBoundedLifecycleTest,
    "Wuwa.Effects.Slash.EmptyBoundedAndEndPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSlashFxBoundedLifecycleTest::RunTest(const FString& Parameters)
{
    using namespace WuwaSlashFxTests;
    FFixture F;
    if (!F.Initialize(*this)) return false;
    TestFalse(TEXT("Idle component does not tick"), F.Effects->IsComponentTickEnabled());
    UWuwaSlashFxPreset* Preset = NewObject<UWuwaSlashFxPreset>(F.Owner);
    F.Effects->PlayEffect(nullptr, FTransform::Identity);
    F.Effects->PlayEffect(Preset, FTransform::Identity);
    TestEqual(TEXT("Null and empty presets do not allocate playback"), F.Effects->GetActivePlaybackCount(), 0);
    TestFalse(TEXT("Rejected presets do not enable idle ticking"), F.Effects->IsComponentTickEnabled());

    Preset->Layers.AddDefaulted();
    F.Effects->PlayEffect(Preset, FTransform::Identity);
    F.Tick(0.01f);
    TestEqual(TEXT("A missing system cannot leave a live playback"), F.Effects->GetActivePlaybackCount(), 0);
    TestFalse(TEXT("Missing-system cleanup returns to idle"), F.Effects->IsComponentTickEnabled());

    Preset->MaximumLifetime = 0.25f;
    Preset->Layers[0].DelaySeconds = 100.f;
    for (int32 Index = 0; Index < 64; ++Index) F.Effects->PlayEffect(Preset, FTransform::Identity);
    TestEqual(TEXT("Accidentally duplicated events retain at most sixteen playbacks"), F.Effects->GetActivePlaybackCount(), 16);
    TestTrue(TEXT("Pending delayed layers enable ticking"), F.Effects->IsComponentTickEnabled());
    F.Tick(0.1f);
    TestEqual(TEXT("Pending layers stay scheduled before the safety lifetime"), F.Effects->GetActivePlaybackCount(), 16);
    F.Tick(0.2f);
    TestEqual(TEXT("MaximumLifetime bounds even an unreachable layer delay"), F.Effects->GetActivePlaybackCount(), 0);
    TestFalse(TEXT("Expiration disables ticking"), F.Effects->IsComponentTickEnabled());
    F.Effects->PlayEffect(Preset, FTransform::Identity);
    F.Effects->EndPlay(EEndPlayReason::Destroyed);
    TestEqual(TEXT("EndPlay discards outstanding delayed work"), F.Effects->GetActivePlaybackCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSlashFxSkillLifetimeTest,
    "Wuwa.Effects.Slash.EndingSkillCancelsDelayedLayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSlashFxSkillLifetimeTest::RunTest(const FString& Parameters)
{
    using namespace WuwaSlashFxTests;
    FEditorScriptExecutionGuard Guard;
    FFixture F;
    if (!F.Initialize(*this, true)) return false;
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, SlashSystemPath);
    if (!TestNotNull(TEXT("The real generated slash system loads"), System)) return false;
    System->WaitForCompilationComplete(false, false);
    if (!TestTrue(TEXT("The delayed layer uses valid compiled scripts"), System->IsValid())) return false;
    if (FApp::CanEverRender() && !TestTrue(TEXT("The delayed layer is ready to render"), System->IsReadyToRun())) return false;
    UWuwaGameplayAbilityBase* Ability = F.StartSkill(*this);
    if (!TestNotNull(TEXT("A real main skill owns the playback"), Ability)) return false;
    const int32 SkillHandle = Ability->GetSkillHandle();
    TestTrue(TEXT("Active skill owns a valid lease"), SkillHandle > 0);

    UWuwaSlashFxPreset* Preset = NewObject<UWuwaSlashFxPreset>(F.Owner);
    Preset->MaximumLifetime = 2.f;
    Preset->Layers.AddDefaulted(); // An empty immediate layer must not break the delayed valid layer.
    FWuwaSlashFxLayer& Delayed = Preset->Layers.AddDefaulted_GetRef();
    Delayed.System = System;
    Delayed.DelaySeconds = 0.25f;
    const int32 InitialParticles = CountNiagaraComponents(F.World);
    F.Effects->PlayEffect(Preset, FTransform::Identity, Ability, SkillHandle + 1);
    TestEqual(TEXT("A stale skill handle cannot schedule a slash"), F.Effects->GetActivePlaybackCount(), 0);
    F.Effects->PlayEffect(Preset, FTransform::Identity, Ability, SkillHandle);
    F.Tick(0.1f);
    TestEqual(TEXT("The valid delayed layer stays scheduled while its skill is active"), F.Effects->GetActivePlaybackCount(), 1);
    TestEqual(TEXT("A delayed layer has not spawned early"), CountNiagaraComponents(F.World), InitialParticles);
    TestTrue(TEXT("The skill ends through its real ownership interface"), Ability->TryEndSkillExecution(SkillHandle));
    TestNull(TEXT("Skill manager has released the finished execution"), F.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
    F.Tick(0.2f); // Cross the layer's due time after the owning skill ended.
    TestEqual(TEXT("Ended skill discards the delayed playback before spawning"), F.Effects->GetActivePlaybackCount(), 0);
    TestEqual(TEXT("No stale Niagara component appears after the old skill ends"), CountNiagaraComponents(F.World), InitialParticles);
    TestFalse(TEXT("Canceled delayed work does not keep the component ticking"), F.Effects->IsComponentTickEnabled());
    F.Effects->PlayEffect(Preset, FTransform::Identity, Ability, SkillHandle);
    TestEqual(TEXT("A late callback cannot replay the ended skill's effect"), F.Effects->GetActivePlaybackCount(), 0);
    F.Tick(3.f);
    TestEqual(TEXT("Later ticks cannot resurrect the canceled layer"), CountNiagaraComponents(F.World), InitialParticles);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSlashFxLocalYawCurveTest,
    "Wuwa.Effects.Slash.RuntimeLocalYawCurveAndDelayedSpawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSlashFxLocalYawCurveTest::RunTest(const FString& Parameters)
{
    using namespace WuwaSlashFxTests;
    FFixture F;
    if (!F.Initialize(*this)) return false;
    UWuwaSlashFxPreset* Preset = NewObject<UWuwaSlashFxPreset>(F.Owner);
    Preset->MaximumLifetime = 3.f;
    Preset->Layers.SetNum(3);
    FWuwaSlashFxLayer& Animated = Preset->Layers[0];
    FWuwaSlashFxLayer& Stationary = Preset->Layers[1];
    FWuwaSlashFxLayer& Delayed = Preset->Layers[2];
    Animated.Transform = FTransform(FRotator(12, 23, -8), FVector(25, -40, 65), FVector(1.2, .8, 1.6));
    Stationary.Transform = FTransform(FRotator(-6, 45, 17), FVector(-85, 40, 35), FVector(.7, 1.4, 1.1));
    Delayed.Transform = FTransform(FRotator(18, -32, 6), FVector(90, 75, 45), FVector(1.1, 1.3, .9));
    Delayed.DelaySeconds = .25f;
    for (FWuwaSlashFxLayer* Layer : { &Animated, &Delayed })
    {
        FRichCurve* Curve = Layer->LocalYawDegrees.GetRichCurve();
        const FKeyHandle Start = Curve->AddKey(0.f, 10.f);
        const FKeyHandle End = Curve->AddKey(.5f, 90.f);
        Curve->SetKeyInterpMode(Start, RCIM_Linear);
        Curve->SetKeyInterpMode(End, RCIM_Linear);
    }
    const FTransform Placement(FRotator(-9, -37, 11), FVector(350, -225, 120), FVector(1.4, .9, 1.2));
    auto ExpectedTransform = [&](const FWuwaSlashFxLayer& Layer, float ExtraLocalYaw)
    {
        // An independent expectation: the curve rotates inside the authored local frame.
        // It must not orbit the layer's translation or replace its authored pitch/roll.
        const FQuat Rotation = Placement.GetRotation() * Layer.Transform.GetRotation()
            * FRotator(0, ExtraLocalYaw, 0).Quaternion();
        return FTransform(Rotation, Placement.TransformPosition(Layer.Transform.GetLocation()),
            Placement.GetScale3D() * Layer.Transform.GetScale3D());
    };
    auto CheckTransform = [&](const TCHAR* Label, const FTransform& Actual, const FTransform& Expected)
    {
        TestTrue(*FString::Printf(TEXT("%s: world location stays at its authored anchor"), Label),
            Actual.GetLocation().Equals(Expected.GetLocation(), .001));
        TestTrue(*FString::Printf(TEXT("%s: authored scale is preserved"), Label),
            Actual.GetScale3D().Equals(Expected.GetScale3D(), .001));
        TestTrue(*FString::Printf(TEXT("%s: yaw composes with the authored rotation"), Label),
            Actual.GetRotation().Equals(Expected.GetRotation(), .001));
    };
    CheckTransform(TEXT("Curve begins at ten degrees"), Animated.GetTransformAtAge(0.f) * Placement, ExpectedTransform(Animated, 10.f));
    CheckTransform(TEXT("Curve interpolates to fifty degrees"), Animated.GetTransformAtAge(.25f) * Placement, ExpectedTransform(Animated, 50.f));
    CheckTransform(TEXT("Curve reaches ninety degrees"), Animated.GetTransformAtAge(.5f) * Placement, ExpectedTransform(Animated, 90.f));
    CheckTransform(TEXT("Negative layer age clamps to its first key"), Animated.GetTransformAtAge(-1.f) * Placement, ExpectedTransform(Animated, 10.f));
    CheckTransform(TEXT("Empty curve preserves its authored transform"), Stationary.GetTransformAtAge(.5f) * Placement, ExpectedTransform(Stationary, 0.f));

    if (!FApp::CanEverRender())
    {
        AddWarning(TEXT("Transform values passed; actual PlayEffect/Niagara component checks require RHI and -AllowCommandletRendering. NullRHI cannot spawn Niagara components."));
        return true;
    }
    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, SlashSystemPath);
    if (!TestNotNull(TEXT("Runtime curve test loads the real slash system"), System)) return false;
    System->WaitForCompilationComplete(false, false);
    if (!TestTrue(TEXT("Runtime curve test system is ready"), System->IsReadyToRun())) return false;
    // Commandlet worlds skip CreateFXSystem even with rendering allowed. The temporary
    // world's ordinary cleanup owns this system, just as in the Niagara preview tool.
    if (!F.World->FXSystem && F.World->Scene)
        F.World->FXSystem = FFXSystemInterface::Create(F.World->GetFeatureLevel(), F.World->Scene);
    if (!TestNotNull(TEXT("Runtime curve test has a real world FX system"), F.World->FXSystem)) return false;
    for (FWuwaSlashFxLayer& Layer : Preset->Layers) Layer.System = System;
    const int32 BaselineComponents = CountNiagaraComponents(F.World);
    F.Effects->PlayEffect(Preset, Placement);
    TestEqual(TEXT("Only the two immediate layers spawn at playback start"), CountNiagaraComponents(F.World), BaselineComponents + 2);
    auto FindParticle = [&](const FWuwaSlashFxLayer& Layer) -> UNiagaraComponent*
    {
        const FVector Location = ExpectedTransform(Layer, 0).GetLocation();
        for (TObjectIterator<UNiagaraComponent> It; It; ++It)
            if (IsValid(*It) && It->IsRegistered() && It->GetWorld() == F.World && It->GetAsset() == System
                && It->GetComponentLocation().Equals(Location, .001)) return *It;
        return nullptr;
    };
    TWeakObjectPtr<UNiagaraComponent> AnimatedParticle = FindParticle(Animated);
    TWeakObjectPtr<UNiagaraComponent> StationaryParticle = FindParticle(Stationary);
    if (!TestNotNull(TEXT("Animated layer creates a real component"), AnimatedParticle.Get())
        || !TestNotNull(TEXT("Unanimated layer creates a real component"), StationaryParticle.Get())) return false;
    CheckTransform(TEXT("Spawned animated component"), AnimatedParticle->GetComponentTransform(), ExpectedTransform(Animated, 10));
    const FTransform StationaryAtSpawn = StationaryParticle->GetComponentTransform();
    F.Tick(.125f);
    TestEqual(TEXT("Delayed layer remains unspawned before its deadline"), CountNiagaraComponents(F.World), BaselineComponents + 2);
    if (!TestTrue(TEXT("Both immediate components survive the transform-only tick"), AnimatedParticle.IsValid() && StationaryParticle.IsValid())) return false;
    CheckTransform(TEXT("Runtime tick advances animated yaw"), AnimatedParticle->GetComponentTransform(), ExpectedTransform(Animated, 30));
    CheckTransform(TEXT("Runtime tick does not move a layer without a curve"), StationaryParticle->GetComponentTransform(), StationaryAtSpawn);

    F.Tick(.125f); // The delayed layer is born exactly at its .25-second deadline.
    TWeakObjectPtr<UNiagaraComponent> DelayedParticle = FindParticle(Delayed);
    if (!TestNotNull(TEXT("Delayed layer spawns when due"), DelayedParticle.Get())) return false;
    TestEqual(TEXT("All three layers now exist"), CountNiagaraComponents(F.World), BaselineComponents + 3);
    CheckTransform(TEXT("Delayed layer starts at curve age zero, not playback age"), DelayedParticle->GetComponentTransform(), ExpectedTransform(Delayed, 10));
    F.Tick(.25f);
    if (!TestTrue(TEXT("Animated components remain available for the runtime check"), AnimatedParticle.IsValid() && DelayedParticle.IsValid())) return false;
    CheckTransform(TEXT("Immediate layer reaches its last curve key"), AnimatedParticle->GetComponentTransform(), ExpectedTransform(Animated, 90));
    CheckTransform(TEXT("Delayed layer advances only its own elapsed time"), DelayedParticle->GetComponentTransform(), ExpectedTransform(Delayed, 50));
    if (StationaryParticle.IsValid())
        CheckTransform(TEXT("Unanimated layer stays fixed through later ticks"), StationaryParticle->GetComponentTransform(), StationaryAtSpawn);
    else AddError(TEXT("The unanimated component disappeared during transform-only ticks."));

    F.Effects->EndPlay(EEndPlayReason::Destroyed);
    TestEqual(TEXT("EndPlay releases all curve playback state"), F.Effects->GetActivePlaybackCount(), 0);
    TestEqual(TEXT("EndPlay destroys every spawned Niagara component"), CountNiagaraComponents(F.World), BaselineComponents);
    for (TWeakObjectPtr<UNiagaraComponent> Particle : { AnimatedParticle, StationaryParticle, DelayedParticle })
        TestTrue(TEXT("A captured component cannot remain registered after owner EndPlay"), !Particle.IsValid() || !Particle->IsRegistered());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSlashFxImportedReferenceArtTest,
    "Wuwa.Effects.Slash.ImportedReferenceGeometryAndLayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSlashFxImportedReferenceArtTest::RunTest(const FString& Parameters)
{
    // Independently audited PSKX geometry counts, recorded in ImportedGeometry.json.
    // Check the saved source mesh description: render vertices may be legitimately split by tangents.
    struct FExpectedMesh { const TCHAR* Name; int32 Points; int32 Wedges; int32 Triangles; bool bHasAlphaMask; };
    const FExpectedMesh Meshes[] = {
        { TEXT("Mod_Changli_Daoguang_140003_a3"), 115, 115, 176, true },
        { TEXT("Mod_Changli_Daoguang_140004"), 93, 93, 120, false },
        { TEXT("Mod_Changli_Daoguang_140005"), 186, 186, 240, true },
        { TEXT("Mod_Changli_Zhuan_140001"), 85, 85, 128, true }
    };
    for (const FExpectedMesh& Expected : Meshes)
    {
        const FString Path = FString::Printf(TEXT("/Game/Effects/ChangliSlash/Reference/Mesh/%s.%s"), Expected.Name, Expected.Name);
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
        if (!TestNotNull(*Path, Mesh)) continue;
        const FMeshDescription* Description = Mesh->GetMeshDescription(0);
        if (!TestNotNull(TEXT("Imported mesh retains an editable source description"), Description)) continue;
        TestEqual(*FString::Printf(TEXT("%s preserves original positions"), Expected.Name), Description->Vertices().Num(), Expected.Points);
        TestEqual(*FString::Printf(TEXT("%s preserves original wedges"), Expected.Name), Description->VertexInstances().Num(), Expected.Wedges);
        TestEqual(*FString::Printf(TEXT("%s preserves original faces"), Expected.Name), Description->Triangles().Num(), Expected.Triangles);
        const FStaticMeshConstAttributes Attributes(*Description);
        TestEqual(TEXT("Original UV channel survives import"), Attributes.GetVertexInstanceUVs().GetNumChannels(), 1);
        const auto Colors = Attributes.GetVertexInstanceColors();
        float MinimumAlpha = 1.f;
        float MaximumAlpha = 0.f;
        for (FVertexInstanceID Instance : Description->VertexInstances().GetElementIDs())
        {
            MinimumAlpha = FMath::Min(MinimumAlpha, Colors[Instance].W);
            MaximumAlpha = FMath::Max(MaximumAlpha, Colors[Instance].W);
        }
        if (Expected.bHasAlphaMask)
            TestTrue(*FString::Printf(TEXT("%s keeps its vertex alpha silhouette mask"), Expected.Name), MinimumAlpha < .99f && MaximumAlpha > MinimumAlpha);
        else
            TestTrue(TEXT("A mesh without source vertex colors defaults to white alpha"), FMath::IsNearlyEqual(MinimumAlpha, 1.f) && FMath::IsNearlyEqual(MaximumAlpha, 1.f));
    }

    UWuwaSlashFxPreset* Preset = LoadObject<UWuwaSlashFxPreset>(nullptr,
        TEXT("/Game/Effects/ChangliSlash/Presets/DA_Attack01_01.DA_Attack01_01"));
    if (!TestNotNull(TEXT("Attack01 reference preset loads"), Preset)) return false;
    TestEqual(TEXT("Attack01 preserves four authored source effect placements"), Preset->Layers.Num(), 4);
    const TCHAR* ExpectedNames[] = {
        TEXT("NS_Reference_Attack01_N_Dg"), TEXT("NS_Reference_Attack01_N_Dg1"),
        TEXT("NS_Reference_Attack01_N1_Dg"), TEXT("NS_Reference_Attack01_N_Dg1")
    };
    TSet<UNiagaraSystem*> Systems;
    for (int32 Index = 0; Index < Preset->Layers.Num(); ++Index)
    {
        const FWuwaSlashFxLayer& Layer = Preset->Layers[Index];
        if (!TestNotNull(TEXT("Every reconstructed source placement has a system"), Layer.System.Get())) continue;
        if (Index < UE_ARRAY_COUNT(ExpectedNames))
        {
            const FString Path = FString::Printf(TEXT("/Game/Effects/ChangliSlash/Reference/Niagara/%s.%s"), ExpectedNames[Index], ExpectedNames[Index]);
            TestEqual(TEXT("Placement keeps its correct source-system variant"), Layer.System->GetPathName(), Path);
        }
        if (Index == 1 || Index == 3)
            TestEqual(TEXT("Feather placements preserve all three authored local-yaw curve keys"), Layer.LocalYawDegrees.GetRichCurveConst()->GetNumKeys(), 3);
        else
            TestEqual(TEXT("Other placements have no invented transform curve"), Layer.LocalYawDegrees.GetRichCurveConst()->GetNumKeys(), 0);
        Systems.Add(Layer.System);
    }
    TestEqual(TEXT("Four placements share three reconstructed source systems"), Systems.Num(), 3);
    for (UNiagaraSystem* System : Systems)
    {
        System->WaitForCompilationComplete(false, false);
        TestTrue(TEXT("Reference system compiled successfully"), System->IsValid());
        int32 MeshRenderers = 0;
        int32 SpriteRenderers = 0;
        for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
        {
            TestTrue(TEXT("Every reconstructed emitter is enabled"), Handle.GetIsEnabled());
            Handle.ForEachEnabledRendererWithIndex([&](const UNiagaraRendererProperties* Renderer, int32)
            {
                if (const auto* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
                {
                    ++MeshRenderers;
                    TestEqual(TEXT("Each reference mesh emitter renders one independent mesh"), MeshRenderer->Meshes.Num(), 1);
                    if (MeshRenderer->Meshes.Num() == 1)
                        TestNotNull(TEXT("Reference mesh renderer has geometry"), MeshRenderer->Meshes[0].Mesh.Get());
                    TestTrue(TEXT("Reference mesh emitter enables its layer material"), bool(MeshRenderer->bOverrideMaterials));
                    TestEqual(TEXT("Reference mesh emitter has exactly one material override"), MeshRenderer->OverrideMaterials.Num(), 1);
                    if (MeshRenderer->OverrideMaterials.Num() == 1)
                        TestNotNull(TEXT("Reference layer material exists"), MeshRenderer->OverrideMaterials[0].ExplicitMat.Get());
                }
                else if (const auto* SpriteRenderer = Cast<UNiagaraSpriteRendererProperties>(Renderer))
                {
                    ++SpriteRenderers;
                    TestNotNull(TEXT("Feather sprite has its reconstructed material"), SpriteRenderer->Material.Get());
                }
                else AddError(TEXT("Unexpected renderer type in the reconstructed first attack."));
            });
        }
        if (System->GetFName() == TEXT("NS_Reference_Attack01_N_Dg"))
        {
            TestEqual(TEXT("Main reference system keeps four mesh layers"), MeshRenderers, 4);
            TestEqual(TEXT("Main reference system does not add invented sprites"), SpriteRenderers, 0);
        }
        else if (System->GetFName() == TEXT("NS_Reference_Attack01_N_Dg1"))
        {
            TestEqual(TEXT("Feather source system keeps two sprite layers"), SpriteRenderers, 2);
            TestEqual(TEXT("Feather source system contains no mesh renderer"), MeshRenderers, 0);
        }
        else if (System->GetFName() == TEXT("NS_Reference_Attack01_N1_Dg"))
        {
            TestEqual(TEXT("Secondary reference system keeps two mesh layers"), MeshRenderers, 2);
            TestEqual(TEXT("Secondary reference system contains no sprite renderer"), SpriteRenderers, 0);
        }
        else AddError(FString::Printf(TEXT("Unexpected reference system: %s"), *System->GetPathName()));
    }
    return true;
}

#endif
