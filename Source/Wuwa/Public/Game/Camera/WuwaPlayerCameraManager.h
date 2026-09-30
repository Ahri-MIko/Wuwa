#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "Game/Camera/WuwaCameraTypes.h"
#include "WuwaPlayerCameraManager.generated.h"

class UWuwaCameraMode;
class UWuwaCameraRuntimeBridge;

/** Per-player engine adapter: input routing, lifetime, collision and final UE POV. */
UCLASS()
class WUWA_API AWuwaPlayerCameraManager : public APlayerCameraManager, public IIWuwaInputRouteHandler
{
	GENERATED_BODY()
public:
	AWuwaPlayerCameraManager();
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
	virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;
	virtual void UpdateCamera(float DeltaTime) override;
	void NotifyPawnChanged();
	void ResetCameraInput();

	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") int32 PushCameraMode(UObject* Source, UWuwaCameraMode* Mode);
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") bool PopCameraMode(int32 Handle);
	UFUNCTION(BlueprintPure, Category="Wuwa|Camera") UWuwaCameraRuntimeBridge* GetCameraRuntime() const { return Runtime; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Camera") FWuwaCameraSettings DefaultSettings;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Camera") TObjectPtr<UWuwaCameraMode> DefaultMode;
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|Camera") TSoftClassPtr<UWuwaCameraRuntimeBridge> RuntimeClass;

	
protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EnsureRuntime();
	bool TryGetGameplayContext(APlayerController*& OutPlayer, APawn*& OutPawn);
	FWuwaCameraSettings GetBaseSettings() const;
	FVector ResolveCollision(const FWuwaCameraView& View, const APawn* Pawn) const;
	UPROPERTY(Transient) TObjectPtr<UWuwaCameraRuntimeBridge> Runtime;
	TWeakObjectPtr<APawn> RuntimePawn;
	float PendingZoom = 0.f;
	uint64 LastEvaluatedFrame = MAX_uint64;
	FWuwaCameraView CachedView;
	bool bReportedMissingRuntime = false;
};
