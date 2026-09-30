#include "Editor/WuwaSlashFxPreviewCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "FXSystem.h"
#include "GameFramework/Character.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SlashFx.h"
#include "Game/Render/Effect/WuwaSlashFxPreset.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraWorldManager.h"
#include "PreviewScene.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSlashFxPreview, Log, All);

UWuwaSlashFxPreviewCommandlet::UWuwaSlashFxPreviewCommandlet()
{
	IsClient = true;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Render real Changli slash Niagara presets to PNG sequences and a diagnostic JSON; does not modify assets.");
	HelpUsage = TEXT("-run=WuwaSlashFxPreview -AllowCommandletRendering -RenderOffscreen [-Preset=DA_Attack01_01] [-NoCharacter|-Isolated] [-Times=0.04,0.12,0.22,0.35,0.55,0.85] [-Output=directory] [-Width=1280] [-Height=960]");
}

#if WITH_EDITOR
namespace WuwaSlashFxPreview
{
	TSharedRef<FJsonObject> InspectSimulation(UNiagaraComponent* Component, const TCHAR* Stage)
	{
		TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
		State->SetStringField(TEXT("stage"), Stage);
		State->SetBoolField(TEXT("registered"), Component->IsRegistered());
		State->SetBoolField(TEXT("active"), Component->IsActive());
		State->SetBoolField(TEXT("complete"), Component->IsComplete());
		State->SetBoolField(TEXT("worldHasFxSystem"), Component->GetWorld() && Component->GetWorld()->FXSystem);
		State->SetBoolField(TEXT("worldHasNiagaraDispatch"), FNiagaraGpuComputeDispatchInterface::Get(Component->GetWorld()) != nullptr);
		State->SetBoolField(TEXT("worldHasNiagaraManager"), FNiagaraWorldManager::Get(Component->GetWorld()) != nullptr);
		if (const UNiagaraSystem* System = Component->GetAsset())
		{
			State->SetBoolField(TEXT("systemReady"), System->IsReadyToRun());
			State->SetBoolField(TEXT("systemValid"), System->IsValid());
			State->SetBoolField(TEXT("systemAllowedByScalability"), System->IsAllowedByScalability());
			State->SetNumberField(TEXT("configuredEmitters"), System->GetNumEmitters());
			State->SetStringField(TEXT("spawnCompileStatus"), UEnum::GetValueAsString(System->GetSystemSpawnScript()->GetLastCompileStatus()));
			State->SetStringField(TEXT("updateCompileStatus"), UEnum::GetValueAsString(System->GetSystemUpdateScript()->GetLastCompileStatus()));
		}
		TArray<TSharedPtr<FJsonValue>> Emitters;
		int32 ParticleCount = 0;
		if (auto Controller = Component->GetSystemInstanceController(); Controller.IsValid())
		{
			State->SetBoolField(TEXT("instanceCreated"), true);
			if (FNiagaraSystemInstance* Instance = Controller->GetSystemInstance_Unsafe())
			{
				State->SetNumberField(TEXT("systemAge"), Instance->GetAge());
				State->SetStringField(TEXT("requestedState"), UEnum::GetValueAsString(Instance->GetRequestedExecutionState()));
				State->SetStringField(TEXT("actualState"), UEnum::GetValueAsString(Instance->GetActualExecutionState()));
				for (const auto& Emitter : Instance->GetEmitters())
				{
					TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
					Item->SetStringField(TEXT("name"), Emitter->GetEmitterHandle().GetName().ToString());
					Item->SetStringField(TEXT("state"), UEnum::GetValueAsString(Emitter->GetExecutionState()));
					Item->SetNumberField(TEXT("particles"), Emitter->GetNumParticles());
					Item->SetNumberField(TEXT("totalSpawned"), Emitter->GetTotalSpawnedParticles());
					ParticleCount += Emitter->GetNumParticles();
					Emitters.Add(MakeShared<FJsonValueObject>(Item));
				}
			}
		}
		else State->SetBoolField(TEXT("instanceCreated"), false);
		State->SetNumberField(TEXT("particleCount"), ParticleCount);
		State->SetArrayField(TEXT("emitters"), Emitters);
		return State;
	}

	struct FAuthoredPlacement
	{
		UAnimMontage* Montage = nullptr;
		UWuwaAnimNotify_SlashFx* Notify = nullptr;
		float NotifyTime = 0.f;
	};

	FAuthoredPlacement FindPlacement(const UWuwaSlashFxPreset* Preset)
	{
		for (int32 Attack = 1; Attack <= 5; ++Attack)
		{
			const FString Path = FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), Attack, Attack);
			if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path))
			{
				for (const FAnimNotifyEvent& Event : Montage->Notifies)
				{
					if (auto* Notify = Cast<UWuwaAnimNotify_SlashFx>(Event.Notify.Get()); Notify && Notify->Preset == Preset)
						return {Montage, Notify, Event.GetTime()};
				}
			}
		}
		return {};
	}

	bool SetAuthoredPose(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float MontageTime)
	{
		if (!Mesh || !Montage || Montage->SlotAnimTracks.IsEmpty()) return false;
		for (const FAnimSegment& Segment : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			float SequenceTime = 0.f;
			if (UAnimSequenceBase* Sequence = Segment.GetAnimationData(MontageTime, SequenceTime))
			{
				Mesh->SetAnimation(Sequence);
				Mesh->SetPosition(SequenceTime, false);
				Mesh->TickAnimation(0.f, false);
				Mesh->RefreshBoneTransforms(nullptr);
				Mesh->UpdateComponentToWorld();
				return true;
			}
		}
		return false;
	}

	bool CapturePixels(FPreviewScene& Scene, USceneCaptureComponent2D* Capture, UTextureRenderTarget2D* Target,
		TArray<FColor>& Pixels)
	{
		Scene.GetWorld()->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
		// First capture brings scene resources into the renderer; the second is the
		// actual sample. Simulation time is unchanged during both captures.
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			++GFrameCounter;
			Capture->CaptureScene();
			FlushRenderingCommands();
		}
		FReadSurfaceDataFlags Flags(RCM_UNorm);
		Flags.SetLinearToGamma(false); // FinalColorLDR has already been tonemapped.
		return Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)
			&& Pixels.Num() == Target->SizeX * Target->SizeY;
	}

	bool RenderSample(UWuwaSlashFxPreset* Preset, const FAuthoredPlacement& Authored, float Age,
		bool bShowCharacter, int32 Width, int32 Height, const FString& Directory,
		TSharedRef<FJsonObject> Record, int64& OutChangedPixels)
	{
		FPreviewScene Scene(FPreviewScene::ConstructionValues()
			.SetCreatePhysicsScene(false).ShouldSimulatePhysics(false).SetTransactional(false)
			.SetLightBrightness(4.f).SetSkyBrightness(0.35f).SetLightRotation(FRotator(-50.f, -35.f, 0.f)));
		UWorld* World = Scene.GetWorld();
		// UWorld::CreateFXSystem deliberately skips every commandlet, even one
		// launched with -AllowCommandletRendering. Niagara requires this dispatch
		// interface for CPU emitters too. Use the same public factory explicitly
		// for our render-enabled preview world; normal world cleanup owns it.
		if (!World->FXSystem && World->Scene)
			World->FXSystem = FFXSystemInterface::Create(World->GetFeatureLevel(), World->Scene);
		if (!FNiagaraGpuComputeDispatchInterface::Get(World) || !FNiagaraWorldManager::Get(World))
		{
			UE_LOG(LogWuwaSlashFxPreview, Error, TEXT("Preview world has no Niagara dispatch/manager after explicit FX initialization."));
			return false;
		}
		World->TimeSeconds = Age;
		World->RealTimeSeconds = Age;
		World->UnpausedTimeSeconds = Age;
		auto* Fill = NewObject<UPointLightComponent>(GetTransientPackage());
		Fill->SetIntensityUnits(ELightUnits::Lumens);
		Fill->SetIntensity(450.f);
		Fill->SetAttenuationRadius(1000.f);
		Fill->SetLightColor(FLinearColor(0.76f, 0.84f, 1.f));
		Fill->SetCastShadows(false);
		Scene.AddComponent(Fill, FTransform(FVector(100, -250, 300)));

		// Keep the capture backdrop empty. A transient floor material can fall back
		// to WorldGridMaterial while shader resources warm up, masking the FX.
		// A 50 cm grid gives the slash's dimensions a stable in-engine reference.
		if (ULineBatchComponent* Lines = Scene.GetLineBatcher())
		{
			for (int32 N = -6; N <= 6; ++N)
			{
				const float Offset = N * 50.f;
				Lines->DrawLine(FVector(-300, Offset, 0), FVector(300, Offset, 0), FLinearColor(0.045f, 0.055f, 0.07f), 0, 0.5f, 100.f);
				Lines->DrawLine(FVector(Offset, -300, 0), FVector(Offset, 300, 0), FLinearColor(0.045f, 0.055f, 0.07f), 0, 0.5f, 100.f);
			}
		}

		USkeletalMeshComponent* CharacterMesh = nullptr;
		FTransform Placement(FRotator::ZeroRotator, FVector(0, 0, 90));
		bool bAuthoredPose = false;
		if (bShowCharacter || Authored.Notify)
		{
			CharacterMesh = NewObject<USkeletalMeshComponent>(GetTransientPackage());
			CharacterMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/Model/Changli.Changli")));
			CharacterMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FTransform MeshTransform(FRotator(0, -90, 0), FVector::ZeroVector);
			if (UClass* CharacterClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Characters/Role/changli/BP_WuwaCharacterBase.BP_WuwaCharacterBase_C")))
			{
				const ACharacter* Defaults = CharacterClass->GetDefaultObject<ACharacter>();
				MeshTransform = Defaults->GetMesh()->GetRelativeTransform();
				MeshTransform.AddToTranslation(FVector(0, 0, Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
			}
			Scene.AddComponent(CharacterMesh, MeshTransform);
			CharacterMesh->InitAnim(true);
			bAuthoredPose = SetAuthoredPose(CharacterMesh, Authored.Montage, Authored.NotifyTime);
			if (Authored.Notify)
			{
				Placement = FTransform(Authored.Notify->RotationOffset, Authored.Notify->LocationOffset, Authored.Notify->Scale)
					* CharacterMesh->GetSocketTransform(Authored.Notify->SocketName);
			}
			if (Authored.Montage)
				SetAuthoredPose(CharacterMesh, Authored.Montage, FMath::Min(Authored.NotifyTime + Age * Authored.Montage->RateScale, Authored.Montage->GetPlayLength() - 0.001f));
			CharacterMesh->SetVisibility(bShowCharacter, true);
		}

		auto* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Target->ClearColor = FLinearColor(0.008f, 0.011f, 0.017f);
		Target->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
		auto* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
		Capture->TextureTarget = Target;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->FOVAngle = 45.f;
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->ShowFlags.SetScreenSpaceReflections(false);
		Capture->ShowFlags.SetLumenGlobalIllumination(false);
		Capture->ShowFlags.SetLumenReflections(false);
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = 0.f;
		Capture->PostProcessSettings.bOverride_BloomIntensity = true;
		Capture->PostProcessSettings.BloomIntensity = 0.45f;
		const FVector CameraPosition(330, -460, 350);
		const FVector LookAt(25, 0, 85);
		Scene.AddComponent(Capture, FTransform((LookAt - CameraPosition).Rotation(), CameraPosition));
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		TArray<FColor> Reference;
		if (!CapturePixels(Scene, Capture, Target, Reference)) return false;

		TArray<TSharedPtr<FJsonValue>> LayerRecords;
		int32 TotalParticles = 0;
		bool bAllLayersComplete = true;
		bool bAllLayersStarted = true;
		TArray<UNiagaraComponent*> LayerComponents;
		for (const FWuwaSlashFxLayer& Layer : Preset->Layers)
		{
			if (!Layer.System || Age < Layer.DelaySeconds) continue;
			auto* Particles = NewObject<UNiagaraComponent>(GetTransientPackage());
			Particles->SetCastShadow(false);
			Particles->SetAutoActivate(false);
			Particles->SetAutoDestroy(false);
			Particles->SetAllowScalability(false);
			Particles->SetForceSolo(true);
			Particles->SetAsset(Layer.System);
			Scene.AddComponent(Particles, Layer.GetTransformAtAge(0.f) * Placement);
			TArray<TSharedPtr<FJsonValue>> States;
			States.Add(MakeShared<FJsonValueObject>(InspectSimulation(Particles, TEXT("BeforeActivate"))));
			Particles->Activate(true);
			States.Add(MakeShared<FJsonValueObject>(InspectSimulation(Particles, TEXT("AfterActivate"))));
			const float LayerAge = FMath::Max(Age - Layer.DelaySeconds, 0.f);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(LayerAge * 240.f));
			const float Delta = FMath::Max(LayerAge / Steps, 0.0001f);
			Particles->AdvanceSimulation(1, Delta);
			States.Add(MakeShared<FJsonValueObject>(InspectSimulation(Particles, TEXT("AfterFirstSimulationStep"))));
			if (Steps > 1) Particles->AdvanceSimulation(Steps - 1, Delta);
			// Authored components are local-space, so the final layer transform also
			// rotates particles already simulated at this sample's age.
			Particles->SetWorldTransform(Layer.GetTransformAtAge(LayerAge) * Placement);
			States.Add(MakeShared<FJsonValueObject>(InspectSimulation(Particles, TEXT("AfterSimulation"))));
			Particles->MarkRenderDynamicDataDirty();
			LayerComponents.Add(Particles);
			TSharedRef<FJsonObject> LayerRecord = MakeShared<FJsonObject>();
			LayerRecord->SetStringField(TEXT("system"), Layer.System->GetPathName());
			LayerRecord->SetNumberField(TEXT("simulatedSeconds"), LayerAge);
			LayerRecord->SetBoolField(TEXT("active"), Particles->IsActive());
			LayerRecord->SetBoolField(TEXT("complete"), Particles->IsComplete());
			LayerRecord->SetArrayField(TEXT("executionStates"), States);
			bAllLayersComplete &= Particles->IsComplete();
			bAllLayersStarted &= Particles->GetSystemInstanceController().IsValid();
			int32 ParticleCount = 0;
			// AdvanceSimulation performs synchronous game-thread simulation, so the
			// instance data can be inspected here before any further world tick.
			if (auto Controller = Particles->GetSystemInstanceController(); Controller.IsValid())
			{
				if (FNiagaraSystemInstance* Instance = Controller->GetSystemInstance_Unsafe())
					for (const auto& Emitter : Instance->GetEmitters()) ParticleCount += Emitter->GetNumParticles();
			}
			LayerRecord->SetNumberField(TEXT("particleCount"), ParticleCount);
			TotalParticles += ParticleCount;
			LayerRecords.Add(MakeShared<FJsonValueObject>(LayerRecord));
		}

		TArray<FColor> Pixels;
		if (!CapturePixels(Scene, Capture, Target, Pixels)) return false;
		// Recapture the reference after scene warmup to reduce streaming/lighting
		// differences. Visibility changes do not advance Niagara simulation time.
		for (UNiagaraComponent* Component : LayerComponents)
		{
			Component->SetVisibility(false, true);
			Component->SetRenderingEnabled(false);
			Component->MarkRenderStateDirty();
		}
		if (!CapturePixels(Scene, Capture, Target, Reference)) return false;
		int64 ChangedPixels = 0;
		int64 WarmEffectPixels = 0;
		for (int32 Index = 0; Index < Pixels.Num(); ++Index)
		{
			const FColor& A = Pixels[Index];
			const FColor& B = Reference[Index];
			// Ignore dark-background tonemapper noise. These are still diagnostic
			// frame differences, not a mask containing exclusively effect pixels.
			const bool bBrightPixel = FMath::Max3(A.R, A.G, A.B) > 45 || FMath::Max3(B.R, B.G, B.B) > 45;
			if (bBrightPixel && FMath::Max3(FMath::Abs(int32(A.R) - B.R), FMath::Abs(int32(A.G) - B.G), FMath::Abs(int32(A.B) - B.B)) > 12)
				++ChangedPixels;
			if (int32(A.R) > int32(B.R) + 15 && A.R > A.B + 15 && A.R > 45)
				++WarmEffectPixels;
			Pixels[Index].A = 255;
		}
		OutChangedPixels += ChangedPixels;
		const FString FileName = FString::Printf(TEXT("%s_%s_%04dms.png"), *Preset->GetName(), bShowCharacter ? TEXT("character") : TEXT("isolated"), FMath::RoundToInt(Age * 1000.f));
		const FString OutputPath = Directory / FileName;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const bool bSaved = FFileHelper::SaveArrayToFile(Png, *OutputPath);
		Record->SetStringField(TEXT("preset"), Preset->GetPathName());
		Record->SetStringField(TEXT("image"), OutputPath);
		Record->SetNumberField(TEXT("secondsAfterNotify"), Age);
		Record->SetStringField(TEXT("montage"), GetPathNameSafe(Authored.Montage));
		Record->SetNumberField(TEXT("notifyAssetTime"), Authored.NotifyTime);
		Record->SetBoolField(TEXT("authoredCharacterPose"), bAuthoredPose);
		Record->SetStringField(TEXT("placement"), Placement.ToHumanReadableString());
		Record->SetNumberField(TEXT("particleCount"), TotalParticles);
		Record->SetBoolField(TEXT("allLayersStarted"), bAllLayersStarted);
		Record->SetBoolField(TEXT("allLayersComplete"), bAllLayersComplete);
		Record->SetNumberField(TEXT("warmEffectPixels"), static_cast<double>(WarmEffectPixels));
		Record->SetNumberField(TEXT("pixelsChangedByEffects"), static_cast<double>(ChangedPixels));
		Record->SetArrayField(TEXT("layers"), LayerRecords);
		UE_LOG(LogWuwaSlashFxPreview, Display, TEXT("%s: %.3f seconds, %d particles, %lld changed pixels -> %s"),
			*Preset->GetName(), Age, TotalParticles, ChangedPixels, *OutputPath);
		const bool bExpectedEarlyParticles = Age >= 0.02f && Age <= 0.12f;
		const bool bEarlySampleValid = !bExpectedEarlyParticles || (TotalParticles > 0 && WarmEffectPixels >= 64);
		const bool bLateSampleValid = Age < 0.85f || (TotalParticles == 0 && bAllLayersComplete);
		Record->SetBoolField(TEXT("expectedVisibilityAndLifetimeVerified"), bEarlySampleValid && bLateSampleValid);
		if (!bEarlySampleValid || !bLateSampleValid || !bAllLayersStarted)
			UE_LOG(LogWuwaSlashFxPreview, Error, TEXT("%s at %.3fs failed simulation/visibility/lifetime checks: particles=%d warmPixels=%lld allStarted=%d allComplete=%d"),
				*Preset->GetName(), Age, TotalParticles, WarmEffectPixels, bAllLayersStarted, bAllLayersComplete);
		return bSaved && bEarlySampleValid && bLateSampleValid && bAllLayersStarted;
	}
}
#endif

int32 UWuwaSlashFxPreviewCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaSlashFxPreview;
	if (!FApp::CanEverRender() || GUsingNullRHI)
	{
		UE_LOG(LogWuwaSlashFxPreview, Error, TEXT("A real RHI is required. Run with -AllowCommandletRendering -RenderOffscreen, without -NullRHI."));
		return 1;
	}
	FString Directory = FPaths::ProjectSavedDir() / TEXT("Diagnostics/ChangliSlashPreview");
	FString PresetFilter;
	FString TimeList;
	FParse::Value(*Params, TEXT("Output="), Directory);
	FParse::Value(*Params, TEXT("Preset="), PresetFilter);
	FParse::Value(*Params, TEXT("Times="), TimeList, false);
	Directory = FPaths::ConvertRelativePathToFull(Directory);
	IFileManager::Get().MakeDirectory(*Directory, true);
	int32 Width = 1280, Height = 960;
	FParse::Value(*Params, TEXT("Width="), Width);
	FParse::Value(*Params, TEXT("Height="), Height);
	Width = FMath::Clamp(Width, 256, 4096);
	Height = FMath::Clamp(Height, 256, 4096);
	const bool bShowCharacter = !FParse::Param(*Params, TEXT("NoCharacter")) && !FParse::Param(*Params, TEXT("Isolated"));
	TArray<float> Times{0.04f, 0.12f, 0.22f, 0.35f, 0.55f, 0.85f};
	if (!TimeList.IsEmpty())
	{
		Times.Reset();
		TArray<FString> Tokens;
		TimeList.ParseIntoArray(Tokens, TEXT(","), true);
		for (const FString& Token : Tokens)
		{
			float Value = 0.f;
			if (!LexTryParseString(Value, *Token) || !FMath::IsFinite(Value) || Value < 0.f || Value > 5.f) return 1;
			Times.Add(Value);
		}
		if (Times.IsEmpty()) return 1;
	}
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	Registry.Get().ScanPathsSynchronous({TEXT("/Game/Effects/ChangliSlash/Presets")}, true);
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game/Effects/ChangliSlash/Presets"));
	Filter.ClassPaths.Add(UWuwaSlashFxPreset::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Assets;
	Registry.Get().GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	TArray<TSharedPtr<FJsonValue>> Records;
	int32 PresetCount = 0;
	int32 Failures = 0;
	int32 MaximumLiveParticles = 0;
	int64 ChangedPixels = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (!PresetFilter.IsEmpty() && !Asset.AssetName.ToString().MatchesWildcard(PresetFilter)) continue;
		auto* Preset = Cast<UWuwaSlashFxPreset>(Asset.GetAsset());
		if (!Preset) { ++Failures; continue; }
		++PresetCount;
		bool bReady = !Preset->Layers.IsEmpty();
		for (const FWuwaSlashFxLayer& Layer : Preset->Layers)
		{
			if (!Layer.System) { bReady = false; continue; }
			Layer.System->WaitForCompilationComplete(true, false);
			bReady &= Layer.System->IsReadyToRun();
		}
		if (!bReady)
		{
			UE_LOG(LogWuwaSlashFxPreview, Error, TEXT("Preset contains missing or uncompiled systems: %s"), *Preset->GetPathName());
			++Failures;
			continue;
		}
		const FAuthoredPlacement Authored = FindPlacement(Preset);
		for (float Age : Times)
		{
			TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
			const bool bRendered = RenderSample(Preset, Authored, Age, bShowCharacter, Width, Height, Directory, Record, ChangedPixels);
			Record->SetBoolField(TEXT("rendered"), bRendered);
			double LiveParticles = 0;
			if (Record->TryGetNumberField(TEXT("particleCount"), LiveParticles))
				MaximumLiveParticles = FMath::Max(MaximumLiveParticles, static_cast<int32>(LiveParticles));
			Records.Add(MakeShared<FJsonValueObject>(Record));
			Failures += bRendered ? 0 : 1;
		}
	}
	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("captureMethod"), TEXT("Real Niagara CPU simulation and UE SceneCapture2D RHI rendering; frame differences compared with the same scene without effect components. Character sampled from the authored montage sequence; no gameplay or root-motion translation simulation."));
	Report->SetNumberField(TEXT("presetCount"), PresetCount);
	Report->SetNumberField(TEXT("failedSamples"), Failures);
	Report->SetNumberField(TEXT("maximumLiveParticles"), MaximumLiveParticles);
	Report->SetNumberField(TEXT("totalPixelsChangedByEffects"), static_cast<double>(ChangedPixels));
	Report->SetArrayField(TEXT("samples"), Records);
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	const bool bReportSaved = FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("preview-report.json")));
	if (PresetCount == 0 || Failures > 0 || ChangedPixels == 0 || MaximumLiveParticles == 0 || !bReportSaved)
	{
		UE_LOG(LogWuwaSlashFxPreview, Error, TEXT("Preview incomplete: presets=%d failures=%d visible effect pixels=%lld."), PresetCount, Failures, ChangedPixels);
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}
