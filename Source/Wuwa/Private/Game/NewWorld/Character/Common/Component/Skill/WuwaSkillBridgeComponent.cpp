#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"

UWuwaSkillBridgeComponent::UWuwaSkillBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaSkillBridgeComponent::CanBeginSkill_Implementation(UWuwaGameplayAbilityBase*) const { return false; }
int32 UWuwaSkillBridgeComponent::TryBeginSkill_Implementation(UWuwaGameplayAbilityBase*) { return 0; }
bool UWuwaSkillBridgeComponent::EndSkill_Implementation(int32) { return false; }
FWuwaSkillData UWuwaSkillBridgeComponent::GetCurrentSkillData_Implementation() const { return {}; }
bool UWuwaSkillBridgeComponent::SetSkillAcceptInput_Implementation(int32, bool) { return false; }
bool UWuwaSkillBridgeComponent::SetMainSkillReadyEnd_Implementation(int32, bool) { return false; }
bool UWuwaSkillBridgeComponent::BeginSkillAcceptInputWindow_Implementation(int32, int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::EndSkillAcceptInputWindow_Implementation(int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::SetSkillInterruptLevel_Implementation(int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::CallAnimBreakPoint_Implementation(int32) { return false; }
bool UWuwaSkillBridgeComponent::RequestInputCacheClear_Implementation(int32, FGameplayTag) { return false; }

void UWuwaSkillBridgeComponent::BroadcastAnimBreakPoint(int32 SkillHandle)
{
	if (SkillHandle > 0 && GetCurrentSkillData().FightStateHandle == SkillHandle)
		OnAnimBreakPoint.Broadcast(this, SkillHandle);
}

void UWuwaSkillBridgeComponent::BroadcastInputCacheClear(int32 SkillHandle, FGameplayTag InputTag)
{
	if (SkillHandle > 0 && GetCurrentSkillData().FightStateHandle == SkillHandle)
		OnInputCacheClearRequested.Broadcast(this, SkillHandle, InputTag);
}
