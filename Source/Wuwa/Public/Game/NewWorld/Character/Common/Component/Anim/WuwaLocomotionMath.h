#pragma once

#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h"

/** 无 UObject 访问的表现层计算，可独立测试；并非鸣潮原生算法。 */
namespace WuwaLocomotion
{
	inline FWuwaVelocityBlend CalculateVelocityBlend(const FVector& LocalVelocity)
	{
		FWuwaVelocityBlend Result;
		const double HorizontalSum = FMath::Abs(LocalVelocity.X) + FMath::Abs(LocalVelocity.Y);
		if (HorizontalSum <= UE_KINDA_SMALL_NUMBER)
		{
			return Result;
		}

		Result.Forward = static_cast<float>(FMath::Max(LocalVelocity.X, 0.0) / HorizontalSum);
		Result.Backward = static_cast<float>(FMath::Max(-LocalVelocity.X, 0.0) / HorizontalSum);
		Result.Left = static_cast<float>(FMath::Max(-LocalVelocity.Y, 0.0) / HorizontalSum);
		Result.Right = static_cast<float>(FMath::Max(LocalVelocity.Y, 0.0) / HorizontalSum);
		return Result;
	}

	inline bool HasMovingSpeed(float Speed, bool bWasMoving, float EnterThreshold, float ExitThreshold)
	{
		const float SafeExitThreshold = FMath::Max(ExitThreshold, 0.f);
		const float SafeEnterThreshold = FMath::Max(EnterThreshold, SafeExitThreshold);
		return Speed > (bWasMoving ? SafeExitThreshold : SafeEnterThreshold);
	}

	inline void UpdateMovementTransitions(FWuwaLocomotionAnimData& Data, bool bWasMoving,
		float EnterThreshold, float ExitThreshold)
	{
		Data.bHasMovingSpeed = HasMovingSpeed(Data.GroundSpeed, bWasMoving, EnterThreshold, ExitThreshold);
		// 保留从静止开始的这一帧，即使 CMC 已经把速度加到进入阈值以上。
		Data.bIsGoingToMove = Data.bStateGround && Data.bHasMoveInput
			&& (!bWasMoving || !Data.bHasMovingSpeed);
		// 同理，强制动一帧内停稳也需要提供一次停步候选信号。
		Data.bWantsToStop = Data.bStateGround && !Data.bHasMoveInput
			&& (bWasMoving || Data.bHasMovingSpeed);
		if (Data.bHasUnifiedState)
		{
			Data.bIsGoingToMove = Data.bGroundMoveActive && (!bWasMoving || !Data.bHasMovingSpeed);
			Data.bWantsToStop = Data.bStateGround && !Data.bHasActionOverride
				&& (Data.MoveState == EWuwaMoveState::WalkStop || Data.MoveState == EWuwaMoveState::RunStop
					|| Data.MoveState == EWuwaMoveState::SprintStop);
		}
	}

	/** 原始采样 -> 动画快照。仅处理值，不访问角色或 Movement，也不回写移动规则。 */
	inline FWuwaLocomotionAnimData BuildAnimationData(const FWuwaAnimMoveData& Move,
		const FWuwaAnimStateData& State, bool bWasMoving, float EnterThreshold,
		float ExitThreshold, float InputThreshold)
	{
		FWuwaLocomotionAnimData Data;
		Data.bHasValidMovementData = true;
		Data.Velocity = Move.Velocity;
		Data.LocalVelocity = Move.ActorRotation.UnrotateVector(Move.Velocity);
		Data.LocalAccel = Move.ActorRotation.UnrotateVector(Move.Acceleration);
		Data.GroundSpeed = static_cast<float>(Move.Velocity.Size2D());
		Data.MaxSpeed = Move.MaxSpeed;
		Data.LocalMoveIntent = Move.ActorRotation.UnrotateVector(Move.InputVector);
		Data.bHasMoveInput = Data.LocalMoveIntent.SizeSquared()
			> FMath::Square(FMath::Max(InputThreshold, 0.f));
		Data.MovementMode = State.MovementMode;
		Data.CustomMovementMode = State.CustomMovementMode;
		Data.DesiredGait = State.DesiredGait;
		Data.AllowedGait = State.AllowedGait;
		Data.SprintDesire = State.SprintDesire;
		Data.bStateGround = State.bStateGround;
		Data.bStateAir = State.bStateAir;
		Data.bStateClimb = State.bStateClimb;
		Data.bIsCrouching = State.bIsCrouching;
		Data.bHasUnifiedState = State.bHasUnifiedState;
		Data.PositionState = State.PositionState;
		Data.MoveState = State.MoveState;
		Data.DirectionState = State.DirectionState;
		Data.bHasActionOverride = State.bHasActionOverride;
		Data.StopGait = State.StopGait;
		Data.bStateGroundWalk = State.bStateGround && State.AllowedGait == EWuwaGait::Walk;
		Data.bStateGroundRun = State.bStateGround && State.AllowedGait == EWuwaGait::Run;
		Data.bStateGroundSprint = State.bStateGround && State.AllowedGait == EWuwaGait::Sprint;
		Data.bGroundMoveActive = State.bStateGround && Data.bHasMoveInput;
		if (State.bHasUnifiedState)
		{
			Data.bStateGroundWalk = State.bStateGround && State.MoveState == EWuwaMoveState::Walk;
			Data.bStateGroundRun = State.bStateGround && State.MoveState == EWuwaMoveState::Run;
			Data.bStateGroundSprint = State.bStateGround && State.MoveState == EWuwaMoveState::Sprint;
			Data.bGroundMoveActive = !State.bHasActionOverride
				&& (Data.bStateGroundWalk || Data.bStateGroundRun || Data.bStateGroundSprint);
		}
		UpdateMovementTransitions(Data, bWasMoving, EnterThreshold, ExitThreshold);
		Data.VelocityBlend = CalculateVelocityBlend(Data.LocalVelocity);
		return Data;
	}
}
