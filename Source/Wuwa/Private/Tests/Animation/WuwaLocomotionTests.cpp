#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionMath.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaVelocityBlendTest, "Wuwa.Locomotion.VelocityBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaVelocityBlendTest::RunTest(const FString& Parameters)
{
	const FWuwaVelocityBlend Still = WuwaLocomotion::CalculateVelocityBlend(FVector::ZeroVector);
	TestEqual(TEXT("Stationary weights are zero"), Still.Forward + Still.Backward + Still.Left + Still.Right, 0.f);
	const FWuwaVelocityBlend Forward = WuwaLocomotion::CalculateVelocityBlend(FVector(100, 0, 0));
	TestEqual(TEXT("Actor X is forward"), Forward.Forward, 1.f);
	const FWuwaVelocityBlend Diagonal = WuwaLocomotion::CalculateVelocityBlend(FVector(100, -100, 500));
	TestEqual(TEXT("Diagonal forward weight"), Diagonal.Forward, 0.5f);
	TestEqual(TEXT("Negative actor Y is left"), Diagonal.Left, 0.5f);
	TestEqual(TEXT("Horizontal weights sum to one"), Diagonal.Forward + Diagonal.Backward + Diagonal.Left + Diagonal.Right, 1.f);
	const FWuwaVelocityBlend BackRight = WuwaLocomotion::CalculateVelocityBlend(FVector(-100, 100, 0));
	TestEqual(TEXT("Negative actor X is backward"), BackRight.Backward, 0.5f);
	TestEqual(TEXT("Positive actor Y is right"), BackRight.Right, 0.5f);
	const FWuwaVelocityBlend Vertical = WuwaLocomotion::CalculateVelocityBlend(FVector(0, 0, 500));
	TestEqual(TEXT("Vertical speed does not cause running blend"), Vertical.Forward + Vertical.Backward + Vertical.Left + Vertical.Right, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMovingThresholdTest, "Wuwa.Locomotion.MovingThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaMovingThresholdTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Low speed does not start movement"), WuwaLocomotion::HasMovingSpeed(3.f, false, 5.f, 2.f));
	TestTrue(TEXT("Same speed preserves existing movement"), WuwaLocomotion::HasMovingSpeed(3.f, true, 5.f, 2.f));
	TestTrue(TEXT("Enter above threshold"), WuwaLocomotion::HasMovingSpeed(6.f, false, 5.f, 2.f));
	TestFalse(TEXT("Exit at stopping threshold"), WuwaLocomotion::HasMovingSpeed(2.f, true, 5.f, 2.f));
	TestFalse(TEXT("Stationary never moves"), WuwaLocomotion::HasMovingSpeed(0.f, true, 5.f, 2.f));
	TestFalse(TEXT("Invalid threshold ordering is clamped"), WuwaLocomotion::HasMovingSpeed(3.f, false, 1.f, 4.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaGaitPolicyTest, "Wuwa.Locomotion.GaitPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaGaitPolicyTest::RunTest(const FString& Parameters)
{
	// UE 的地面判断也依赖 UpdatedComponent；使用真实角色组件，不用游离 Movement mock。
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Gait policy test world was created"), TestWorld))
	{
		return false;
	}
	ON_SCOPE_EXIT { TestWorld->DestroyWorld(false); };
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AWuwaCharacter* Character = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
	if (!TestNotNull(TEXT("Gait policy uses a character-owned movement component"), Movement))
	{
		return false;
	}
	// 不调用 BeginPlay、Possess 或物理 Tick，只指定当前运动模式。
	Movement->MovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 650.f;
	TestTrue(TEXT("Default gait remains Run"), Movement->GetAllowedGait() == EWuwaGait::Run);
	TestEqual(TEXT("Run preserves existing MaxWalkSpeed"), Movement->GetMaxSpeed(), 650.f);
	Movement->ToggleWalkRun();
	TestTrue(TEXT("Walk toggle changes policy"), Movement->GetAllowedGait() == EWuwaGait::Walk);
	TestTrue(TEXT("Walk speed is no faster than Run"), Movement->GetMaxSpeed() <= 650.f);
	Movement->ToggleWalkRun();
	TestTrue(TEXT("Toggle returns to Run"), Movement->GetAllowedGait() == EWuwaGait::Run);
	Movement->SetDesiredGait(EWuwaGait::Sprint);
	TestTrue(TEXT("Sprint request is retained"), Movement->GetDesiredGait() == EWuwaGait::Sprint);
	TestTrue(TEXT("Sprint requires permission"), Movement->GetAllowedGait() == EWuwaGait::Run);
	Movement->SetSprintAllowed(true);
	TestTrue(TEXT("Permission enables Sprint"), Movement->GetAllowedGait() == EWuwaGait::Sprint);
	Movement->SetSprintAllowed(false);
	TestEqual(TEXT("Revoking permission restores Run speed"), Movement->GetMaxSpeed(), 650.f);
	Movement->MovementMode = MOVE_Custom;
	Movement->CustomMovementMode = ECustomMoveMode::MOVE_Climb;
	TestEqual(TEXT("Climbing retains existing speed"), Movement->GetMaxSpeed(), 100.f);
	Movement->MovementMode = MOVE_None;
	TestEqual(TEXT("Disabled movement stays disabled"), Movement->GetMaxSpeed(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaLocomotionTransitionTest, "Wuwa.Locomotion.TransitionSignals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaLocomotionTransitionTest::RunTest(const FString& Parameters)
{
	FWuwaLocomotionAnimData Data;
	Data.bStateGround = true;
	Data.bHasMoveInput = true;
	Data.GroundSpeed = 35.f;
	WuwaLocomotion::UpdateMovementTransitions(Data, false, 5.f, 2.f);
	TestTrue(TEXT("First moving frame retains start signal even above speed threshold"), Data.bIsGoingToMove);
	TestFalse(TEXT("Starting is not stopping"), Data.bWantsToStop);
	WuwaLocomotion::UpdateMovementTransitions(Data, true, 5.f, 2.f);
	TestFalse(TEXT("Steady movement does not restart every frame"), Data.bIsGoingToMove);
	Data.bHasMoveInput = false;
	Data.GroundSpeed = 0.f;
	WuwaLocomotion::UpdateMovementTransitions(Data, true, 5.f, 2.f);
	TestTrue(TEXT("A one-frame stop still provides a stop signal"), Data.bWantsToStop);
	WuwaLocomotion::UpdateMovementTransitions(Data, false, 5.f, 2.f);
	TestFalse(TEXT("Idle does not repeat the stop signal"), Data.bWantsToStop);
	Data.bStateGround = false;
	Data.bHasMoveInput = true;
	WuwaLocomotion::UpdateMovementTransitions(Data, false, 5.f, 2.f);
	TestFalse(TEXT("Air movement does not request a ground start"), Data.bIsGoingToMove);
	Data.bHasMoveInput = false;
	Data.GroundSpeed = 500.f;
	WuwaLocomotion::UpdateMovementTransitions(Data, true, 5.f, 2.f);
	TestFalse(TEXT("Air momentum does not request a ground stop"), Data.bWantsToStop);
	return true;
}

#endif
