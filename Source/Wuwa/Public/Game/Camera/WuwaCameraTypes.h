#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraTypes.generated.h"

/** A complete gameplay camera profile. Distances are centimetres, angles are degrees. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") float ArmLength = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0")) float MinArmLength = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="0")) float MaxArmLength = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FVector PivotOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FRotator RotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera", meta=(ClampMin="5", ClampMax="170")) float FieldOfView = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") float PitchMin = -89.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") float PitchMax = 89.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin="0")) float ZoomStep = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin="0")) float ZoomInterpSpeed = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") bool bAllowRotationInput = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") bool bAllowZoomInput = true;
	/** Skill profiles can ignore the user's exploration zoom without overwriting it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input") bool bUseUserZoom = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transition", meta=(ClampMin="0")) float BlendTime = .2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision") bool bCollisionTest = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision", meta=(ClampMin="1")) float ProbeRadius = 12.f;
};

/** Native code validates engine objects once and passes only values into script policy. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FRotator ControlRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float DeltaSeconds = 0.f;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ZoomDelta = 0.f;
};

/** Script output, before world collision and UE camera modifiers/shakes. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="Camera") bool bValid = false;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector Pivot = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float FieldOfView = 90.f;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ArmLength = 400.f;
	UPROPERTY(BlueprintReadWrite, Category="Camera") bool bCollisionTest = true;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ProbeRadius = 12.f;
};
