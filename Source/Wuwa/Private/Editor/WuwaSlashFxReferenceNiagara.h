#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UStaticMesh;
class UMaterialInterface;

namespace WuwaSlashFxReferenceNiagara
{
	/** Explicit authoring values reconstructed from a reference emitter, not runtime playback state. */
	struct FReferenceEmitterSpec
	{
		FName Name;
		UStaticMesh* Mesh = nullptr; // Null selects an elongated sprite emitter.
		UMaterialInterface* Material = nullptr;
		float Lifetime = .32f;
		int32 Count = 1; // Mesh layers always spawn one particle.
		int32 SortOrder = 0;
		FVector PivotOffset = FVector::ZeroVector;
		bool bLocalSpace = true;

		FVector2D SpriteSize = FVector2D(12, 40);
		float SpawnRadius = 0.f;
		/** Feather spawn ring in emitter-local XY; thickness is the full Z range in centimeters. */
		float RingHeight = 8.f;
		float RingArcDegrees = 360.f;
		float RingStartAngleDegrees = 0.f;
		bool bClockwise = false;
		float SpeedMin = 0.f;
		float SpeedMax = 0.f;
		FVector Gravity = FVector::ZeroVector;
		float Drag = 0.f;
		int32 RandomSeed = 913;
	};

	/** Replaces emitters in an already-owned system. Does not save assets or modify playback/notify state. */
	bool BuildReferenceSystem(UNiagaraSystem* System, const TArray<FReferenceEmitterSpec>& Specs);
}
