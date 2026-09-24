#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/World.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Common/WuwaGameTags.h"

namespace
{
	struct FScopedSprintInputWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaPlayerController* Controller = nullptr;

		FScopedSprintInputWorld()
		{
			if (World)
			{
				FActorSpawnParameters Parameters;
				Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Parameters.ObjectFlags |= RF_Transient;
				Controller = World->SpawnActor<AWuwaPlayerController>(Parameters);
			}
		}

		~FScopedSprintInputWorld()
		{
			if (IsValid(Controller))
			{
				Controller->SetPawn(nullptr);
			}
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
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Parameters.ObjectFlags |= RF_Transient;
			return World->SpawnActor<AWuwaCharacter>(Parameters);
		}

		UWuwaInputRouterComponent* Router() const
		{
			return Controller ? Controller->GetInputRouter() : nullptr;
		}

		bool Dispatch(const FInputDataAsset& Binding, EWuwaInputPhase Phase, double Time) const
		{
			World->TimeSeconds = Time;
			FWuwaInputEvent Event;
			Event.Phase = Phase;
			Event.Timestamp = Time;
			return Router()->DispatchInput(Binding, Event);
		}
	};

	FInputDataAsset MakeSprintBinding(UObject* Outer, FGameplayTag Tag, FGameplayTag Route)
	{
		FInputDataAsset Binding;
		Binding.InputAction = NewObject<UInputAction>(Outer);
		Binding.InputTag = Tag;
		Binding.RouteTag = Route;
		return Binding;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputSourcesTest, "Wuwa.Input.Sprint.SemanticHoldSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputSourcesTest::RunTest(const FString& Parameters)
{
	FScopedSprintInputWorld Fixture;
	UWuwaInputRouterComponent* Router = Fixture.Router();
	if (!TestNotNull(TEXT("The controller owns a router"), Router))
	{
		return false;
	}
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	const FInputDataAsset First = MakeSprintBinding(Fixture.Controller, Tags.Abilities_Movement_Dash, Tags.Input_Route_Ability);
	const FInputDataAsset Second = MakeSprintBinding(Fixture.Controller, Tags.Abilities_Movement_Dash, Tags.Input_Route_Ability);
	if (!TestTrue(TEXT("The semantic input binding is configured"), First.IsConfigured()))
	{
		return false;
	}

	// State must be recorded independently of whether a gameplay handler accepts the command.
	TestFalse(TEXT("Without a handler, dispatch still reports unhandled"), Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 1.0));
	TestTrue(TEXT("An unhandled action still has a held input state"), Router->GetInputActionState(First.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 1.1);
	Fixture.Dispatch(First, EWuwaInputPhase::Triggered, 1.12);
	Fixture.Dispatch(Second, EWuwaInputPhase::Pressed, 1.15);
	Fixture.Dispatch(First, EWuwaInputPhase::Released, 1.2);
	Fixture.World->TimeSeconds = 1.3;
	const FWuwaInputActionState Overlapping = Router->GetInputActionState(First.InputTag);
	TestTrue(TEXT("Releasing one action leaves the other source held"), Overlapping.bHeld);
	TestTrue(TEXT("Duplicate presses, triggers and overlapping sources preserve continuous semantic hold time"),
		FMath::IsNearlyEqual(Overlapping.HeldSeconds, 0.3f, 0.0001f));

	Fixture.Dispatch(Second, EWuwaInputPhase::Released, 1.4);
	TestFalse(TEXT("Releasing the final source clears the semantic hold"), Router->GetInputActionState(First.InputTag).bHeld);
	TestEqual(TEXT("A released action reports zero held time"), Router->GetInputActionState(First.InputTag).HeldSeconds, 0.f);
	Fixture.Dispatch(Second, EWuwaInputPhase::Triggered, 1.5);
	TestFalse(TEXT("A late Triggered event cannot resurrect a released action"), Router->GetInputActionState(First.InputTag).bHeld);
	Fixture.Dispatch(Second, EWuwaInputPhase::Pressed, 1.6);
	Fixture.Dispatch(Second, EWuwaInputPhase::Canceled, 1.7);
	TestFalse(TEXT("Canceled clears the source just like release"), Router->GetInputActionState(First.InputTag).bHeld);

	// Dispatch must use the configured binding, not an event's unrelated tag or source object.
	Fixture.World->TimeSeconds = 2.0;
	FWuwaInputEvent ForgedEvent;
	ForgedEvent.InputTag = Tags.Player_Common_Movement_WalkRun;
	ForgedEvent.SourceAction = NewObject<UInputAction>(Fixture.Controller);
	ForgedEvent.Phase = EWuwaInputPhase::Pressed;
	ForgedEvent.Timestamp = 2.0;
	Router->DispatchInput(First, ForgedEvent);
	TestTrue(TEXT("The binding's tag receives the held state"), Router->GetInputActionState(First.InputTag).bHeld);
	TestFalse(TEXT("The caller's mismatched tag is not recorded"), Router->GetInputActionState(ForgedEvent.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Released, 2.1);
	TestFalse(TEXT("Release matches the binding source rather than the caller's mismatched source"), Router->GetInputActionState(First.InputTag).bHeld);

	Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 3.0);
	Router->ResetInputStates();
	TestFalse(TEXT("Reset clears all action states"), Router->GetInputActionState(First.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Triggered, 3.1);
	TestFalse(TEXT("Triggered cannot recreate an input cleared by reset"), Router->GetInputActionState(First.InputTag).bHeld);
	TestFalse(TEXT("An invalid tag has no held input"), Router->GetInputActionState(FGameplayTag()).bHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputLifecycleTest, "Wuwa.Input.Sprint.ConfigurationAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedSprintInputWorld Fixture;
	if (!TestNotNull(TEXT("A controller was spawned"), Fixture.Controller)
		|| !TestNotNull(TEXT("The controller has a router"), Fixture.Router()))
	{
		return false;
	}
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	const FInputDataAsset Dash = MakeSprintBinding(Fixture.Controller, Tags.Abilities_Movement_Dash, Tags.Input_Route_Ability);
	const FInputDataAsset Remapped = MakeSprintBinding(Fixture.Controller, Tags.Player_Common_Camera_Rotate, Tags.Input_Route_Ability);
	Fixture.Controller->RegisterInputRouteHandlers();

	// A controller with no Pawn/ASC must still receive semantic input safely.
	TestNull(TEXT("The fixture starts without an ASC"), Fixture.Controller->GetASC());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.0);
	Fixture.World->TimeSeconds = 1.3;
	TestTrue(TEXT("The router records input even without a Pawn or ASC"), Fixture.Router()->GetInputActionState(Dash.InputTag).bHeld);
	TestTrue(TEXT("The router reports elapsed game time without requiring an ability handler to accept input"),
		FMath::IsNearlyEqual(Fixture.Router()->GetInputActionState(Dash.InputTag).HeldSeconds, 0.3f, 0.0001f));
	TestFalse(TEXT("The controller exposes no character sprint request without a controlled Pawn"), Fixture.Controller->GetSprintInputState().bHeld);
	TestEqual(TEXT("The no-Pawn view has no held duration"), Fixture.Controller->GetSprintInputState().HeldSeconds, 0.f);
	Fixture.Controller->FlushPressedKeys();
	TestFalse(TEXT("Flushing player input clears the router's held state"), Fixture.Router()->GetInputActionState(Dash.InputTag).bHeld);

	AWuwaCharacter* FirstCharacter = Fixture.SpawnCharacter();
	AWuwaCharacter* SecondCharacter = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("The first character was spawned"), FirstCharacter)
		|| !TestNotNull(TEXT("The second character was spawned"), SecondCharacter))
	{
		return false;
	}
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.31);
	// Establish the snapshot association without Possess/PlayerState initialization or a world tick.
	Fixture.Controller->SetPawn(FirstCharacter);
	FirstCharacter->Controller = Fixture.Controller;
	TestFalse(TEXT("Taking control of a Pawn discards input held before possession"), Fixture.Controller->GetSprintInputState().bHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.4);
	Fixture.World->TimeSeconds = 1.6;
	TestTrue(TEXT("A controlled Pawn receives the default Dash semantic input as its sprint request"), Fixture.Controller->GetSprintInputState().bHeld);
	FWuwaPlayerInputState Snapshot = FirstCharacter->GetPlayerInputState();
	TestTrue(TEXT("The character snapshot carries the routed sprint hold"), Snapshot.bSprintHeld);
	TestTrue(TEXT("The character snapshot carries the action's held duration"),
		FMath::IsNearlyEqual(Snapshot.SprintHeldSeconds, 0.2f, 0.0001f));
	TestFalse(TEXT("An uncontrolled character receives no sprint hold"), SecondCharacter->GetPlayerInputState().bSprintHeld);

	UWuwaMovementComponent* Movement = FirstCharacter->GetWuwaMovementComponent();
	if (!TestNotNull(TEXT("The character has a movement component for its notify window"), Movement))
	{
		return false;
	}
	UObject* WindowSource = NewObject<UInputAction>(FirstCharacter);
	Movement->BeginSprintDesireWindow(WindowSource);
	Fixture.World->TimeSeconds = 1.7;
	Snapshot = FirstCharacter->GetPlayerInputState();
	Movement->UpdateSprintDesireWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("The snapshot's pre-window hold cannot skip the 0.2 second window requirement"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Fixture.World->TimeSeconds = 1.81;
	Snapshot = FirstCharacter->GetPlayerInputState();
	Movement->UpdateSprintDesireWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("More than 0.2 seconds of window hold reaches sustained desire through the snapshot"), Movement->GetSprintDesire() == EWuwaSprintDesire::Sustained);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Released, 1.82);
	Snapshot = FirstCharacter->GetPlayerInputState();
	TestFalse(TEXT("Release propagates to the character snapshot"), Snapshot.bSprintHeld);
	TestEqual(TEXT("The released snapshot clears held duration"), Snapshot.SprintHeldSeconds, 0.f);
	Movement->UpdateSprintDesireWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("A released semantic action demotes an open window to temporary desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::Temporary);
	Movement->EndSprintDesireWindow(WindowSource);
	TestTrue(TEXT("The completed window leaves no sprint desire"), Movement->GetSprintDesire() == EWuwaSprintDesire::None);

	// Reusing a registered tag here tests configuration without introducing a physical key dependency.
	Fixture.Controller->SprintInputTag = Remapped.InputTag;
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 2.0);
	TestFalse(TEXT("After semantic remapping, the original action is no longer the sprint request"), Fixture.Controller->GetSprintInputState().bHeld);
	Fixture.Dispatch(Remapped, EWuwaInputPhase::Pressed, 2.1);
	Fixture.World->TimeSeconds = 2.4;
	TestTrue(TEXT("The configured semantic action now provides sprint hold"), Fixture.Controller->GetSprintInputState().bHeld);
	TestTrue(TEXT("The remapped action has its own press time"),
		FMath::IsNearlyEqual(Fixture.Controller->GetSprintInputState().HeldSeconds, 0.3f, 0.0001f));
	Fixture.Dispatch(Remapped, EWuwaInputPhase::Canceled, 2.5);
	TestFalse(TEXT("Canceling the remapped action clears the configured sprint request"), Fixture.Controller->GetSprintInputState().bHeld);
	Fixture.Controller->SprintInputTag = FGameplayTag();
	TestTrue(TEXT("An empty configuration falls back to the existing Dash semantic input"), Fixture.Controller->GetSprintInputState().bHeld);

	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 3.0);
	Fixture.Controller->SetPawn(SecondCharacter);
	SecondCharacter->Controller = Fixture.Controller;
	TestFalse(TEXT("Changing Pawn cannot carry the previous character's sprint hold"), Fixture.Controller->GetSprintInputState().bHeld);
	TestFalse(TEXT("The former Pawn cannot read input through its stale controller pointer"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	TestFalse(TEXT("The newly controlled Pawn starts with an empty sprint snapshot"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Triggered, 3.1);
	TestFalse(TEXT("A stale Triggered callback cannot restart sprint after switching Pawn"), Fixture.Controller->GetSprintInputState().bHeld);

	// Existing semantic commands continue through the same router and affect only the current Pawn.
	FirstCharacter->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
	SecondCharacter->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
	const FInputDataAsset WalkRun = MakeSprintBinding(Fixture.Controller, Tags.Player_Common_Movement_WalkRun, Tags.Input_Route_Movement);
	TestTrue(TEXT("Walk/run is still handled by the existing movement command route"), Fixture.Dispatch(WalkRun, EWuwaInputPhase::Pressed, 4.0));
	TestTrue(TEXT("The controlled character switches to Walk"), SecondCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The other character retains Run"), FirstCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Run);
	TestFalse(TEXT("Walk/run input does not become a sprint request"), Fixture.Controller->GetSprintInputState().bHeld);
	Fixture.Dispatch(WalkRun, EWuwaInputPhase::Released, 4.1);
	TestTrue(TEXT("Releasing walk/run does not toggle the gait again"), SecondCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Walk);

	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 5.0);
	TestTrue(TEXT("A fresh press reaches the newly controlled character"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	TestFalse(TEXT("A fresh press does not leak to the previous character"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Controller->FlushPressedKeys();
	TestFalse(TEXT("Flushing input clears the current character snapshot"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 5.1);
	Fixture.Controller->SetPawn(nullptr);
	TestFalse(TEXT("Unpossessing clears held sprint input"), Fixture.Controller->GetSprintInputState().bHeld);
	TestFalse(TEXT("An unpossessed character exposes no held sprint input"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputAssetsTest, "Wuwa.Input.Sprint.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputAssetsTest::RunTest(const FString& Parameters)
{
	const UInputAction* DodgeAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Dodge.IA_Dodge"));
	const UWuwaInputDataAsset* InputTagMap = LoadObject<UWuwaInputDataAsset>(nullptr, TEXT("/Game/CoreInput/DataAsset/DA_InputActionTagAsset.DA_InputActionTagAsset"));
	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	if (!TestNotNull(TEXT("The existing Dodge action loads"), DodgeAction)
		|| !TestNotNull(TEXT("The current input tag asset loads"), InputTagMap)
		|| !TestNotNull(TEXT("The character mapping context loads"), MappingContext))
	{
		return false;
	}
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	int32 DodgeBindings = 0;
	for (const FInputDataAsset& Binding : InputTagMap->InputDataAssetMap)
	{
		if (Binding.InputAction == DodgeAction)
		{
			++DodgeBindings;
			TestTrue(TEXT("Dodge maps to the existing Dash semantic input"), Binding.InputTag == Tags.Abilities_Movement_Dash);
			TestTrue(TEXT("Dodge remains on the ability input route"), Binding.RouteTag == Tags.Input_Route_Ability);
		}
	}
	TestEqual(TEXT("Dodge has one configured semantic binding"), DodgeBindings, 1);
	bool bDodgeMapped = false;
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		bDodgeMapped |= Mapping.Action == DodgeAction;
	}
	TestTrue(TEXT("A device mapping supplies Dodge without the gameplay code depending on a particular key"), bDodgeMapped);
	return true;
}

#endif
