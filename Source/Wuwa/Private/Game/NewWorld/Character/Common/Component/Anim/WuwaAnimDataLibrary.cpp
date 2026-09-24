#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.h"

#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "GameFramework/Character.h"

bool UWuwaAnimDataLibrary::UpdateAnimationData(const UWuwaMovementComponent* Movement, UWuwaAnimLogicParams* Params)
{
	check(IsInGameThread());
	if (!IsValid(Params))
	{
		return false;
	}
	Params->Reset();
	if (!IsValid(Movement) || !IsValid(Movement->GetCharacterOwner())
		|| !IsValid(Movement->UpdatedComponent))
	{
		return false;
	}

	UpdateAnimInfoMove(*Movement, *Params);
	UpdateAnimInfoUnifiedState(*Movement, *Params);
	Params->bHasValidData = true;
	return true;
}

void UWuwaAnimDataLibrary::UpdateAnimInfoMove(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params)
{
	FWuwaAnimMoveData& Data = Params.MoveData;
	Data.ActorRotation = Movement.GetCharacterOwner()->GetActorQuat();
	Data.Velocity = Movement.Velocity;
	Data.Acceleration = Movement.GetCurrentAcceleration();
	// 与 Acceleration 分开：有输入但未产生速度/加速度仍然能被动画观察到。
	Data.InputVector = Movement.GetLastInputVector().GetClampedToMaxSize(1.0);
	Data.MaxSpeed = Movement.GetMaxSpeed();
}

void UWuwaAnimDataLibrary::UpdateAnimInfoUnifiedState(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params)
{
	FWuwaAnimStateData& Data = Params.StateData;
	Data.MovementMode = Movement.MovementMode;
	Data.CustomMovementMode = Movement.CustomMovementMode;
	Data.DesiredGait = Movement.GetDesiredGait();
	Data.AllowedGait = Movement.GetAllowedGait();
	Data.SprintDesire = Movement.GetSprintDesire();
	Data.bStateGround = Movement.IsMovingOnGround();
	Data.bStateAir = Movement.IsFalling();
	Data.bStateClimb = Movement.IsClimbing();
	Data.bIsCrouching = Movement.IsCrouching();
}

float UWuwaAnimDataLibrary::GetStartTimeFromSyncPosition(const UAnimSequence* Sequence,
                                                     const FMarkerSyncAnimPosition& SyncPosition)
{
    if (!Sequence || SyncPosition.PreviousMarkerName.IsNone() || SyncPosition.NextMarkerName.IsNone())
    {
        return 0.f; // 没拿到有效同步位置时从头播
    }
    return Sequence->GetFirstMatchingPosFromMarkerSyncPos(SyncPosition);
}
