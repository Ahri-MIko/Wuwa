#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/Movement/WuwaTestGait.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Misc/ScopeExit.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/World.h"
#include "Curves/CurveFloat.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintDesireTest, "Wuwa.Locomotion.SprintDesire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintDesireTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Sprint desire test world was created"), TestWorld))
	{
		return false;
	}
	ON_SCOPE_EXIT { TestWorld->DestroyWorld(false); };
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	AWuwaCharacter* Character = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	AWuwaCharacter* OtherCharacter = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
	UWuwaMovementComponent* OtherMovement = OtherCharacter ? OtherCharacter->GetWuwaMovementComponent() : nullptr;
	if (!TestNotNull(TEXT("First character owns its movement component"), Movement)
		|| !TestNotNull(TEXT("Second character owns its movement component"), OtherMovement))
	{
		return false;
	}

	// No world tick is needed: use the same game-time clock as the routed input hold duration.
	// UObject 本身在 UE 5.7 标为 abstract；使用具体的 UObject 子类作为窗口来源。
	UObject* WindowA = NewObject<UCurveFloat>(Character);
	UObject* WindowB = NewObject<UCurveFloat>(Character);
	constexpr float HoldThreshold = 0.2f;
	Movement->MovementMode = MOVE_Walking;
	OtherMovement->MovementMode = MOVE_Walking;
	// 未初始化 Actor 的测试世界不会调用 PostInitializeComponents：设好移动模式后显式装配运动状态。
	if (!TestTrue(TEXT("First character assembles its movement state"), Character->EnsureMovementStateSystem())) return false;
	if (!TestTrue(TEXT("Second character assembles its movement state"), OtherCharacter->EnsureMovementStateSystem())) return false;
	Character->InputIntent->SetMoveAxis(FVector2D(0.f, 1.f));
	OtherCharacter->InputIntent->SetMoveAxis(FVector2D(0.f, 1.f));
	WuwaTestGait::SetDesiredGait(Movement, EWuwaGait::Walk);
	TestTrue(TEXT("No window means no sprint desire"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("An unpaired tick cannot open a window"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	TestWorld->TimeSeconds = 1.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestTrue(TEXT("A newly opened window requests temporary sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 1.1;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("Holding before the window does not count toward its threshold"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 1.3;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, HoldThreshold, HoldThreshold);
	TestTrue(TEXT("Exactly 0.2 seconds remains temporary"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.21f, HoldThreshold);
	TestTrue(TEXT("More than 0.2 seconds requests sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("Sustained desire preserves the walk preference"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Ending the last window retains sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	TestWorld->TimeSeconds = 60.0;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.f, HoldThreshold);
	TestTrue(TEXT("Elapsed time and late release sampling cannot demote committed sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	// RootMotion may still move the actor after input release. Intent, not speed, ends sustained sprint.
	Movement->Velocity = FVector(900.f, 0.f, 0.f);
	WuwaTestInput::SetMoveAxis(Character, FVector2D::ZeroVector);
	TestTrue(TEXT("Releasing movement clears sustained sprint despite remaining velocity"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestInput::SetMoveAxis(Character, FVector2D(0.f, 1.f));
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 1.f, HoldThreshold);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Moving again and late callbacks cannot revive a completed request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	TestWorld->TimeSeconds = 2.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestWorld->TimeSeconds = 2.5;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.05f, HoldThreshold);
	TestTrue(TEXT("A late key press uses held duration rather than window age"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.7;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.25f, HoldThreshold);
	TestTrue(TEXT("A late press can still reach sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.f, HoldThreshold);
	TestTrue(TEXT("Release clears the long-hold request without ending the window"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.8;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.1f, HoldThreshold);
	TestTrue(TEXT("Repressing does not inherit the previous held duration"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.95;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.25f, HoldThreshold);
	TestTrue(TEXT("The new continuous press can reach the threshold again"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->ResetSprintRequest();

	TestWorld->TimeSeconds = 3.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestWorld->TimeSeconds = 3.3;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.3f, HoldThreshold);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowB);
	TestTrue(TEXT("Overlapping windows retain the strongest request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowB);
	TestTrue(TEXT("Ending another source cannot clear a sustained window"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowB);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Ending the stronger source commits its sustained request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("A duplicate end cannot clear the retained request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowB);
	TestTrue(TEXT("A weaker window cannot demote retained sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->ResetSprintRequest();

	// AnimNotifyState objects can be shared: the source must not own character-specific state.
	TestWorld->TimeSeconds = 4.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestWorld->TimeSeconds = 4.3;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.3f, HoldThreshold);
	WuwaTestGait::Of(OtherMovement)->OpenSprintWindow(WindowA);
	WuwaTestGait::Of(OtherMovement)->SampleSprintWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("The first character retains sustained desire"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("A shared source has a separate start time for the second character"), WuwaTestGait::Of(OtherMovement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Ending the first character's window does not clear the second"), WuwaTestGait::Of(OtherMovement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 4.6;
	WuwaTestGait::Of(OtherMovement)->SampleSprintWindow(WindowA, 0.3f, HoldThreshold);
	TestTrue(TEXT("The second character can promote its own request"), WuwaTestGait::Of(OtherMovement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("The first character retains its own committed request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(OtherMovement)->CloseSprintWindow(WindowA);
	WuwaTestGait::Of(OtherMovement)->ResetSprintRequest();
	TestTrue(TEXT("Explicit reset clears only the second character"), WuwaTestGait::Of(OtherMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("The first character's retained request is independent"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	WuwaTestGait::Of(Movement)->ResetSprintRequest();

	// Temporary duration starts at End, not Begin, and is not refreshed by duplicate callbacks.
	TestEqual(TEXT("The requested default duration is one second"), Character->RoleGaitComponent->TemporarySprintDuration, 1.f);
	TestWorld->TimeSeconds = 10.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestWorld->TimeSeconds = 15.0;
	TestTrue(TEXT("An open window does not consume the post-window temporary duration"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestWorld->TimeSeconds = 15.99;
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("Temporary sprint survives until one second after the window ended"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 16.0;
	TestTrue(TEXT("Temporary sprint expires exactly at its deadline without a notify tick"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	Character->RoleGaitComponent->TemporarySprintDuration = 0.25f;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestWorld->TimeSeconds = 16.24;
	TestTrue(TEXT("A subsequent dash creates a fresh configurable temporary duration"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 16.25;
	TestTrue(TEXT("The configured temporary duration is honored"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Character->RoleGaitComponent->TemporarySprintDuration = 0.f;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Zero duration adds no post-window temporary sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Character->RoleGaitComponent->TemporarySprintDuration = 1.f;

	// A release during Dash must not leave sustained intent when the window later ends.
	TestWorld->TimeSeconds = 20.0;
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestWorld->TimeSeconds = 20.3;
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowA, 0.3f, HoldThreshold);
	WuwaTestInput::SetMoveAxis(Character, FVector2D::ZeroVector);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	TestTrue(TEXT("Ending a sustained window without movement input cannot latch sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestInput::SetMoveAxis(Character, FVector2D(0.f, 1.f));
	TestTrue(TEXT("Movement resuming after that window does not restore sustained sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	// Leaving ground movement cancels both committed intent and unfinished sampling.
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowA);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowB);
	Movement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Leaving ground movement clears retained and active requests"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowB, 1.f, HoldThreshold);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowB);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowA);
	TestTrue(TEXT("Late callbacks and new ground windows cannot resurrect sprint while falling"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Movement->SetMovementMode(MOVE_Walking);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowB);
	TestTrue(TEXT("Landing does not revive the previous ground sprint"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("All window changes preserve the saved walk preference"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	return true;
}

#endif
