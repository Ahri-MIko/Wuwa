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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaActionStateLegalityTest, "Wuwa.Ability.StatePolicy.ActionStateLegality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaActionStateLegalityTest::RunTest(const FString& Parameters)
{
	WuwaAbilityStatePolicyTests::FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* CDO = GetMutableDefault<UWuwaGameplayAbilityBase>();
	// Restore native defaults on every return; no asset is loaded or modified.
	TGuardValue<bool> MainGuard(CDO->bIsMainSkill, true);
	TGuardValue<EWuwaMoveState> MoveGuard(CDO->StartMoveState, EWuwaMoveState::Dodge);
	const FGameplayAbilityActorInfo* ActorInfo = F.ASC->AbilityActorInfo.Get();
	TestTrue(TEXT("A legal action state passes the activation query"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	TestFalse(TEXT("A missing Avatar is rejected without consulting a stale instance context"),
		CDO->CanActivateAbility(F.AbilityHandle, nullptr));

	// As in the original, a written action state is not an occupation: it neither blocks nor ranks later actions.
	if (!TestTrue(TEXT("Another action wrote Dodge"), F.State->SetMoveState(EWuwaMoveState::Dodge, EWuwaGait::Run))) return false;
	const int32 Revision = F.State->GetStateData().Revision;
	TestTrue(TEXT("An existing action state does not block another action"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	CDO->StartMoveState = EWuwaMoveState::Fall;
	TestFalse(TEXT("An action state that is illegal in the current position is rejected"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	CDO->StartMoveState = static_cast<EWuwaMoveState>(255);
	TestFalse(TEXT("An unknown action state fails closed"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	TestEqual(TEXT("Activation queries never publish movement state"), F.State->GetStateData().Revision, Revision);
	CDO->StartMoveState = EWuwaMoveState::Other;
	TestTrue(TEXT("Ordinary abilities retain the original GAS activation rules"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	CDO->bIsMainSkill = false;
	TestTrue(TEXT("A non-main ability is not gated by the main-skill manager"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
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
	TGuardValue<EWuwaMoveState> MoveGuard(CDO->StartMoveState, EWuwaMoveState::Fall);
	if (!TestTrue(TEXT("The native ability writes its action state from an instance"),
		CDO->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)) return false;
	const FGameplayAbilityActorInfo* ActorInfo = F.ASC->AbilityActorInfo.Get();
	const int32 Revision = F.State->GetStateData().Revision;
	const FGameplayTagContainer OwnedTags = F.ASC->GetOwnedGameplayTags();
	TestFalse(TEXT("CDO activation query rejects an action state that is illegal on the ground"),
		CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	TestFalse(TEXT("The actual GAS request is rejected before activation"), F.ASC->TryActivateAbility(F.AbilityHandle, false));
	TestEqual(TEXT("Rejected request emits no GAS activation notification"), F.ActivationNotifications, 0);
	TestEqual(TEXT("Rejected request does not publish movement state"), F.State->GetStateData().Revision, Revision);
	TestTrue(TEXT("Rejected request does not change owned gameplay tags"), F.ASC->GetOwnedGameplayTags() == OwnedTags);
	const FGameplayAbilitySpec* Spec = F.ASC->FindAbilitySpecFromHandle(F.AbilityHandle);
	if (!TestNotNull(TEXT("Rejected ability spec remains present"), Spec)) return false;
	TestFalse(TEXT("Rejected spec is not active"), Spec->IsActive());
	TestEqual(TEXT("No execution instance is created for the rejected action"), Spec->GetAbilityInstances().Num(), 0);

	CDO->StartMoveState = EWuwaMoveState::Dodge;
	TestTrue(TEXT("The same ability is accepted once its action state is legal"), CDO->CanActivateAbility(F.AbilityHandle, ActorInfo));
	return true;
}

#endif
