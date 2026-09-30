#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"

EWuwaCombatInputResult UWuwaCombatInputRuntimeBridge::ProcessInput_Implementation(UWuwaAbilitySystemComponent* ASC, FWuwaInputEvent Input, float BufferLifetimeSeconds)
{
	return EWuwaCombatInputResult::Ignored;
}

void UWuwaCombatInputRuntimeBridge::ResetInput_Implementation()
{
}

EWuwaCombatInputResult UWuwaCombatInputRuntimeBridge::ProcessPendingInput_Implementation(UWuwaAbilitySystemComponent*)
{
	return EWuwaCombatInputResult::Ignored;
}

int32 UWuwaCombatInputRuntimeBridge::GetBufferedInputCount_Implementation() const { return 0; }
int32 UWuwaCombatInputRuntimeBridge::ClearBufferedInput_Implementation(FGameplayTag) { return 0; }
