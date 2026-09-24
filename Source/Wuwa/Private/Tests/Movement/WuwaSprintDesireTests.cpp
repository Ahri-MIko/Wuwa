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
	TestTrue(TEXT("Ending the last window clears sprint desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	Movement->UpdateSprintDesireWindow(WindowA, 1.f, HoldThreshold);
	TestTrue(TEXT("A late tick after end cannot reopen the window"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

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
	TestTrue(TEXT("Ending the stronger source reveals the remaining temporary request"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("A duplicate end cannot clear a different source"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->EndSprintDesireWindow(WindowB);
	TestTrue(TEXT("Ending all overlapping windows clears desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

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
	TestTrue(TEXT("The first character remains outside a window"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);
	OtherMovement->EndSprintDesireWindow(WindowA);
	TestTrue(TEXT("Second character cleanup clears its request"), OtherMovement->GetSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("All window changes preserve the saved walk preference"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	return true;
}

#endif
