#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/Movement/WuwaTestGait.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "UObject/Script.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "WuwaSkillTestAbility.h"

namespace WuwaSkillLifecycleTests
{
	// The test class only copies request configuration; the production native base
	// owns the activation guard and the real managed policy, with no graph or montage.
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaSkillBridgeComponent* Skills = nullptr;
		UWuwaFightStateBridgeComponent* Fight = nullptr;
		UWuwaGameplayAbilityBase* Defaults = GetMutableDefault<UWuwaSkillTestAbility>();
		const bool OriginalMainSkill = Defaults->bIsMainSkill;
		const int32 OriginalInterruptLevel = Defaults->InterruptLevel;
		const EWuwaSkillOverrideType OriginalOverrideType = Defaults->SkillOverrideType;
		const EWuwaMoveState OriginalStartMoveState = Defaults->StartMoveState;
		int32 ActivationNotifications = 0;
		FGameplayAbilitySpecHandle ReentryCandidate;
		bool bProbeOnNextEnd = false;
		bool bReentryAttempted = false;
		bool bReentryAccepted = false;

		~FFixture()
		{
			bProbeOnNextEnd = false;
			if (ASC)
			{
				ASC->CancelAllAbilities();
				ASC->ClearActorInfo();
			}
			if (World) World->DestroyWorld(false);
			Defaults->bIsMainSkill = OriginalMainSkill;
			Defaults->InterruptLevel = OriginalInterruptLevel;
			Defaults->SkillOverrideType = OriginalOverrideType;
			Defaults->StartMoveState = OriginalStartMoveState;
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AWuwaCharacter>(Spawn);
			if (!Test.TestNotNull(TEXT("Character exists"), Character)) return false;
			Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!Test.TestTrue(TEXT("Managed movement assembles"), Character->EnsureMovementStateSystem())
				|| !Test.TestTrue(TEXT("Managed skill system assembles"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RefreshPolicy();
			Skills = Character->SkillComponent;
			Fight = Character->FightStateComponent;
			if (!Test.TestNotNull(TEXT("Skill bridge exists"), Skills)
				|| !Test.TestNotNull(TEXT("Fight bridge exists"), Fight)
				|| !Test.TestEqual(TEXT("Skill policy is the actual managed generated class"),
					Skills->GetClass()->GetClass()->GetFName(), FName(TEXT("CSClass")))) return false;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*) { ++ActivationNotifications; });
			ASC->AbilityEndedCallbacks.AddLambda([this](UGameplayAbility*)
			{
				if (!bProbeOnNextEnd) return;
				bProbeOnNextEnd = false;
				bReentryAttempted = true;
				bReentryAccepted = ASC->TryActivateAbility(ReentryCandidate, false);
			});
			Configure(100);
			return Test.TestTrue(TEXT("Native ability owns per-execution context"),
				Defaults->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution);
		}

		void Configure(int32 InterruptLevel, bool bStartDodge = false,
			EWuwaSkillOverrideType OverrideType = EWuwaSkillOverrideType::None)
		{
			Defaults->bIsMainSkill = true;
			Defaults->InterruptLevel = InterruptLevel;
			Defaults->SkillOverrideType = OverrideType;
			Defaults->StartMoveState = bStartDodge ? EWuwaMoveState::Dodge : EWuwaMoveState::Other;
		}

		FWuwaUnifiedStateData MoveData() const { return Character->UnifiedStateComponent->GetStateData(); }

		FGameplayAbilitySpecHandle Grant() const
		{
			return ASC->GiveAbility(FGameplayAbilitySpec(UWuwaSkillTestAbility::StaticClass(), 1));
		}

		UWuwaGameplayAbilityBase* GetActiveAbility(FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (!Spec) return nullptr;
			for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
			{
				if (UWuwaGameplayAbilityBase* Ability = Cast<UWuwaGameplayAbilityBase>(Instance);
					IsValid(Ability) && Ability->IsActive()) return Ability;
			}
			return nullptr;
		}

		UWuwaGameplayAbilityBase* Start(FAutomationTestBase& Test, FGameplayAbilitySpecHandle Handle) const
		{
			const FString Context = FString::Printf(TEXT("Skill %s (level=%d, main=%d, startMove=%d, override=%d): "),
				*Handle.ToString(), Defaults->InterruptLevel, Defaults->bIsMainSkill,
				static_cast<int32>(Defaults->StartMoveState), static_cast<int32>(Defaults->SkillOverrideType));
			if (!Test.TestTrue(Context + TEXT("GAS accepts activation"), ASC->TryActivateAbility(Handle, false))) return nullptr;
			UWuwaGameplayAbilityBase* Ability = GetActiveAbility(Handle);
			return Test.TestNotNull(Context + TEXT("execution remains active after the native activation guard"), Ability) ? Ability : nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillAssemblyLifecycleTest, "Wuwa.Combat.Skill.ManagedAssemblyAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillAssemblyLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	TestEqual(TEXT("Manager starts without a skill"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	const FWuwaUnifiedStateData MovementBefore = F.Character->UnifiedStateComponent->GetStateData();
	const FGameplayAbilitySpecHandle MainSpec = F.Grant();
	UWuwaGameplayAbilityBase* Main = F.Start(*this, MainSpec);
	if (!Main) return false;
	const int32 Handle = Main->GetSkillHandle();
	TestTrue(TEXT("Main skill receives a positive fight handle"), Handle > 0);
	const FWuwaSkillData Current = F.Skills->GetCurrentSkillData();
	TestEqual(TEXT("Manager records the executing GA instance"), Current.ActiveAbility.Get(), Main);
	TestEqual(TEXT("GA, manager and fight state share the same handle"), Current.FightStateHandle, Handle);
	TestEqual(TEXT("Fight state owns the skill handle"), F.Fight->GetStateData().Handle, Handle);
	TestTrue(TEXT("Normal skill enters the Skill category"), F.Fight->GetStateData().State == EWuwaFightState::Skill);
	TestEqual(TEXT("Manager snapshots configured interrupt level"), Current.InterruptLevel, 100);
	TestFalse(TEXT("Accept-input window starts closed"), Current.bSkillAcceptInput);
	TestFalse(TEXT("Ready-end window starts closed"), Current.bMainSkillReadyEnd);
	TestEqual(TEXT("A skill without move override leaves movement revision untouched"),
		F.Character->UnifiedStateComponent->GetStateData().Revision, MovementBefore.Revision);
	TestFalse(TEXT("Registering a skill does not write an action move state"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	TestTrue(TEXT("Repeated assembly succeeds"), F.Character->EnsureSkillSystem());
	TestEqual(TEXT("Repeated assembly preserves the component"), F.Character->SkillComponent.Get(), F.Skills);
	TestEqual(TEXT("Repeated assembly preserves its active skill"), F.Skills->GetCurrentSkillData().FightStateHandle, Handle);

	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AWuwaCharacter* Other = F.World->SpawnActor<AWuwaCharacter>(Spawn);
	if (!TestNotNull(TEXT("Another character exists"), Other)
		|| !TestTrue(TEXT("Another character assembles independently"), Other->EnsureSkillSystem())) return false;
	TestTrue(TEXT("Characters have independent skill components"), Other->SkillComponent.Get() != F.Skills);
	TestEqual(TEXT("Other character has no current skill"), Other->SkillComponent->GetCurrentSkillData().FightStateHandle, 0);

	F.Defaults->bIsMainSkill = false;
	const FGameplayAbilitySpecHandle AuxiliarySpec = F.Grant();
	UWuwaGameplayAbilityBase* Auxiliary = F.Start(*this, AuxiliarySpec);
	if (!Auxiliary) return false;
	TestEqual(TEXT("Opted-out abilities receive no skill handle"), Auxiliary->GetSkillHandle(), 0);
	TestEqual(TEXT("Opted-out ability does not replace the main skill"), F.Skills->GetCurrentSkillData().FightStateHandle, Handle);
	F.ASC->CancelAbilityHandle(AuxiliarySpec);
	TestEqual(TEXT("Auxiliary cancellation does not clear the main skill"), F.Skills->GetCurrentSkillData().FightStateHandle, Handle);

	F.ASC->CancelAbilityHandle(MainSpec);
	TestNull(TEXT("Main spec has no active instance after GAS cancellation"), F.GetActiveAbility(MainSpec));
	TestEqual(TEXT("GAS cancellation clears the manager"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("GAS cancellation releases its fight handle"), F.Fight->GetStateData().Handle, 0);
	TestEqual(TEXT("Normal lifecycle did not publish movement state"),
		F.Character->UnifiedStateComponent->GetStateData().Revision, MovementBefore.Revision);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillInterruptWindowsTest, "Wuwa.Combat.Skill.InterruptWindowsAndReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillInterruptWindowsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle FirstSpec = F.Grant();
	UWuwaGameplayAbilityBase* First = F.Start(*this, FirstSpec);
	if (!First) return false;
	const int32 FirstHandle = First->GetSkillHandle();
	const FGameplayAbilitySpecHandle EqualSpec = F.Grant();
	const int32 BeforeRejected = F.ActivationNotifications;
	TestFalse(TEXT("Equal-priority skill is rejected before the input window"), F.ASC->TryActivateAbility(EqualSpec, false));
	TestEqual(TEXT("Rejected skill emits no GAS activation notification"), F.ActivationNotifications, BeforeRejected);
	TestEqual(TEXT("Rejected skill leaves the old execution active"), F.GetActiveAbility(FirstSpec), First);
	TestEqual(TEXT("Rejected skill preserves its fight handle"), F.Fight->GetStateData().Handle, FirstHandle);

	F.Skills->SetSkillAcceptInput(First->GetSkillHandle(), true);
	TestTrue(TEXT("Owning GA opens the input window"), F.Skills->GetCurrentSkillData().bSkillAcceptInput);
	F.ReentryCandidate = F.Grant();
	F.bProbeOnNextEnd = true;
	UWuwaGameplayAbilityBase* Equal = F.Start(*this, EqualSpec);
	if (!Equal) return false;
	const int32 EqualHandle = Equal->GetSkillHandle();
	TestNull(TEXT("Replacement ends the old GAS execution"), F.GetActiveAbility(FirstSpec));
	TestTrue(TEXT("Replacement receives a new handle"), EqualHandle > FirstHandle);
	TestEqual(TEXT("The manager now points to the new execution"), F.Skills->GetCurrentSkillData().ActiveAbility.Get(), Equal);
	TestFalse(TEXT("Replacement does not inherit the old input window"), F.Skills->GetCurrentSkillData().bSkillAcceptInput);
	TestTrue(TEXT("Old GAS end callback attempted a reentrant activation"), F.bReentryAttempted);
	TestFalse(TEXT("Skill transition rejects the reentrant activation"), F.bReentryAccepted);
	TestNull(TEXT("Reentrant request left no extra active instance"), F.GetActiveAbility(F.ReentryCandidate));
	F.Skills->EndSkill(FirstHandle);
	F.Skills->SetSkillAcceptInput(FirstHandle, true);
	F.Skills->SetMainSkillReadyEnd(FirstHandle, true);
	TestEqual(TEXT("Stale end cannot clear the replacement"), F.Skills->GetCurrentSkillData().FightStateHandle, EqualHandle);
	TestFalse(TEXT("Stale window cannot open the replacement input window"), F.Skills->GetCurrentSkillData().bSkillAcceptInput);
	TestFalse(TEXT("Stale window cannot open the replacement ready-end window"), F.Skills->GetCurrentSkillData().bMainSkillReadyEnd);

	F.Configure(150);
	const FGameplayAbilitySpecHandle HigherSpec = F.Grant();
	UWuwaGameplayAbilityBase* Higher = F.Start(*this, HigherSpec);
	if (!Higher) return false;
	TestNull(TEXT("Higher interrupt level ends the previous execution without an input window"), F.GetActiveAbility(EqualSpec));
	TestEqual(TEXT("Higher skill becomes current"), F.Skills->GetCurrentSkillData().ActiveAbility.Get(), Higher);
	const int32 HigherHandle = Higher->GetSkillHandle();
	Higher->TryEndSkillExecution(EqualHandle);
	TestEqual(TEXT("A mismatched native end request leaves the execution alive"), F.GetActiveAbility(HigherSpec), Higher);
	Higher->TryEndSkillExecution(HigherHandle);
	TestNull(TEXT("Matching native end request ends the execution"), F.GetActiveAbility(HigherSpec));
	TestEqual(TEXT("Matching end clears manager state"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Matching end clears fight state"), F.Fight->GetStateData().Handle, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillReadyEndMovementTest, "Wuwa.Combat.Skill.ReadyEndMovementHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillReadyEndMovementTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaUnifiedStateBridgeComponent* State = F.Character->UnifiedStateComponent;
	UWuwaMovementComponent* Movement = F.Character->GetWuwaMovementComponent();
	F.Configure(200, true);
	const FGameplayAbilitySpecHandle FirstSpec = F.Grant();
	UWuwaGameplayAbilityBase* First = F.Start(*this, FirstSpec);
	if (!First) return false;
	TestTrue(TEXT("Activation writes the configured action state"),
		F.MoveData().MoveState == EWuwaMoveState::Dodge && F.MoveData().bHasActionOverride);
	const int32 FirstHandle = First->GetSkillHandle();
	TestTrue(TEXT("There is no lease: an ordinary write replaces the action state"), State->SetMoveState(EWuwaMoveState::Stand, EWuwaGait::Run));
	TestEqual(TEXT("Replacing the movement state does not end the skill"), F.GetActiveAbility(FirstSpec), First);
	F.Configure(20, true);
	const FGameplayAbilitySpecHandle LowerSpec = F.Grant();
	TestFalse(TEXT("Lower-level skill is rejected before ready-end"), F.ASC->TryActivateAbility(LowerSpec, false));
	F.Skills->SetSkillAcceptInput(First->GetSkillHandle(), true);
	TestFalse(TEXT("Accept-input alone does not allow a lower-level skill"), F.ASC->TryActivateAbility(LowerSpec, false));
	F.Skills->SetMainSkillReadyEnd(First->GetSkillHandle(), true);
	TestTrue(TEXT("Owning GA opens ready-end"), F.Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	UWuwaGameplayAbilityBase* Lower = F.Start(*this, LowerSpec);
	if (!Lower) return false;
	TestNull(TEXT("Ready-end replacement finishes the old GAS execution"), F.GetActiveAbility(FirstSpec));
	TestTrue(TEXT("Lower-level skill receives a new fight handle"), Lower->GetSkillHandle() > FirstHandle);
	TestEqual(TEXT("New interrupt level is recorded"), F.Skills->GetCurrentSkillData().InterruptLevel, 20);
	TestTrue(TEXT("The new skill writes its action state after the old one ended"), F.MoveData().MoveState == EWuwaMoveState::Dodge);
	F.Skills->EndSkill(FirstHandle);
	TestTrue(TEXT("Stale cleanup of the old skill leaves the new action state intact"), F.MoveData().MoveState == EWuwaMoveState::Dodge);

	// Ending the action recomputes at once with the original RoleGait rules.
	Movement->Velocity = FVector(300.f, 0.f, 0.f);
	F.ASC->CancelAbilityHandle(LowerSpec);
	TestEqual(TEXT("Finishing replacement clears skill state"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Finishing replacement clears fight state"), F.Fight->GetStateData().Handle, 0);
	TestTrue(TEXT("Without input, Dodge is kept while the character still moves"), F.MoveData().MoveState == EWuwaMoveState::Dodge);
	Movement->Velocity = FVector::ZeroVector;
	F.Character->RoleGaitComponent->RefreshPolicy();
	TestTrue(TEXT("No-input movement resumes standing once the character stops"), F.MoveData().MoveState == EWuwaMoveState::Stand);
	TestFalse(TEXT("Standing is not an action state"), F.MoveData().bHasActionOverride);

	F.Configure(100, true);
	const FGameplayAbilitySpecHandle StationarySpec = F.Grant();
	if (!F.Start(*this, StationarySpec)) return false;
	TestTrue(TEXT("A stationary action writes Dodge"), F.MoveData().MoveState == EWuwaMoveState::Dodge);
	F.ASC->CancelAbilityHandle(StationarySpec);
	TestTrue(TEXT("Ending a stationary action recomputes Stand immediately, without a movement tick"),
		F.MoveData().MoveState == EWuwaMoveState::Stand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillBeginMoveActionTest, "Wuwa.Combat.Skill.BeginMoveAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillBeginMoveActionTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaUnifiedStateBridgeComponent* State = F.Character->UnifiedStateComponent;
	UWuwaMovementComponent* Movement = F.Character->GetWuwaMovementComponent();

	// Original CharacterSkillComponent: a main skill started from Sprint ends the sprint request and switches to Run.
	UObject* Window = NewObject<UInputAction>(F.Character);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(Window);
	if (!TestTrue(TEXT("A sprint request is open"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary)
		|| !TestTrue(TEXT("The character is sprinting"), State->SetMoveState(EWuwaMoveState::Sprint, EWuwaGait::Sprint))) return false;
	const FGameplayAbilitySpecHandle SprintSpec = F.Grant();
	if (!F.Start(*this, SprintSpec)) return false;
	TestTrue(TEXT("A main skill started from Sprint switches to Run"),
		F.MoveData().MoveState == EWuwaMoveState::Run && F.MoveData().Gait == EWuwaGait::Run);
	TestTrue(TEXT("A main skill started from Sprint ends the sprint request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(Window);
	TestTrue(TEXT("A late window end cannot revive the ended request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	F.ASC->CancelAbilityHandle(SprintSpec);

	// A main skill started from any Stop switches to Stand.
	for (const EWuwaMoveState Stop : { EWuwaMoveState::WalkStop, EWuwaMoveState::RunStop, EWuwaMoveState::SprintStop })
	{
		const FString Name = StaticEnum<EWuwaMoveState>()->GetNameStringByValue(static_cast<int64>(Stop));
		if (!TestTrue(Name + TEXT(" is written"), State->SetMoveState(Stop, EWuwaGait::Run))) return false;
		const FGameplayAbilitySpecHandle StopSpec = F.Grant();
		if (!F.Start(*this, StopSpec)) return false;
		TestTrue(TEXT("A main skill started from ") + Name + TEXT(" switches to Stand"), F.MoveData().MoveState == EWuwaMoveState::Stand);
		F.ASC->CancelAbilityHandle(StopSpec);
	}

	State->SetMoveState(EWuwaMoveState::Walk, EWuwaGait::Walk);
	const FGameplayAbilitySpecHandle WalkSpec = F.Grant();
	if (!F.Start(*this, WalkSpec)) return false;
	TestTrue(TEXT("Other movement states are left unchanged by a skill start"), F.MoveData().MoveState == EWuwaMoveState::Walk);
	F.ASC->CancelAbilityHandle(WalkSpec);

	F.Defaults->bIsMainSkill = false;
	State->SetMoveState(EWuwaMoveState::Sprint, EWuwaGait::Sprint);
	const FGameplayAbilitySpecHandle AuxiliarySpec = F.Grant();
	if (!F.Start(*this, AuxiliarySpec)) return false;
	TestTrue(TEXT("Only main skills apply the start move action"), F.MoveData().MoveState == EWuwaMoveState::Sprint);
	F.ASC->CancelAbilityHandle(AuxiliarySpec);

	F.Configure(100, true);
	State->SetMoveState(EWuwaMoveState::RunStop, EWuwaGait::Run);
	const FGameplayAbilitySpecHandle ActionSpec = F.Grant();
	if (!F.Start(*this, ActionSpec)) return false;
	TestTrue(TEXT("The GA writes its action state after the skill start move action"), F.MoveData().MoveState == EWuwaMoveState::Dodge);
	F.ASC->CancelAbilityHandle(ActionSpec);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillFightOverrideTest, "Wuwa.Combat.Skill.FightStateOverridesAndStaleCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillFightOverrideTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle FirstSpec = F.Grant();
	UWuwaGameplayAbilityBase* First = F.Start(*this, FirstSpec);
	if (!First) return false;
	const int32 FirstHandle = First->GetSkillHandle();
	const int32 HitHandle = F.Fight->TrySwitchState(EWuwaFightState::Hit, 0);
	if (!TestTrue(TEXT("External hit replaces the old fight ownership"), HitHandle > FirstHandle)) return false;
	// Stage 2 has no hit behavior yet: the future hit component will request this end.
	First->TryEndSkillExecution(FirstHandle);
	TestNull(TEXT("The interrupted skill ends normally"), F.GetActiveAbility(FirstSpec));
	TestEqual(TEXT("Old skill cleanup clears its manager record"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Old skill cleanup cannot clear the replacing hit"), F.Fight->GetStateData().Handle, HitHandle);

	const FGameplayAbilitySpecHandle NormalSpec = F.Grant();
	const int32 NotificationsBeforeHitReject = F.ActivationNotifications;
	TestFalse(TEXT("Normal skill cannot override hit"), F.ASC->TryActivateAbility(NormalSpec, false));
	TestEqual(TEXT("Hit rejection happens before GAS activation notifications"), F.ActivationNotifications, NotificationsBeforeHitReject);
	TestEqual(TEXT("Rejected normal skill preserves hit"), F.Fight->GetStateData().Handle, HitHandle);

	F.Configure(100, false, EWuwaSkillOverrideType::Hit);
	const FGameplayAbilitySpecHandle OverrideSpec = F.Grant();
	UWuwaGameplayAbilityBase* Override = F.Start(*this, OverrideSpec);
	if (!Override) return false;
	const int32 OverrideHandle = Override->GetSkillHandle();
	TestTrue(TEXT("Configured skill enters the hit-override category"),
		F.Fight->GetStateData().State == EWuwaFightState::SkillOverrideHit);
	TestEqual(TEXT("Overriding skill owns the fight state"), F.Fight->GetStateData().Handle, OverrideHandle);
	TestFalse(TEXT("Late hit exit cannot clear the overriding skill"), F.Fight->ExitState(HitHandle));
	F.Skills->EndSkill(FirstHandle);
	TestEqual(TEXT("Late skill exit cannot clear the overriding skill"), F.Skills->GetCurrentSkillData().FightStateHandle, OverrideHandle);
	F.ASC->CancelAbilityHandle(OverrideSpec);
	TestEqual(TEXT("Override end releases fight ownership"), F.Fight->GetStateData().Handle, 0);
	TestEqual(TEXT("Override end clears manager record"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	return true;
}

#endif
