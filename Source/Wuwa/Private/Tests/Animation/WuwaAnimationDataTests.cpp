#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionMath.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommand.h"

namespace WuwaAnimationDataTests
{
	// 使用真实 Character/Movement 所有权关系；不运行 BeginPlay、Possess 或物理 Tick。
	struct FCharacterFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaMovementComponent* Movement = nullptr;

		FCharacterFixture()
		{
			Character = SpawnCharacter();
			Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
			if (Movement)
			{
				Movement->MovementMode = MOVE_Walking;
				Movement->MaxWalkSpeed = 650.f;
			}
		}

		~FCharacterFixture()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		AWuwaCharacter* SpawnCharacter() const
		{
			if (!World)
			{
				return nullptr;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParameters.ObjectFlags |= RF_Transient;
			return World->SpawnActor<AWuwaCharacter>(SpawnParameters);
		}

		bool IsReady(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Transient test world exists"), World)
				&& Test.TestNotNull(TEXT("Character exists"), Character)
				&& Test.TestNotNull(TEXT("Character-owned Movement exists"), Movement)
				&& Test.TestNotNull(TEXT("Movement has an UpdatedComponent"), Movement->UpdatedComponent.Get());
		}
	};

	FWuwaLocomotionAnimData BuildSnapshot(const UWuwaAnimLogicParams& Params, bool bWasMoving = false)
	{
		return WuwaLocomotion::BuildAnimationData(Params.GetMoveData(), Params.GetStateData(),
			bWasMoving, 5.f, 2.f, 0.01f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationDataCaptureTest, "Wuwa.Animation.DataCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationDataCaptureTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	Fixture.Character->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	Fixture.Movement->Velocity = FVector(0.f, 120.f, 25.f);
	Fixture.Movement->SetDesiredGait(EWuwaGait::Walk);
	UWuwaAnimLogicParams* Params = NewObject<UWuwaAnimLogicParams>(Fixture.Character);
	TestTrue(TEXT("Movement can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestTrue(TEXT("Capture is marked valid"), Params->HasValidData());
	TestTrue(TEXT("Rotation is Actor rotation, not Mesh rotation"),
		Params->GetMoveData().ActorRotation.Equals(Fixture.Character->GetActorQuat()));
	TestEqual(TEXT("Velocity is copied without modification"), Params->GetMoveData().Velocity, Fixture.Movement->Velocity);
	TestEqual(TEXT("Acceleration is copied from Movement"), Params->GetMoveData().Acceleration, Fixture.Movement->GetCurrentAcceleration());
	TestEqual(TEXT("Max speed uses the active Movement policy"), Params->GetMoveData().MaxSpeed, Fixture.Movement->GetMaxSpeed());
	TestTrue(TEXT("Desired gait is copied"), Params->GetStateData().DesiredGait == EWuwaGait::Walk);
	TestTrue(TEXT("Allowed gait is copied"), Params->GetStateData().AllowedGait == EWuwaGait::Walk);
	TestTrue(TEXT("Walking movement is grounded"), Params->GetStateData().bStateGround);
	TestFalse(TEXT("Walking movement is not falling"), Params->GetStateData().bStateAir);
	TestFalse(TEXT("Standing character is not crouched"), Params->GetStateData().bIsCrouching);

	const FWuwaLocomotionAnimData Snapshot = BuildSnapshot(*Params);
	TestTrue(TEXT("Snapshot marks data valid"), Snapshot.bHasValidMovementData);
	TestTrue(TEXT("World velocity becomes Actor-local forward velocity"), Snapshot.LocalVelocity.Equals(FVector(120.f, 0.f, 25.f), 0.001));
	TestEqual(TEXT("Ground speed excludes vertical velocity"), Snapshot.GroundSpeed, 120.f);
	TestEqual(TEXT("Snapshot exposes the effective Movement max speed"), Snapshot.MaxSpeed, Fixture.Movement->GetMaxSpeed());
	TestTrue(TEXT("Walk gait produces the ground-walk animation flag"), Snapshot.bStateGroundWalk);
	TestTrue(TEXT("Snapshot construction never writes Movement velocity"), Fixture.Movement->Velocity.Equals(FVector(0.f, 120.f, 25.f)));

	Fixture.Movement->MovementMode = MOVE_Falling;
	TestTrue(TEXT("Falling can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestTrue(TEXT("Falling state is reflected"), Params->GetStateData().bStateAir);
	TestFalse(TEXT("Falling clears grounded state"), Params->GetStateData().bStateGround);
	Fixture.Movement->MovementMode = MOVE_Custom;
	Fixture.Movement->CustomMovementMode = ECustomMoveMode::MOVE_Climb;
	TestTrue(TEXT("Custom movement can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestTrue(TEXT("Climbing state is reflected"), Params->GetStateData().bStateClimb);
	TestTrue(TEXT("Movement mode is copied"), Params->GetStateData().MovementMode == MOVE_Custom);
	TestEqual(TEXT("Custom mode is copied"), Params->GetStateData().CustomMovementMode, static_cast<uint8>(ECustomMoveMode::MOVE_Climb));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationInputIntentTest, "Wuwa.Animation.InputIntentAndMomentum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationInputIntentTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	Fixture.Character->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	// 只消费输入，不计算控制加速度和速度，证明输入不从 Acceleration 反推。
	Fixture.Character->AddMovementInput(FVector::RightVector, 0.5f, true);
	Fixture.Movement->ConsumeInputVector();
	UWuwaAnimLogicParams* Params = NewObject<UWuwaAnimLogicParams>(Fixture.Character);
	TestTrue(TEXT("Capture after input consumption succeeds"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestEqual(TEXT("Input vector is last consumed world-space input"), Params->GetMoveData().InputVector, FVector(0.f, 0.5f, 0.f));
	TestTrue(TEXT("Acceleration remains zero without a movement tick"), Params->GetMoveData().Acceleration.IsNearlyZero());
	TestTrue(TEXT("Velocity remains zero without a movement tick"), Params->GetMoveData().Velocity.IsNearlyZero());
	const FWuwaLocomotionAnimData Starting = BuildSnapshot(*Params);
	TestTrue(TEXT("Input exists even with zero acceleration and velocity"), Starting.bHasMoveInput);
	TestTrue(TEXT("Local intent preserves analog strength and Actor basis"), Starting.LocalMoveIntent.Equals(FVector(0.5f, 0.f, 0.f), 0.001));
	TestTrue(TEXT("Stationary ground input offers a start signal"), Starting.bIsGoingToMove);
	TestFalse(TEXT("Input does not invent moving speed"), Starting.bHasMovingSpeed);

	// 再消费一次空输入代表松开；实际速度可以继续存在。
	Fixture.Movement->ConsumeInputVector();
	Fixture.Movement->Velocity = FVector(0.f, 180.f, 0.f);
	TestTrue(TEXT("Momentum can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	const FWuwaLocomotionAnimData Stopping = BuildSnapshot(*Params, true);
	TestFalse(TEXT("Momentum is not treated as input"), Stopping.bHasMoveInput);
	TestTrue(TEXT("No-input momentum remains moving"), Stopping.bHasMovingSpeed);
	TestTrue(TEXT("No-input ground momentum offers a stop signal"), Stopping.bWantsToStop);
	TestFalse(TEXT("No-input momentum does not offer a start signal"), Stopping.bIsGoingToMove);
	TestTrue(TEXT("Earlier snapshot is a value copy, not mutable live data"), Starting.bHasMoveInput && Starting.GroundSpeed == 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationGaitCommandTest, "Wuwa.Animation.GaitCommandBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationGaitCommandTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	Fixture.Movement->Velocity = FVector(300.f, 0.f, 0.f);
	UWuwaAnimLogicParams* Params = NewObject<UWuwaAnimLogicParams>(Fixture.Character);
	TestTrue(TEXT("Initial run state can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	const FWuwaLocomotionAnimData Before = BuildSnapshot(*Params);
	TestEqual(TEXT("Run snapshot exposes the configured run speed limit"), Before.MaxSpeed, 650.f);
	TestEqual(TEXT("Run snapshot reports actual speed separately"), Before.GroundSpeed, 300.f);
	FWuwaInputCommand Command;
	Command.Type = EWuwaInputCommandType::SwitchWalk;
	TestTrue(TEXT("Grounded SwitchWalk is accepted"), Fixture.Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Accepted state can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	const FWuwaLocomotionAnimData After = BuildSnapshot(*Params, true);
	TestTrue(TEXT("Animation reads the accepted Walk state"), After.DesiredGait == EWuwaGait::Walk && After.AllowedGait == EWuwaGait::Walk);
	TestTrue(TEXT("Animation state flag reflects the accepted gait"), After.bStateGroundWalk);
	TestEqual(TEXT("Gait switch does not rewrite actual velocity"), Fixture.Movement->Velocity, FVector(300.f, 0.f, 0.f));
	TestEqual(TEXT("Animation retains actual speed during deceleration"), After.GroundSpeed, 300.f);
	TestEqual(TEXT("Walk snapshot reads the effective CMC speed limit"), After.MaxSpeed, Fixture.Movement->GetMaxSpeed());
	TestTrue(TEXT("SwitchWalk lowers the effective movement speed limit"), After.MaxSpeed < Before.MaxSpeed);
	TestEqual(TEXT("Walk does not overwrite the configured run speed"), Fixture.Movement->MaxWalkSpeed, 650.f);
	TestEqual(TEXT("Changing the speed limit does not pretend actual speed changed"), After.GroundSpeed, Before.GroundSpeed);
	TestTrue(TEXT("Prior snapshot retains Run"), Before.DesiredGait == EWuwaGait::Run);
	TestTrue(TEXT("New max speed reflects Walk policy"), Params->GetMoveData().MaxSpeed <= 650.f);

	Fixture.Movement->MovementMode = MOVE_Falling;
	TestFalse(TEXT("Air SwitchWalk is rejected"), Fixture.Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Rejected command still allows state capture"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestTrue(TEXT("Rejected command does not change accepted gait"), Params->GetStateData().DesiredGait == EWuwaGait::Walk);
	TestFalse(TEXT("Air snapshot does not show grounded walk"), BuildSnapshot(*Params).bStateGroundWalk);
	Fixture.Movement->MovementMode = MOVE_Walking;
	TestTrue(TEXT("A second ground SwitchWalk returns to Run"), Fixture.Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Restored run state can be captured"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	const FWuwaLocomotionAnimData RestoredRun = BuildSnapshot(*Params, true);
	TestEqual(TEXT("Returning to Run restores the original effective speed limit"), RestoredRun.MaxSpeed, Before.MaxSpeed);
	TestEqual(TEXT("Returning to Run still does not overwrite actual speed"), RestoredRun.GroundSpeed, Before.GroundSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationInvalidDataTest, "Wuwa.Animation.InvalidSourceReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationInvalidDataTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	Fixture.Movement->Velocity = FVector(100.f, 0.f, 0.f);
	UWuwaAnimLogicParams* Params = NewObject<UWuwaAnimLogicParams>(Fixture.Character);
	TestTrue(TEXT("Capture starts valid"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	TestFalse(TEXT("Null source is rejected"), UWuwaAnimDataLibrary::UpdateAnimationData(nullptr, Params));
	TestFalse(TEXT("Null source clears validity"), Params->HasValidData());
	TestTrue(TEXT("Null source clears previous velocity"), Params->GetMoveData().Velocity.IsNearlyZero());
	TestEqual(TEXT("Null source clears the previous effective speed limit"), Params->GetMoveData().MaxSpeed, 0.f);
	TestFalse(TEXT("Null source clears previous grounded state"), Params->GetStateData().bStateGround);
	TestTrue(TEXT("A valid source can recover"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	UWuwaMovementComponent* OrphanMovement = NewObject<UWuwaMovementComponent>(GetTransientPackage());
	TestFalse(TEXT("Unowned Movement is rejected"), UWuwaAnimDataLibrary::UpdateAnimationData(OrphanMovement, Params));
	TestFalse(TEXT("Unowned Movement does not preserve stale validity"), Params->HasValidData());
	TestTrue(TEXT("Unowned Movement clears previous velocity"), Params->GetMoveData().Velocity.IsNearlyZero());
	TestFalse(TEXT("Null output object is rejected without a crash"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, nullptr));
	TestTrue(TEXT("Capture can recover again"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, Params));
	Params->Reset();
	TestFalse(TEXT("Explicit reset clears validity"), Params->HasValidData());
	TestTrue(TEXT("Explicit reset clears movement"), Params->GetMoveData().Velocity.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationPerInstanceTest, "Wuwa.Animation.PerInstanceIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationPerInstanceTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	AWuwaCharacter* OtherCharacter = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("Second character exists"), OtherCharacter))
	{
		return false;
	}
	UWuwaMovementComponent* OtherMovement = OtherCharacter->GetWuwaMovementComponent();
	if (!TestNotNull(TEXT("Second Movement exists"), OtherMovement))
	{
		return false;
	}
	Fixture.Movement->Velocity = FVector(100.f, 0.f, 0.f);
	Fixture.Movement->SetDesiredGait(EWuwaGait::Walk);
	OtherMovement->MovementMode = MOVE_Falling;
	OtherMovement->Velocity = FVector(0.f, 250.f, -100.f);
	UWuwaAnimLogicParams* FirstParams = NewObject<UWuwaAnimLogicParams>(Fixture.Character);
	UWuwaAnimLogicParams* SecondParams = NewObject<UWuwaAnimLogicParams>(OtherCharacter);
	TestTrue(TEXT("First instance captures"), UWuwaAnimDataLibrary::UpdateAnimationData(Fixture.Movement, FirstParams));
	TestTrue(TEXT("Second instance captures"), UWuwaAnimDataLibrary::UpdateAnimationData(OtherMovement, SecondParams));
	TestEqual(TEXT("First velocity is independent"), FirstParams->GetMoveData().Velocity, FVector(100.f, 0.f, 0.f));
	TestEqual(TEXT("Second velocity is independent"), SecondParams->GetMoveData().Velocity, FVector(0.f, 250.f, -100.f));
	TestTrue(TEXT("First ground state is independent"), FirstParams->GetStateData().bStateGround);
	TestTrue(TEXT("Second air state is independent"), SecondParams->GetStateData().bStateAir);
	FirstParams->Reset();
	TestTrue(TEXT("Resetting first object leaves second valid"), SecondParams->HasValidData());
	TestEqual(TEXT("Resetting first object leaves second velocity intact"), SecondParams->GetMoveData().Velocity, OtherMovement->Velocity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationInstanceLifecycleTest, "Wuwa.Animation.AnimInstanceLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationInstanceLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaAnimationDataTests;
	FCharacterFixture Fixture;
	if (!Fixture.IsReady(*this))
	{
		return false;
	}

	// 原生更新不要求已加载动画图，但 Outer 必须为真实 SkeletalMeshComponent。
	UWuwaAnimInstance* Anim = NewObject<UWuwaAnimInstance>(Fixture.Character->GetMesh());
	Anim->NativeInitializeAnimation();
	TestFalse(TEXT("Initialization starts with an invalid snapshot"), Anim->LocomotionData.bHasValidMovementData);
	Fixture.Movement->Velocity = FVector(120.f, 0.f, 0.f);
	Anim->NativeUpdateAnimation(1.f / 60.f);
	if (!TestNotNull(TEXT("Native update owns its parameter object"), Anim->AnimLogicParams.Get()))
	{
		return false;
	}
	TestTrue(TEXT("Native update captures valid data"), Anim->AnimLogicParams->HasValidData());
	TestTrue(TEXT("Native update publishes a valid snapshot"), Anim->LocomotionData.bHasValidMovementData);
	TestEqual(TEXT("Native update publishes Movement speed"), Anim->LocomotionData.GroundSpeed, 120.f);
	TestEqual(TEXT("Native update publishes Movement effective max speed"), Anim->LocomotionData.MaxSpeed, Fixture.Movement->GetMaxSpeed());

	UWuwaAnimInstance* OtherAnim = NewObject<UWuwaAnimInstance>(Fixture.Character->GetMesh());
	OtherAnim->NativeInitializeAnimation();
	OtherAnim->NativeUpdateAnimation(1.f / 60.f);
	TestNotNull(TEXT("Second AnimInstance owns parameters"), OtherAnim->AnimLogicParams.Get());
	TestTrue(TEXT("AnimInstances never share a parameter object"), Anim->AnimLogicParams != OtherAnim->AnimLogicParams);

	Anim->NativeInitializeAnimation();
	TestFalse(TEXT("Reinitialization discards the old snapshot"), Anim->LocomotionData.bHasValidMovementData);
	TestTrue(TEXT("Reinitialization discards prior parameter validity"), !Anim->AnimLogicParams || !Anim->AnimLogicParams->HasValidData());
	Fixture.Movement->Velocity = FVector::ZeroVector;
	Fixture.Movement->ConsumeInputVector();
	Anim->NativeUpdateAnimation(1.f / 60.f);
	TestTrue(TEXT("Update after reinitialization captures valid data"), Anim->LocomotionData.bHasValidMovementData);
	TestFalse(TEXT("Reinitialization removes prior movement hysteresis"), Anim->LocomotionData.bHasMovingSpeed);
	TestFalse(TEXT("Reinitialization does not leak a stop signal"), Anim->LocomotionData.bWantsToStop);
	Anim->NativeUninitializeAnimation();
	TestFalse(TEXT("Uninitialization clears the snapshot"), Anim->LocomotionData.bHasValidMovementData);
	TestTrue(TEXT("Uninitialization clears parameter validity"), !Anim->AnimLogicParams || !Anim->AnimLogicParams->HasValidData());
	TestTrue(TEXT("Another AnimInstance remains valid"), OtherAnim->AnimLogicParams && OtherAnim->AnimLogicParams->HasValidData());
	OtherAnim->NativeUninitializeAnimation();

	USkeletalMeshComponent* PreviewMesh = NewObject<USkeletalMeshComponent>(GetTransientPackage());
	UWuwaAnimInstance* PreviewAnim = NewObject<UWuwaAnimInstance>(PreviewMesh);
	PreviewAnim->NativeInitializeAnimation();
	PreviewAnim->NativeUpdateAnimation(1.f / 60.f);
	TestFalse(TEXT("Ownerless preview remains safely invalid"), PreviewAnim->LocomotionData.bHasValidMovementData);
	PreviewAnim->NativeUninitializeAnimation();
	return true;
}

#endif
