#pragma once

#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "WuwaStateEventRecorder.generated.h"

/** Test listener: records the per-dimension events of the movement state bridge in broadcast order. */
UCLASS(NotBlueprintable, Transient)
class UWuwaStateEventRecorder : public UObject
{
	GENERATED_BODY()

public:
	TArray<FString> Events;

	void Bind(UWuwaUnifiedStateBridgeComponent* State)
	{
		State->OnPositionStateChanged.AddUniqueDynamic(this, &UWuwaStateEventRecorder::HandlePosition);
		State->OnMoveStateChanged.AddUniqueDynamic(this, &UWuwaStateEventRecorder::HandleMove);
		State->OnGaitChanged.AddUniqueDynamic(this, &UWuwaStateEventRecorder::HandleGait);
		State->OnDirectionStateChanged.AddUniqueDynamic(this, &UWuwaStateEventRecorder::HandleDirection);
		State->OnWalkPreferenceChanged.AddUniqueDynamic(this, &UWuwaStateEventRecorder::HandleWalkOrRun);
	}

	int32 IndexOf(const FString& Event) const { return Events.IndexOfByKey(Event); }

	static FString Name(EWuwaPositionState Value) { return StaticEnum<EWuwaPositionState>()->GetNameStringByValue(static_cast<int64>(Value)); }
	static FString Name(EWuwaMoveState Value) { return StaticEnum<EWuwaMoveState>()->GetNameStringByValue(static_cast<int64>(Value)); }
	static FString Name(EWuwaGait Value) { return StaticEnum<EWuwaGait>()->GetNameStringByValue(static_cast<int64>(Value)); }
	static FString Name(EWuwaDirectionState Value) { return StaticEnum<EWuwaDirectionState>()->GetNameStringByValue(static_cast<int64>(Value)); }

	UFUNCTION() void HandlePosition(EWuwaPositionState OldState, EWuwaPositionState NewState)
	{
		Events.Add(FString::Printf(TEXT("Position %s->%s"), *Name(OldState), *Name(NewState)));
	}

	UFUNCTION() void HandleMove(EWuwaMoveState OldState, EWuwaMoveState NewState)
	{
		Events.Add(FString::Printf(TEXT("Move %s->%s"), *Name(OldState), *Name(NewState)));
	}

	UFUNCTION() void HandleGait(EWuwaGait OldGait, EWuwaGait NewGait)
	{
		Events.Add(FString::Printf(TEXT("Gait %s->%s"), *Name(OldGait), *Name(NewGait)));
	}

	UFUNCTION() void HandleDirection(EWuwaDirectionState OldState, EWuwaDirectionState NewState)
	{
		Events.Add(FString::Printf(TEXT("Direction %s->%s"), *Name(OldState), *Name(NewState)));
	}

	UFUNCTION() void HandleWalkOrRun(bool bWasWalk, bool bIsWalk)
	{
		Events.Add(FString::Printf(TEXT("WalkOrRun %s->%s"), bWasWalk ? TEXT("Walk") : TEXT("Run"), bIsWalk ? TEXT("Walk") : TEXT("Run")));
	}
};
