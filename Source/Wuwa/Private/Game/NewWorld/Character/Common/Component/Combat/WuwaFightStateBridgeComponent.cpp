#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"

UWuwaFightStateBridgeComponent::UWuwaFightStateBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 未装配托管类时拒绝请求，不在原生层复制另一套规则。
bool UWuwaFightStateBridgeComponent::CheckSwitchState_Implementation(EWuwaFightState, int32) const { return false; }
int32 UWuwaFightStateBridgeComponent::TrySwitchState_Implementation(EWuwaFightState, int32) { return 0; }
bool UWuwaFightStateBridgeComponent::ExitState_Implementation(int32) { return false; }
void UWuwaFightStateBridgeComponent::ResetState_Implementation() {}

void UWuwaFightStateBridgeComponent::PublishFightState(FWuwaFightStateData NewState)
{
	check(IsInGameThread());
	if (StateData.State == NewState.State && StateData.SubStatePriority == NewState.SubStatePriority
		&& StateData.Handle == NewState.Handle)
	{
		return;
	}

	const FWuwaFightStateData OldState = StateData;
	StateData = NewState;
	// 先提交再广播；用局部副本防止同步回调改变本次事件参数。
	OnFightStateChanged.Broadcast(OldState, NewState);
}
