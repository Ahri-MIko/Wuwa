#include "Game/Camera/WuwaPlayerCameraManager.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCamera, Log, All);

AWuwaPlayerCameraManager::AWuwaPlayerCameraManager()
{
	RuntimeClass = TSoftClassPtr<UWuwaCameraRuntimeBridge>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaCameraRuntime_C")));
}

FWuwaCameraSettings AWuwaPlayerCameraManager::GetBaseSettings() const
{
	return IsValid(DefaultMode) ? DefaultMode->Settings : DefaultSettings;
}

bool AWuwaPlayerCameraManager::EnsureRuntime()
{
	if (IsValid(Runtime)) return true;
	UClass* Class = RuntimeClass.LoadSynchronous();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		if (!bReportedMissingRuntime)
		{
			UE_LOG(LogWuwaCamera, Error, TEXT("Camera runtime unavailable: %s. Build ManagedWuwa before playing."), *RuntimeClass.ToString());
			bReportedMissingRuntime = true;
		}
		return false;
	}
	Runtime = NewObject<UWuwaCameraRuntimeBridge>(this, Class);
	return IsValid(Runtime);
}

bool AWuwaPlayerCameraManager::TryGetGameplayContext(APlayerController*& OutPlayer, APawn*& OutPawn)
{
	OutPlayer = GetOwningPlayerController();
	OutPawn = IsValid(OutPlayer) ? OutPlayer->GetPawn() : nullptr;
	if (!IsValid(OutPlayer) || !OutPlayer->IsLocalController() || !IsValid(OutPawn)) return false;
	if (!EnsureRuntime()) return false;
	if (RuntimePawn.Get() != OutPawn)
	{
		NotifyPawnChanged();
		RuntimePawn = OutPawn;
	}
	return true;
}

void AWuwaPlayerCameraManager::NotifyPawnChanged()
{
	ResetCameraInput();
	RuntimePawn.Reset();
	LastEvaluatedFrame = MAX_uint64;
	CachedView = {};
	if (IsValid(Runtime)) Runtime->ResetCamera();
}

void AWuwaPlayerCameraManager::ResetCameraInput()
{
	PendingZoom = 0.f;
}

bool AWuwaPlayerCameraManager::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	const auto& InputTags = FWuwaGameTags::Get();
	const bool bLook = InputEvent.InputTag == InputTags.Player_Common_Camera_Rotate;
	const bool bZoom = InputEvent.InputTag == InputTags.Player_Common_Camera_Zoom;
	if (!bLook && !bZoom) return false;
	// Started and Triggered may both arrive in the first frame. Consume the axis only once.
	if (InputEvent.Phase != EWuwaInputPhase::Triggered) return true;
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn)) return false;
	// External CameraActor/Sequencer view targets own their input policy.
	if (Player->GetViewTarget() != Pawn || Player->IsLookInputIgnored()) return true;
	if (bLook && InputEvent.Value.GetValueType() == EInputActionValueType::Axis2D)
	{
		const FVector2D Axis = InputEvent.Value.Get<FVector2D>();
		if (!Axis.ContainsNaN())
		{
			// Preserve the project's existing sensitivity/inversion through UE's input path.
			Player->AddYawInput(Axis.X);
			Player->AddPitchInput(Axis.Y);
		}
	}
	else if (bZoom && InputEvent.Value.GetValueType() == EInputActionValueType::Axis1D)
	{
		const float Delta = InputEvent.Value.Get<float>();
		if (FMath::IsFinite(Delta)) PendingZoom = FMath::Clamp(PendingZoom + Delta, -100.f, 100.f);
	}
	return true;
}

void AWuwaPlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	APlayerController* Player;
	APawn* Pawn;
	if (TryGetGameplayContext(Player, Pawn))
	{
		const auto Settings = Runtime->ResolveSettings(GetBaseSettings());
		ViewPitchMin = Settings.PitchMin;
		ViewPitchMax = Settings.PitchMax;
		if (!Settings.bAllowRotationInput || Player->GetViewTarget() != Pawn) OutDeltaRot = FRotator::ZeroRotator;
	}
	Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
}

void AWuwaPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);
	// An external view must never accumulate wheel events to replay on returning to gameplay.
	PendingZoom = 0.f;
}

void AWuwaPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn) || OutVT.Target != Pawn)
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}
	if (LastEvaluatedFrame != GFrameCounter)
	{
		FWuwaCameraFrame Frame;
		Frame.TargetLocation = Pawn->GetActorLocation();
		Frame.ControlRotation = Player->GetControlRotation();
		Frame.DeltaSeconds = DeltaTime;
		Frame.ZoomDelta = PendingZoom;
		PendingZoom = 0.f;
		CachedView = Runtime->EvaluateCamera(Frame, GetBaseSettings());
		LastEvaluatedFrame = GFrameCounter;
	}
	if (!CachedView.bValid || CachedView.Location.ContainsNaN() || CachedView.Rotation.ContainsNaN())
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}
	OutVT.POV.Location = ResolveCollision(CachedView, Pawn);
	OutVT.POV.Rotation = CachedView.Rotation;
	OutVT.POV.FOV = CachedView.FieldOfView;
}

FVector AWuwaPlayerCameraManager::ResolveCollision(const FWuwaCameraView& View, const APawn* Pawn) const
{
	if (!View.bCollisionTest || !GetWorld()) return View.Location;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WuwaCamera), false, Pawn);
	Query.AddIgnoredActor(this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, View.Pivot, View.Location, FQuat::Identity,
		ECC_Camera, FCollisionShape::MakeSphere(FMath::Max(1.f, View.ProbeRadius)), Query);
	return bHit ? (Hit.bStartPenetrating ? View.Pivot : Hit.Location) : View.Location;
}

int32 AWuwaPlayerCameraManager::PushCameraMode(UObject* Source, UWuwaCameraMode* Mode)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!IsValid(Source) || !IsValid(Mode) || !TryGetGameplayContext(Player, Pawn)) return 0;
	return Runtime->PushCameraMode(Source, Mode->Settings, Mode->Priority);
}

bool AWuwaPlayerCameraManager::PopCameraMode(int32 Handle)
{
	return IsValid(Runtime) && Handle > 0 && Runtime->PopCameraMode(Handle);
}

void AWuwaPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyPawnChanged();
	Runtime = nullptr;
	Super::EndPlay(EndPlayReason);
}
