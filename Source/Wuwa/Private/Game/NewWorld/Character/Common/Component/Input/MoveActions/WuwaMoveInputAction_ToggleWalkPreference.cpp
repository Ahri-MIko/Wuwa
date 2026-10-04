#include "Game/NewWorld/Character/Common/Component/Input/MoveActions/WuwaMoveInputAction_ToggleWalkPreference.h"

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"

bool UWuwaMoveInputAction_ToggleWalkPreference::Execute_Implementation(const FWuwaMoveInputContext& Context) const
{
	const AWuwaCharacter* Character = Context.Character;
	const UWuwaMovementComponent* Movement = IsValid(Character) ? Character->GetWuwaMovementComponent().Get() : nullptr;
	UWuwaUnifiedStateBridgeComponent* State = IsValid(Character) ? Character->UnifiedStateComponent.Get() : nullptr;
	// 原作 WalkPress 先问 CMC 的 CanWalkPress；执行时再判断一次，按下后状态变了（比如已经起跳）就不切换。
	return IsValid(Movement) && IsValid(State) && Movement->CanToggleWalkPreference() && State->ToggleWalkPreference();
}
