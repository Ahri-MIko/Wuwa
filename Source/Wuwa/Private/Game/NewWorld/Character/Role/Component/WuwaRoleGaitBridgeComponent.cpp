#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Engine/World.h"

//策略生命周期
void UWuwaRoleGaitBridgeComponent::InitializePolicy_Implementation() {}
void UWuwaRoleGaitBridgeComponent::RefreshPolicy_Implementation() {}
void UWuwaRoleGaitBridgeComponent::ResetRuntime_Implementation() {}
//步态请求
bool UWuwaRoleGaitBridgeComponent::RequestDesiredGait_Implementation(EWuwaGait) { return false; }
bool UWuwaRoleGaitBridgeComponent::RequestWalkRunToggle_Implementation() { return false; }
bool UWuwaRoleGaitBridgeComponent::CanRequestWalkRun_Implementation() const { return false; }
//步态封锁
void UWuwaRoleGaitBridgeComponent::SetGaitBlocked_Implementation(UObject*, EWuwaGait, bool) {}
bool UWuwaRoleGaitBridgeComponent::IsGaitAllowed_Implementation(EWuwaGait) const { return false; }

//冲刺窗口
void UWuwaRoleGaitBridgeComponent::OpenSprintWindow_Implementation(UObject*) {}
void UWuwaRoleGaitBridgeComponent::SampleSprintWindow_Implementation(UObject*, float, float) {}
void UWuwaRoleGaitBridgeComponent::CloseSprintWindow_Implementation(UObject*) {}
void UWuwaRoleGaitBridgeComponent::ResetSprintRequest_Implementation() {}
EWuwaSprintDesire UWuwaRoleGaitBridgeComponent::ReadSprintDesire_Implementation() const { return EWuwaSprintDesire::None; }

FWuwaMovementStateContext UWuwaRoleGaitBridgeComponent::ReadMovementContext() const
{
	check(IsInGameThread());
	FWuwaMovementStateContext Context;
	const AWuwaCharacter* Character = Cast<AWuwaCharacter>(GetOwner());
	const UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent().Get() : nullptr;
	if (!IsValid(Character) || Character->IsMovementStateEnding() || !IsValid(Movement)
		|| !GetWorld() || !GetWorld()->IsGameWorld()) return Context;
	Context.PositionState = Movement->ReadPositionState();
	Context.bHasMoveInput = Character->GetPlayerInputState().bHasMoveInput;
	Context.bIsCrouching = Movement->IsCrouching();
	Context.bCanDriveState = Character->GetLocalRole() != ROLE_SimulatedProxy;
	Context.bIsFlying = Movement->MovementMode == MOVE_Flying;
	Context.GroundSpeed = Movement->Velocity.Size2D();
	Context.VerticalSpeed = Movement->Velocity.Z;
	Context.GameTimeSeconds = GetWorld()->GetTimeSeconds();
	Context.TemporarySprintDuration = Movement->TemporarySprintDuration;
	Context.DefaultGait = Movement->GetInitialDesiredGait();
	return Context;
}

void UWuwaRoleGaitBridgeComponent::PublishGait(EWuwaGait Desired, EWuwaSprintDesire Desire, EWuwaGait LastMovingGait)
{
	check(IsInGameThread());
	DesiredGait = Desired;
	SprintDesire = Desire;
	StopGait = LastMovingGait;
}

void UWuwaRoleGaitBridgeComponent::HandleUnifiedStateChanged(const FWuwaUnifiedStateData&, const FWuwaUnifiedStateData&)
{
	RefreshPolicy();
}
