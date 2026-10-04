// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/Utilities/DebugHelper.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"

#pragma region Common

void UWuwaMovementComponent::BindMovementState(UWuwaUnifiedStateBridgeComponent* InUnifiedState, UWuwaRoleGaitBridgeComponent* InRoleGait)
{
    UnifiedState = InUnifiedState;
    RoleGait = InRoleGait;
}

UWuwaRoleGaitBridgeComponent* UWuwaMovementComponent::ResolveGaitComponent() const
{
    return IsValid(RoleGait) ? RoleGait.Get() : nullptr;
}

UWuwaUnifiedStateBridgeComponent* UWuwaMovementComponent::ResolveUnifiedState() const
{
    // 与 RoleGait 同时装配；步态不再驱动（角色结束）后，这里也不再修改运动状态。
    return ResolveGaitComponent() && IsValid(UnifiedState) ? UnifiedState.Get() : nullptr;
}

bool UWuwaMovementComponent::CanToggleWalkPreference() const
{
    const auto* State = ResolveUnifiedState();
    return State && State->CanDriveState() && !IsCrouching()
        && State->GetStateData().PositionState == EWuwaPositionState::Ground;
}

EWuwaGait UWuwaMovementComponent::GetDesiredGait() const
{
    const auto* State = ResolveUnifiedState();
    return State ? (State->IsWalkPreferred() ? EWuwaGait::Walk : EWuwaGait::Run) : DesiredGait;
}

EWuwaGait UWuwaMovementComponent::GetAllowedGait() const
{
    return GetUnifiedStateData().Gait;
}

void UWuwaMovementComponent::BindInputIntent(UWuwaInputIntentComponent* InInputIntent)
{
    InputIntent = InInputIntent;
}

void UWuwaMovementComponent::ApplyMoveIntent()
{
    if (!IsValid(InputIntent) || !PawnOwner) return;
    const FWuwaMoveIntent Move = InputIntent->GetMoveIntent();
    if (IsClimbing())
    {
        // 攀爬：前后沿墙面上下，左右沿墙面横向，保留轴的幅度。
        const FVector ForwardDirection = FVector::CrossProduct(-ProcessedSurfaceNomal, PawnOwner->GetActorRightVector());
        const FVector RightDirection = FVector::CrossProduct(-ProcessedSurfaceNomal, -PawnOwner->GetActorUpVector());
        AddInputVector(ForwardDirection * Move.Axis.Y);
        AddInputVector(RightDirection * Move.Axis.X);
    }
    else if (Move.bHasInput)
    {
        AddInputVector(Move.WorldDirection);
    }
}

EWuwaPositionState UWuwaMovementComponent::ReadPositionState() const
{
    if (IsClimbing()) return EWuwaPositionState::Climb;
    if (IsMovingOnGround()) return EWuwaPositionState::Ground;
    if (IsFalling() || MovementMode == MOVE_Flying) return EWuwaPositionState::Air;
    if (IsSwimming()) return EWuwaPositionState::Water;
    return EWuwaPositionState::None;
}

FWuwaUnifiedStateData UWuwaMovementComponent::GetUnifiedStateData() const
{
    return IsValid(UnifiedState) ? UnifiedState->GetStateData() : FWuwaUnifiedStateData{};
}

void UWuwaMovementComponent::HandleMoveStateChanged(EWuwaMoveState, EWuwaMoveState)
{
    RefreshMovementSettings();
}

void UWuwaMovementComponent::HandleGaitChanged(EWuwaGait, EWuwaGait)
{
    RefreshMovementSettings();
}

void UWuwaMovementComponent::RefreshMovementSettings()
{
    // 只按已接受状态选配置。这里没有输入判断、冲刺计时，也不修改 Velocity。
    if (!bCapturedDefaultMovementSettings)
    {
        DefaultMovementSettings.MaxSpeed = MaxWalkSpeed;
        DefaultMovementSettings.MaxAcceleration = MaxAcceleration;
        DefaultMovementSettings.GroundFriction = GroundFriction;
        DefaultMovementSettings.BrakingDeceleration = BrakingDecelerationWalking;
        bCapturedDefaultMovementSettings = true;
    }
    const EWuwaGait Gait = GetUnifiedStateData().Gait;
    FWuwaGaitMovementSettings Settings = DefaultMovementSettings;
    // 兼容原 GetMaxSpeed 的速度上限语义；MaxWalkSpeed 现在是执行输出，不能再用它作配置输入。
    Settings.MaxSpeed = Gait == EWuwaGait::Walk ? FMath::Min(WalkSpeed, DefaultMovementSettings.MaxSpeed)
        : FMath::Max(Gait == EWuwaGait::Sprint ? SprintSpeed : RunSpeed, DefaultMovementSettings.MaxSpeed);
    if (MovementSettings) Settings = MovementSettings->ForGait(Gait);
    MaxWalkSpeed = FMath::Max(0.f, Settings.MaxSpeed);
    MaxAcceleration = FMath::Max(0.f, Settings.MaxAcceleration);
    GroundFriction = FMath::Max(0.f, Settings.GroundFriction);
    BrakingDecelerationWalking = FMath::Max(0.f, Settings.BrakingDeceleration);
}

void UWuwaMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // 原作 RoleGait 在自己的 OnTick 中决策。UE 默认让 CMC 先于 Character Tick，
    // 这里固定在本帧物理前调度脚本，避免形成 Tick 依赖环。
    if (auto* Gait = ResolveGaitComponent()) Gait->RefreshPolicy();
    // 输入在本帧物理前写入，Super 中立刻消费，不依赖输入事件和角色 Tick 的先后。
    ApplyMoveIntent();
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
    // 原作运动状态组件监听 CharMovementModeChanged 同步位置；随后本帧重算一次步态，不等下一次 Tick。
    if (auto* Gait = ResolveGaitComponent())
    {
        if (auto* State = ResolveUnifiedState()) State->HandleMovementModeChanged(ReadPositionState(), MovementMode.GetValue());
        Gait->RefreshPolicy();
    }
}

float UWuwaMovementComponent::GetMaxSpeed() const
{
    if (IsClimbing()) return 100.f;
    // 状态事件已把地面配置写入 MaxWalkSpeed；物理查询不再跨语言做步态决策。
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


