// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/CharacterMovementComponent/WuwaMovementComponent.h"
#include "Input/WuwaInputCommand.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Tools/DebugHelper.h"

#pragma region Common

void UWuwaMovementComponent::SetDesiredGait(EWuwaGait NewGait)
{
	DesiredGait = NewGait;
}

void UWuwaMovementComponent::ToggleWalkRun()
{
	// 保留已有蓝图接口，但所有走跑切换都走同一个许可检查和执行入口。
	FWuwaInputCommand Command;
	Command.Type = EWuwaInputCommandType::SwitchWalk;
	ExecuteInputCommand(Command);
}

bool UWuwaMovementComponent::CanSwitchWalk() const
{
	return IsMovingOnGround() && !IsCrouching();
}

bool UWuwaMovementComponent::ExecuteInputCommand(const FWuwaInputCommand& Command)
{
	if (Command.Type != EWuwaInputCommandType::SwitchWalk || !CanSwitchWalk())
	{
		return false;
	}

	// 只改变移动策略；GetMaxSpeed 使用它限速，动画在更新时读取它。
	// 不强写 Velocity，不播放动画，也不存 Alt 是否按下。
	DesiredGait = DesiredGait == EWuwaGait::Walk ? EWuwaGait::Run : EWuwaGait::Walk;
	UE_LOG(LogTemp, Display, TEXT("[CommonMove] SwitchWalk -> %s (MaxSpeed=%.0f)"),
		DesiredGait == EWuwaGait::Walk ? TEXT("Walk") : TEXT("Run"), GetMaxSpeed());
	return true;
}

void UWuwaMovementComponent::SetSprintAllowed(bool bAllowed)
{
	bSprintAllowed = bAllowed;
}

EWuwaGait UWuwaMovementComponent::GetAllowedGait() const
{
	return DesiredGait == EWuwaGait::Sprint && !bSprintAllowed ? EWuwaGait::Run : DesiredGait;
}

void UWuwaMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (CanEnterClimbState())
	{
		Debug::Print(FString("Player Enter Climbing State"));
		SetMovementMode(MOVE_Custom, (uint8)ECustomMoveMode::MOVE_Climb);
	}
}



bool UWuwaMovementComponent::PlayerisInputing()
{
	return !Acceleration.IsNearlyZero();
}


#pragma endregion


#pragma region Climb Core

//When MovementState Changed This function will be called
void UWuwaMovementComponent::OnMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
	}

	if (PrevMode == MOVE_Custom && PrevCustomMode == ECustomMoveMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
	}
	Super::OnMovementModeChanged(PrevMode, PrevCustomMode);
}

float UWuwaMovementComponent::GetMaxSpeed() const
{
	if (IsClimbing())
	{
		return 100.f;
	}

	// 与引擎的地面/空中水平速度限制保持同一入口；不覆盖游泳、飞行和蹲伏。
	if ((IsMovingOnGround() || IsFalling()) && !IsCrouching())
	{
		switch (GetAllowedGait())
		{
		case EWuwaGait::Walk:
			return FMath::Min(FMath::Max(WalkSpeed, 0.f), Super::GetMaxSpeed());
		case EWuwaGait::Sprint:
			return FMath::Max(SprintSpeed, Super::GetMaxSpeed());
		case EWuwaGait::Run:
			return FMath::Max(RunSpeed, Super::GetMaxSpeed());
		default:
			break;
		}
	}

	return Super::GetMaxSpeed();
}

float UWuwaMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return 300.f;
	}
	return Super::GetMaxAcceleration();
}

bool UWuwaMovementComponent::IsClimbing() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == (uint8)ECustomMoveMode::MOVE_Climb);
}

bool UWuwaMovementComponent::CanEnterClimbState()
{
	if (IsClimbing()) return false;
	if (IsFalling()) return false;
	if (!ClimbableSurfacesTrace()) return false;
	if (!ClimbableEyeSiteTrace()) return false;
	if (!IsFacingClimbableSurface()) return false;
	if (!PlayerisInputing()) return false;
	return true;
}

bool UWuwaMovementComponent::ProcessClimbSurfaces()
{
	if (ClimbableSurfaceResults.IsEmpty())
	{
		ProcessedSurfaceResults = FVector::Zero();
		ProcessedSurfaceNomal = FVector::Zero();
		return false;
	}
	FVector CurClimbabllSurfacesAveLoc = FVector::Zero();
	FVector CurClimbableSurfacesAveNormal = FVector::Zero();
	for (const FHitResult& TraceHitResults : ClimbableSurfaceResults)
	{
		CurClimbabllSurfacesAveLoc += TraceHitResults.ImpactPoint;
		CurClimbableSurfacesAveNormal += TraceHitResults.ImpactNormal;
	}
	ProcessedSurfaceResults = CurClimbabllSurfacesAveLoc/ClimbableSurfaceResults.Num();
	ProcessedSurfaceNomal = (CurClimbableSurfacesAveNormal / ClimbableSurfaceResults.Num()).GetSafeNormal();
	return true;
}

bool UWuwaMovementComponent::IsFacingClimbableSurface()
{
	// ---- 1. 筛选 + 平均 ----
	FVector NormalSum = FVector::ZeroVector;
	FVector PointSum = FVector::ZeroVector;
	int32   ValidCount = 0;

	// ImpactNormal.Z = cos(表面倾角)
	//   倾角 0°(地面)  → +1
	//   倾角 90°(垂直墙)→  0
	//   倾角 >90°(外倾) →  负
	const float MaxSlopeNormalZ = FMath::Cos(FMath::DegreesToRadians(MinClimbSurfaceAngle));   // 太缓 → 当地面
	const float MaxOverhangNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxClimbSurfaceAngle));   // 太外倾 → 爬不上

	for (const FHitResult& Hit : ClimbableSurfaceResults)
	{
		if (!Hit.bBlockingHit) continue;

		const FVector& N = Hit.ImpactNormal;
		if (N.Z > MaxSlopeNormalZ)    continue;
		if (N.Z < MaxOverhangNormalZ) continue;   // 注意 MaxOverhangNormalZ 本身是负数

		NormalSum += N;
		PointSum += Hit.ImpactPoint;
		++ValidCount;
	}

	if (ValidCount == 0)
	{
		ProcessedSurfaceNomal = FVector::ZeroVector;
		ProcessedSurfaceResults = FVector::ZeroVector;
		return false;
	}

	const FVector AvgNormal = (NormalSum / ValidCount).GetSafeNormal();
	const FVector AvgLocation = PointSum / ValidCount;

	// ---- 2. 朝向锥(只看水平分量)----
	const FVector FlatNormal = AvgNormal.GetSafeNormal2D();
	const FVector FlatForward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	if (FlatNormal.IsNearlyZero() || FlatForward.IsNearlyZero()) return false;

	const float Facing = FVector::DotProduct(-FlatNormal, FlatForward);
	if (Facing < FMath::Cos(FMath::DegreesToRadians(MaxClimbEntryAngle))) return false;

	//// ---- 3. 一致性校验:眼高看到的和胸口撞到的是同一面墙吗 ----
	//if (ClimbableEyesiteResult.bBlockingHit)
	//{
	//	const FVector EyeFlatNormal = ClimbableEyesiteResult.ImpactNormal.GetSafeNormal2D();
	//	if (!EyeFlatNormal.IsNearlyZero())
	//	{
	//		const float Coherence = FVector::DotProduct(EyeFlatNormal, FlatNormal);
	//		if (Coherence < FMath::Cos(FMath::DegreesToRadians(MaxSurfaceDivergenceAngle)))
	//			return false;
	//	}
	//}

	// ---- 4. 缓存给 PhysClimbing 用 ----
	ProcessedSurfaceNomal = AvgNormal;
	ProcessedSurfaceResults = AvgLocation;
	return true;
}

FQuat UWuwaMovementComponent::GetClimbRotation(float Deltatime)
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return CurrentRotation;
	}
	const FQuat TargetQuat = FRotationMatrix::MakeFromX(-ProcessedSurfaceNomal).ToQuat();
	return FMath::QInterpTo(CurrentRotation, TargetQuat, Deltatime, 5.f);
}

void UWuwaMovementComponent::SnapMovementToClimbableSurface(float Deltatime)
{
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ProjectedCharacterToSurface =
		(ProcessedSurfaceResults - ComponentLocation).ProjectOnTo(ComponentForward);
	const FVector SnapVector = -ProcessedSurfaceNomal * ProjectedCharacterToSurface.Length();
	UpdatedComponent->MoveComponent(
		SnapVector * Deltatime * 500,
		UpdatedComponent->GetComponentQuat(),
		true);
}

bool UWuwaMovementComponent::CheckShouldStopClimbing()
{
	if (ClimbableSurfaceResults.IsEmpty()) return true;
	const float DotResult = FVector::DotProduct(ProcessedSurfaceNomal, FVector::UpVector);
	const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));
	return DegreeDiff < 60.f;
}

void UWuwaMovementComponent::StopClimbing()
{
	SetMovementMode(MOVE_Falling);
}


void UWuwaMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimbing(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

void UWuwaMovementComponent::PhysClimbing(float deltaTime, int32 Iterations)
{
	//Only Happend On some extreme cases
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//Calculate hit surfaces and average normals
	ClimbableSurfacesTrace();
	ProcessClimbSurfaces();


	if (CheckShouldStopClimbing())
	{
		StopClimbing();
		return;
	}

	//Incase Rootmotion's Velocity impact real Velocity
	RestorePreAdditiveRootMotionVelocity();
	//计算除了强加的比如MoveTo之外和动画的RootMotion之外本身的移动速度
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		//calculate true Speed
		CalcVelocity(deltaTime, 0.1, true, MaxClimbBrakingDeceleration);
	}
	//return Rootmotion Speed
	ApplyRootMotionToVelocity(deltaTime);


	//origional position and expected Adjusted Move
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	//Calculate Move safely, when hit is not equal to 1 , it means that character hit something
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);//

	//cause cha encountered some backforce so we need to fix the move
	if (Hit.Time < 1.f)
	{
		//adjust and try again
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	//calculate final Velocity On this Frame
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	//Making Cha always face to the center of walls
	SnapMovementToClimbableSurface(deltaTime);
}

#pragma endregion


#pragma region Climb Traces
bool UWuwaMovementComponent::ClimbableSurfacesTrace()
{
	FVector Startoffset = UpdatedComponent->GetForwardVector()* 20.f;
	FVector StartPostion = UpdatedComponent->GetComponentLocation()+ Startoffset;
	FVector EndPosition = StartPostion + UpdatedComponent->GetForwardVector();
	ClimbableSurfaceResults = DoCapsuleMultipleforObjectTrace(StartPostion, EndPosition, false);
	return !ClimbableSurfaceResults.IsEmpty();
}
bool UWuwaMovementComponent::ClimbableEyeSiteTrace()
{
	FVector CharacterForward = UpdatedComponent->GetForwardVector();
	FVector StartPostion = UpdatedComponent->GetComponentLocation()+ UpdatedComponent->GetUpVector()*50.0f;
	FVector EndPosition = StartPostion + ClimbTraceDistance* CharacterForward;
	ClimbableEyesiteResult = DoEyeSideTrace(StartPostion, EndPosition, true);
	AActor* HitActor = ClimbableEyesiteResult.GetActor();
	return HitActor ? true : false;
}
TArray<FHitResult> UWuwaMovementComponent::DoCapsuleMultipleforObjectTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide)
{

	TArray<FHitResult> HitResults;
	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		StartPos,
		EndPos,
		ClimbCapsuleTraceRadius,
		ClimbCapsuleTraceHeight,
		ObjectTypes,
		false,
		ActorsToIgnore,
		bDrawTraceOutSide?EDrawDebugTrace::ForOneFrame: EDrawDebugTrace::None,
		HitResults,
		true
	);
	return HitResults;
}


FHitResult UWuwaMovementComponent::DoEyeSideTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide)
{
	FHitResult HitResult;
	UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		StartPos,
		EndPos,
		ObjectTypes,
		false,
		ActorsToIgnore,
		bDrawTraceOutSide ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		HitResult,
		true
	);
	return HitResult;
}

#pragma endregion


