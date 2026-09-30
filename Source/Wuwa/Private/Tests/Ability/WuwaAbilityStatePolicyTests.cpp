#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaAbilityStatePolicyTests
{
	// Use the generated managed policy through ProcessEvent, with no asset or mock policy dependency.
	struct FFixture
	{
		// Headless worlds do not enter PIE; allow the real managed component's native calls.
		FEditorScriptExecutionGuard ScriptExecutionGuard;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaUnifiedStateBridgeComponent* State = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		FGameplayAbilitySpecHandle AbilityHandle;
		int32 ActivationNotifications = 0;

		~FFixture()
		{
			if (ASC)
			{
				ASC->CancelAllAbilities();
				ASC->ClearActorInfo();
			}
			if (World) World->DestroyWorld(false);
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
			if (!Test.TestTrue(TEXT("Managed state system assembles"), Character->EnsureMovementStateSystem())) return false;
			if (!Test.TestTrue(TEXT("Managed skill system assembles before activation queries"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RefreshPolicy();
			State = Character->UnifiedStateComponent;
			if (!Test.TestNotNull(TEXT("State bridge exists"), State)
				|| !Test.TestEqual(TEXT("State uses the real UnrealSharp generated class"),
					State->GetClass()->GetClass()->GetFName(), FName(TEXT("CSClass")))) return false;

			ASC = NewObject<UAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*) { ++ActivationNotifications; });
			AbilityHandle = ASC->GiveAbility(FGameplayAbilitySpec(UWuwaGameplayAbilityBase::StaticClass(), 1));
			return Test.TestTrue(TEXT("Native test ability is granted"), AbilityHandle.IsValid());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaActionStatePreflightTest, "Wuwa.Ability.StatePolicy.PurePreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaActionStatePreflightTest::RunTest(const FString& Parameters)
{
	WuwaAbilityStatePolicyTests::FFixture F;
	if (!F.Initialize(*this)) return false;
	using M = EWuwaMoveState;
	UObject* FirstOwner = NewObject<UInputAction>(F.Character);
	UObject* NextOwner = NewObject<UInputAction>(F.Character);
	const int32 FirstHandle = F.State->AcquireMoveState(FirstOwner, M::Dodge, 200);
	if (!TestTrue(TEXT("High priority action is active"), FirstHandle > 0)) return false;
	const int32 Revision = F.State->GetStateData().Revision;

	TestFalse(TEXT("Lower priority preflight is rejected"), F.State->CanAcquireMoveState(M::Dodge, 100));
	TestTrue(TEXT("Equal priority may replace the action"), F.State->CanAcquireMoveState(M::Dodge, 200));
	TestFalse(TEXT("Even higher priority cannot request an illegal ground state"), F.State->CanAcquireMoveState(M::Fall, 300));
	TestFalse(TEXT("Unknown movement fails closed"), F.State->CanAcquireMoveState(static_cast<M>(255), 300));
	TestEqual(TEXT("All preflight queries leave the state revision unchanged"), F.State->GetStateData().Revision, Revision);
	TestTrue(TEXT("Preflight does not replace the current lease"), F.State->ReleaseMoveState(FirstHandle));

	TestTrue(TEXT("An unoccupied state passes preflight"), F.State->CanAcquireMoveState(M::Dodge, 100));
	const int32 RacingHandle = F.State->AcquireMoveState(FirstOwner, M::Dodge, 200);
	TestEqual(TEXT("Acquire rechecks priority after a successful earlier preflight"), F.State->AcquireMoveState(NextOwner, M::Dodge, 100), 0);
	TestTrue(TEXT("The intervening action keeps its lease"), F.State->ReleaseMoveState(RacingHandle));

	const int32 DeadHandle = F.State->AcquireMoveState(FirstOwner, M::Dodge, 200);
	FirstOwner->MarkAsGarbage();
	const int32 BeforeDeadOwnerQuery = F.State->GetStateData().Revision;
	TestTrue(TEXT("An invalid source cannot block a lower priority request"), F.State->CanAcquireMoveState(M::Dodge, 100));
	TestEqual(TEXT("Querying an invalid source does not prune or publish"), F.State->GetStateData().Revision, BeforeDeadOwnerQuery);
	TestTrue(TEXT("Pure query leaves the previously committed snapshot intact"), F.State->GetStateData().bHasActionOverride);
	const int32 NewHandle = F.State->AcquireMoveState(NextOwner, M::Dodge, 100);
	TestTrue(TEXT("Actual acquisition prunes the dead source and receives a new handle"), NewHandle > DeadHandle);
	TestFalse(TEXT("Pruned old handle cannot release the replacement"), F.State->ReleaseMoveState(DeadHandle));
	TestTrue(TEXT("Replacement can release normally"), F.State->ReleaseMoveState(NewHandle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAbilityStatePreflightTest, "Wuwa.Ability.StatePolicy.GASRejectsBeforeActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAbilityStatePreflightTest::RunTest(const FString& Parameters)
{
	WuwaAbilityStatePolicyTests::FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* CDO = GetMutableDefault<UWuwaGameplayAbilityBase>();
	// Restore native defaults on every return; no asset is loaded or modified.
	TGuardValue<bool> OverrideGuard(CDO->bOverridesMoveState, true);
	TGuardValue<EWuwaMoveState> MoveGuard(CDO->ActionMoveState, EWuwaMoveState::Dodge);
	TGuardValue<int32> PriorityGuard(CDO->ActionMoveStatePriority, 100);
	if (!TestTrue(TEXT("The native ability supports instance-owned handles"),
		CDO->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)) return false;
	const FGameplayAbilityActorInfo* ActorInfo = F.ASC->AbilityActorInfo.Get();
	TestTrue(TEXT("CDO activation query accepts an available action"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	TestFalse(TEXT("A missing Avatar is rejected without consulting a stale instance context"),
		CDO->CanActivateAbility(F.AbilityHandle, nullptr));

	UObject* ExistingOwner = NewObject<UInputAction>(F.Character);
	const int32 ExistingHandle = F.State->AcquireMoveState(ExistingOwner, EWuwaMoveState::Dodge, 200);
	if (!TestTrue(TEXT("Competing action holds the state"), ExistingHandle > 0)) return false;
	const int32 Revision = F.State->GetStateData().Revision;
	const FGameplayTagContainer OwnedTags = F.ASC->GetOwnedGameplayTags();
	TestFalse(TEXT("CDO activation query rejects lower priority on the provided Avatar"),
		CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	TestFalse(TEXT("The actual GAS request is rejected before activation"), F.ASC->TryActivateAbility(F.AbilityHandle, false));
	TestEqual(TEXT("Rejected request emits no GAS activation notification"), F.ActivationNotifications, 0);
	TestEqual(TEXT("Rejected request does not publish movement state"), F.State->GetStateData().Revision, Revision);
	TestTrue(TEXT("Rejected request does not change owned gameplay tags"), F.ASC->GetOwnedGameplayTags() == OwnedTags);
	const FGameplayAbilitySpec* Spec = F.ASC->FindAbilitySpecFromHandle(F.AbilityHandle);
	if (!TestNotNull(TEXT("Rejected ability spec remains present"), Spec)) return false;
	TestFalse(TEXT("Rejected spec is not active"), Spec->IsActive());
	TestEqual(TEXT("No execution instance is created for the rejected action"), Spec->GetAbilityInstances().Num(), 0);
	TestTrue(TEXT("The original action remains the valid owner"), F.State->ReleaseMoveState(ExistingHandle));

	CDO->ActionMoveState = EWuwaMoveState::Fall;
	TestFalse(TEXT("CDO activation query also rejects an illegal movement state"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	CDO->bOverridesMoveState = false;
	TestTrue(TEXT("Ordinary abilities retain the original GAS activation rules"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	return true;
}

#endif
