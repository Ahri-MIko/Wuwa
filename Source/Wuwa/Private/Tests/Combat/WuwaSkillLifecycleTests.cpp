#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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
		const bool OriginalMoveOverride = Defaults->bOverridesMoveState;
		const EWuwaMoveState OriginalMoveState = Defaults->ActionMoveState;
		const int32 OriginalMovePriority = Defaults->ActionMoveStatePriority;
		int32 ActivationNotifications = 0;
		FGameplayAbilitySpecHandle ReentryCandidate;
		bool bProbeOnNextEnd = false;
		bool bReentryAttempted = false;
		bool bReentryAccepted = false;
		bool bAcquireMoveOnNextActivation = false;
		TWeakObjectPtr<UObject> ActivationMoveSource;
		int32 ActivationMoveHandle = 0;

		~FFixture()
		{
			bProbeOnNextEnd = false;
			bAcquireMoveOnNextActivation = false;
			if (ASC)
			{
				ASC->CancelAllAbilities();
				ASC->ClearActorInfo();
			}
			if (World) World->DestroyWorld(false);
			Defaults->bIsMainSkill = OriginalMainSkill;
			Defaults->InterruptLevel = OriginalInterruptLevel;
			Defaults->SkillOverrideType = OriginalOverrideType;
			Defaults->bOverridesMoveState = OriginalMoveOverride;
			Defaults->ActionMoveState = OriginalMoveState;
			Defaults->ActionMoveStatePriority = OriginalMovePriority;
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
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*)
			{
				++ActivationNotifications;
				if (!bAcquireMoveOnNextActivation) return;
				bAcquireMoveOnNextActivation = false;
				// Inject a competing movement owner after CanActivate passed, but before
				// the native GA acquires its movement lease in PreActivate.
				ActivationMoveHandle = Character->UnifiedStateComponent->AcquireMoveState(
					ActivationMoveSource.Get(), EWuwaMoveState::Dodge, 1000);
			});
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

		void Configure(int32 InterruptLevel, bool bMoveOverride = false, int32 MovePriority = 100,
			EWuwaSkillOverrideType OverrideType = EWuwaSkillOverrideType::None)
		{
			Defaults->bIsMainSkill = true;
			Defaults->InterruptLevel = InterruptLevel;
			Defaults->SkillOverrideType = OverrideType;
			Defaults->bOverridesMoveState = bMoveOverride;
			Defaults->ActionMoveState = EWuwaMoveState::Dodge;
			Defaults->ActionMoveStatePriority = MovePriority;
		}

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
			const FString Context = FString::Printf(TEXT("Skill %s (level=%d, main=%d, move=%d, override=%d): "),
				*Handle.ToString(), Defaults->InterruptLevel, Defaults->bIsMainSkill,
				Defaults->bOverridesMoveState, static_cast<int32>(Defaults->SkillOverrideType));
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
	TestFalse(TEXT("Registering a skill does not implicitly acquire a movement lease"),
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

	First->SetSkillAcceptInput(true);
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
	{
		// An external lease replaces the prior lease permanently, so keep this case
		// separate from the real old-skill lease handoff tested below.
		FFixture External;
		if (!External.Initialize(*this)) return false;
		External.Configure(200, true, 300);
		const FGameplayAbilitySpecHandle OldSpec = External.Grant();
		UWuwaGameplayAbilityBase* Old = External.Start(*this, OldSpec);
		if (!Old) return false;
		const int32 OldHandle = Old->GetSkillHandle();
		Old->SetSkillReadyEnd(true);
		External.Configure(20, true, 10);
		const FGameplayAbilitySpecHandle NewSpec = External.Grant();
		UObject* MoveSource = NewObject<UInputAction>(External.Character);
		const int32 MoveHandle = External.Character->UnifiedStateComponent->AcquireMoveState(
			MoveSource, EWuwaMoveState::Dodge, 1000);
		if (!TestTrue(TEXT("Unrelated source acquires a higher-priority movement lease"), MoveHandle > 0)) return false;
		const int32 BeforeRejected = External.ActivationNotifications;
		const int32 MoveRevision = External.Character->UnifiedStateComponent->GetStateData().Revision;
		TestFalse(TEXT("Ready-end cannot bypass an unrelated source's movement lease"), External.ASC->TryActivateAbility(NewSpec, false));
		TestEqual(TEXT("Unrelated movement blocks activation before GAS notifications"), External.ActivationNotifications, BeforeRejected);
		TestEqual(TEXT("Failed unrelated-source preflight keeps the old skill active"), External.GetActiveAbility(OldSpec), Old);
		TestEqual(TEXT("Failed unrelated-source preflight preserves the old skill handle"), External.Skills->GetCurrentSkillData().FightStateHandle, OldHandle);
		TestEqual(TEXT("Failed preflight does not mutate the unrelated movement state"),
			External.Character->UnifiedStateComponent->GetStateData().Revision, MoveRevision);
		TestTrue(TEXT("The unrelated movement owner can still release its original handle"),
			External.Character->UnifiedStateComponent->ReleaseMoveState(MoveHandle));
		if (!External.Start(*this, NewSpec)) return false;
		TestNull(TEXT("Releasing the unrelated movement owner allows the pending transition"), External.GetActiveAbility(OldSpec));
	}
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Configure(200, true, 300);
	const FGameplayAbilitySpecHandle FirstSpec = F.Grant();
	UWuwaGameplayAbilityBase* First = F.Start(*this, FirstSpec);
	if (!First) return false;
	TestTrue(TEXT("First skill holds its configured movement lease"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	const int32 FirstHandle = First->GetSkillHandle();
	F.Configure(20, true, 10);
	const FGameplayAbilitySpecHandle LowerSpec = F.Grant();
	TestFalse(TEXT("Lower-priority skill is rejected before ready-end"), F.ASC->TryActivateAbility(LowerSpec, false));
	First->SetSkillAcceptInput(true);
	TestFalse(TEXT("Accept-input alone does not allow a lower-priority skill"), F.ASC->TryActivateAbility(LowerSpec, false));
	First->SetSkillReadyEnd(true);
	TestTrue(TEXT("Owning GA opens ready-end"), F.Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	UWuwaGameplayAbilityBase* Lower = F.Start(*this, LowerSpec);
	if (!Lower) return false;
	TestNull(TEXT("Ready-end replacement finishes the old GAS execution"), F.GetActiveAbility(FirstSpec));
	TestTrue(TEXT("Lower-priority skill receives a new fight handle"), Lower->GetSkillHandle() > FirstHandle);
	TestEqual(TEXT("New interrupt level is recorded"), F.Skills->GetCurrentSkillData().InterruptLevel, 20);
	TestTrue(TEXT("New skill acquires movement after the old higher-priority lease is released"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	TestTrue(TEXT("Movement remains in the requested action state"),
		F.Character->UnifiedStateComponent->GetStateData().MoveState == EWuwaMoveState::Dodge);
	F.Skills->EndSkill(FirstHandle);
	TestTrue(TEXT("Old skill cleanup leaves the new movement lease intact"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	F.ASC->CancelAbilityHandle(LowerSpec);
	TestEqual(TEXT("Finishing replacement clears skill state"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Finishing replacement clears fight state"), F.Fight->GetStateData().Handle, 0);
	TestFalse(TEXT("Finishing replacement releases movement lease"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	TestTrue(TEXT("No-input movement resumes standing"),
		F.Character->UnifiedStateComponent->GetStateData().MoveState == EWuwaMoveState::Stand);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillMovementRaceRollbackTest, "Wuwa.Combat.Skill.MovementRaceRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillMovementRaceRollbackTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillLifecycleTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Configure(100, true, 100);
	const FGameplayAbilitySpecHandle SpecHandle = F.Grant();
	F.ActivationMoveSource = NewObject<UInputAction>(F.Character);
	F.bAcquireMoveOnNextActivation = true;
	const int32 FightHandleBefore = F.Fight->GetStateData().Handle;
	// GAS reports that it dispatched activation even when the ability ends itself
	// during activation. Inspect the resulting execution and ownership as well.
	TestTrue(TEXT("GAS dispatches a request that passed pure preflight"), F.ASC->TryActivateAbility(SpecHandle, false));
	TestEqual(TEXT("The activation callback ran once"), F.ActivationNotifications, 1);
	if (!TestTrue(TEXT("A competing movement lease was installed during activation"), F.ActivationMoveHandle > 0)) return false;
	TestNull(TEXT("Failed movement acquisition ends the new GAS execution"), F.GetActiveAbility(SpecHandle));
	const FGameplayAbilitySpec* Spec = F.ASC->FindAbilitySpecFromHandle(SpecHandle);
	if (!TestNotNull(TEXT("The rejected execution leaves its granted spec present"), Spec)) return false;
	TestFalse(TEXT("The failed execution has no active spec count"), Spec->IsActive());
	TestEqual(TEXT("Failed movement acquisition rolls back the new skill record"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Failed movement acquisition releases the acquired fight state"), F.Fight->GetStateData().Handle, FightHandleBefore);
	TestTrue(TEXT("Skill rollback preserves the competing movement lease"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	TestTrue(TEXT("The competing movement owner still has its original valid handle"),
		F.Character->UnifiedStateComponent->ReleaseMoveState(F.ActivationMoveHandle));

	UWuwaGameplayAbilityBase* Retry = F.Start(*this, SpecHandle);
	if (!Retry) return false;
	TestTrue(TEXT("The rejected execution acquired and consumed a fight handle before rollback"), Retry->GetSkillHandle() > 1);
	TestTrue(TEXT("Retry succeeds after the competing movement owner releases"),
		F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
	F.ASC->CancelAbilityHandle(SpecHandle);
	TestEqual(TEXT("Retry ends with no current skill"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestEqual(TEXT("Retry ends with no fight state"), F.Fight->GetStateData().Handle, 0);
	TestFalse(TEXT("Retry releases its own movement lease"), F.Character->UnifiedStateComponent->GetStateData().bHasActionOverride);
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

	F.Configure(100, false, 100, EWuwaSkillOverrideType::Hit);
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
