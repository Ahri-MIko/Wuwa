#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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
	Character->MoveInput = FVector2D(0.f, 1.f);
	OtherCharacter->MoveInput = FVector2D(0.f, 1.f);
	Movement->SetDesiredGait(EWuwaGait::Walk);
	TestTrue(TEXT("No window means no sprint desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->UpdateSprintDesireWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("An unpaired tick cannot open a window"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

	TestWorld->TimeSeconds = 1.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestTrue(TEXT("A newly opened window requests temporary sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 1.1;
	Movement->UpdateSprintDesireWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("Holding before the window does not count toward its threshold"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 1.3;
	Movement->UpdateSprintDesireWindow(WindowA, HoldThreshold, HoldThreshold);
	TestTrue(TEXT("Exactly 0.2 seconds remains temporary"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->UpdateSprintDesireWindow(WindowA, 0.21f, HoldThreshold);
	TestTrue(TEXT("More than 0.2 seconds requests sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("Sustained desire preserves the walk preference"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Ending the last window retains sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	TestWorld->TimeSeconds = 60.0;
	Movement->UpdateSprintDesireWindow(WindowA, 0.f, HoldThreshold);
	TestTrue(TEXT("Elapsed time and late release sampling cannot demote committed sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	// RootMotion may still move the actor after input release. Intent, not speed, ends sustained sprint.
	Movement->Velocity = FVector(900.f, 0.f, 0.f);
	Character->HandleMoveInput(FInputActionValue(FVector2D::ZeroVector));
	TestTrue(TEXT("Releasing movement clears sustained sprint despite remaining velocity"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Character->HandleMoveInput(FInputActionValue(FVector2D(0.f, 1.f)));
	Movement->UpdateSprintDesireWindow(WindowA, 1.f, HoldThreshold);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Moving again and late callbacks cannot revive a completed request"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

	TestWorld->TimeSeconds = 2.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 2.5;
	Movement->UpdateSprintDesireWindow(WindowA, 0.05f, HoldThreshold);
	TestTrue(TEXT("A late key press uses held duration rather than window age"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.7;
	Movement->UpdateSprintDesireWindow(WindowA, 0.25f, HoldThreshold);
	TestTrue(TEXT("A late press can still reach sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->UpdateSprintDesireWindow(WindowA, 0.f, HoldThreshold);
	TestTrue(TEXT("Release clears the long-hold request without ending the window"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.8;
	Movement->UpdateSprintDesireWindow(WindowA, 0.1f, HoldThreshold);
	TestTrue(TEXT("Repressing does not inherit the previous held duration"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 2.95;
	Movement->UpdateSprintDesireWindow(WindowA, 0.25f, HoldThreshold);
	TestTrue(TEXT("The new continuous press can reach the threshold again"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->EndSprintDesireWindow(WindowA);
	Movement->ClearSprintDesire();

	TestWorld->TimeSeconds = 3.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 3.3;
	Movement->UpdateSprintDesireWindow(WindowA, 0.3f, HoldThreshold);
	Movement->BeginSprintDesireWindow(WindowB);
	TestTrue(TEXT("Overlapping windows retain the strongest request"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->EndSprintDesireWindow(WindowB);
	TestTrue(TEXT("Ending another source cannot clear a sustained window"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->BeginSprintDesireWindow(WindowB);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Ending the stronger source commits its sustained request"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("A duplicate end cannot clear the retained request"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->EndSprintDesireWindow(WindowB);
	TestTrue(TEXT("A weaker window cannot demote retained sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->ClearSprintDesire();

	// AnimNotifyState objects can be shared: the source must not own character-specific state.
	TestWorld->TimeSeconds = 4.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 4.3;
	Movement->UpdateSprintDesireWindow(WindowA, 0.3f, HoldThreshold);
	OtherMovement->BeginSprintDesireWindow(WindowA);
	OtherMovement->UpdateSprintDesireWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("The first character retains sustained desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("A shared source has a separate start time for the second character"), OtherMovement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Ending the first character's window does not clear the second"), OtherMovement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 4.6;
	OtherMovement->UpdateSprintDesireWindow(WindowA, 0.3f, HoldThreshold);
	TestTrue(TEXT("The second character can promote its own request"), OtherMovement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	TestTrue(TEXT("The first character retains its own committed request"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	OtherMovement->EndSprintDesireWindow(WindowA);
	OtherMovement->ClearSprintDesire();
	TestTrue(TEXT("Explicit reset clears only the second character"), OtherMovement->GetSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("The first character's retained request is independent"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Movement->ClearSprintDesire();

	// Temporary duration starts at End, not Begin, and is not refreshed by duplicate callbacks.
	TestEqual(TEXT("The requested default duration is one second"), Movement->TemporarySprintDuration, 1.f);
	TestWorld->TimeSeconds = 10.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 15.0;
	TestTrue(TEXT("An open window does not consume the post-window temporary duration"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->EndSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 15.99;
	Movement->EndSprintDesireWindow(WindowA);
	Movement->UpdateSprintDesireWindow(WindowA, 5.f, HoldThreshold);
	TestTrue(TEXT("Temporary sprint survives until one second after the window ended"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 16.0;
	TestTrue(TEXT("Temporary sprint expires exactly at its deadline without a notify tick"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

	Movement->TemporarySprintDuration = 0.25f;
	Movement->BeginSprintDesireWindow(WindowA);
	Movement->EndSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 16.24;
	TestTrue(TEXT("A subsequent dash creates a fresh configurable temporary duration"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	TestWorld->TimeSeconds = 16.25;
	TestTrue(TEXT("The configured temporary duration is honored"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->TemporarySprintDuration = 0.f;
	Movement->BeginSprintDesireWindow(WindowA);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Zero duration adds no post-window temporary sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->TemporarySprintDuration = 1.f;

	// A release during Dash must not leave sustained intent when the window later ends.
	TestWorld->TimeSeconds = 20.0;
	Movement->BeginSprintDesireWindow(WindowA);
	TestWorld->TimeSeconds = 20.3;
	Movement->UpdateSprintDesireWindow(WindowA, 0.3f, HoldThreshold);
	Character->HandleMoveInput(FInputActionValue(FVector2D::ZeroVector));
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Ending a sustained window without movement input cannot latch sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Character->HandleMoveInput(FInputActionValue(FVector2D(0.f, 1.f)));
	TestTrue(TEXT("Movement resuming after that window does not restore sustained sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

	// Leaving ground movement cancels both committed intent and unfinished sampling.
	Movement->BeginSprintDesireWindow(WindowA);
	Movement->EndSprintDesireWindow(WindowA);
	Movement->BeginSprintDesireWindow(WindowB);
	Movement->SetMovementMode(MOVE_Falling);
	TestTrue(TEXT("Leaving ground movement clears retained and active requests"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->UpdateSprintDesireWindow(WindowB, 1.f, HoldThreshold);
	Movement->EndSprintDesireWindow(WindowB);
	Movement->BeginSprintDesireWindow(WindowA);
	TestTrue(TEXT("Late callbacks and new ground windows cannot resurrect sprint while falling"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->SetMovementMode(MOVE_Walking);
	Movement->EndSprintDesireWindow(WindowB);
	TestTrue(TEXT("Landing does not revive the previous ground sprint"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("All window changes preserve the saved walk preference"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	return true;
}

#endif
