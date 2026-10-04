#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "WuwaInputConditionTestTypes.h"
#include "WuwaSkillTestAbility.h"

namespace WuwaInputCommandTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaSkillBridgeComponent* Skills = nullptr;
		UWuwaCombatInputRuntimeBridge* Runtime = nullptr;
		UWuwaInputCommandConfig* Config = nullptr;
		UWuwaSkillTestAbility* Defaults = GetMutableDefault<UWuwaSkillTestAbility>();
		TGuardValue<bool> MainGuard{Defaults->bIsMainSkill, true};
		TGuardValue<int32> LevelGuard{Defaults->InterruptLevel, 100};
		TGuardValue<EWuwaSkillOverrideType> OverrideGuard{Defaults->SkillOverrideType, EWuwaSkillOverrideType::None};
		TGuardValue<EWuwaMoveState> MovementGuard{Defaults->StartMoveState, EWuwaMoveState::Other};
		int32 Activations = 0;
		const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("GAS.GA.Role.Attack1"));
		const FGameplayTag Skill1 = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Skill.Attack01"));
		const FGameplayTag Skill2 = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Skill.Attack02"));
		const FGameplayTag Skill3 = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Skill.Attack03"));
		const FGameplayTag MissingSkill = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Skill.Attack05"));
		const FGameplayTag OwnerTag = FGameplayTag::RequestGameplayTag(TEXT("WuwaASC.GE.SpeedUp"));
		const FGameplayTag UnknownInput = FGameplayTag::RequestGameplayTag(TEXT("Animation.Montage.Event"));

		~FFixture()
		{
			if (Runtime) Runtime->ResetInput();
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
			World->TimeSeconds = 1.0;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AWuwaCharacter>(Spawn);
			if (!Test.TestNotNull(TEXT("Character exists"), Character)
				|| !Test.TestTrue(TEXT("Real managed skill policy assembles"), Character->EnsureSkillSystem())) return false;
			Skills = Character->SkillComponent;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*) { ++Activations; });
			UClass* RuntimeClass = LoadClass<UWuwaCombatInputRuntimeBridge>(nullptr,
				TEXT("/Script/UnrealSharp.WuwaCombatInputRuntime_C"));
			if (!Test.TestNotNull(TEXT("Production managed input runtime loads"), RuntimeClass)) return false;
			Runtime = NewObject<UWuwaCombatInputRuntimeBridge>(Character, RuntimeClass);
			Config = NewObject<UWuwaInputCommandConfig>(Character);
			Character->InputCommandConfig = Config;
			return Test.TestNotNull(TEXT("Production input runtime exists"), Runtime)
				&& Test.TestEqual(TEXT("Input runtime uses the generated managed implementation"),
					Runtime->GetClass()->GetClass()->GetFName(), FName(TEXT("CSClass")));
		}

		FGameplayAbilitySpecHandle Grant(FGameplayTag Tag = {}) const
		{
			FGameplayAbilitySpec Spec(UWuwaSkillTestAbility::StaticClass(), 1);
			if (Tag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Tag);
			return ASC->GiveAbility(Spec);
		}

		UWuwaGameplayAbilityBase* Active(FGameplayAbilitySpecHandle Handle) const
		{
			if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
				for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
					if (auto* Ability = Cast<UWuwaGameplayAbilityBase>(Instance); IsValid(Ability) && Ability->IsActive()) return Ability;
			return nullptr;
		}

		UWuwaGameplayAbilityBase* Start(FAutomationTestBase& Test, FGameplayAbilitySpecHandle Handle) const
		{
			if (!Test.TestTrue(TEXT("Initial skill activates through real GAS"), ASC->TryActivateAbility(Handle, false))) return nullptr;
			UWuwaGameplayAbilityBase* Ability = Active(Handle);
			return Test.TestNotNull(TEXT("Initial skill retains an active execution"), Ability) ? Ability : nullptr;
		}

		bool EndCurrent(FAutomationTestBase& Test) const
		{
			const FWuwaSkillData Current = Skills->GetCurrentSkillData();
			return Test.TestNotNull(TEXT("There is a skill to end"), Current.ActiveAbility.Get())
				&& Test.TestTrue(TEXT("Current skill completes through production EndAbility"),
					Current.ActiveAbility->TryEndSkillExecution(Current.FightStateHandle));
		}

		FWuwaInputCommandRule Rule(FName Name, FGameplayTag Target) const
		{
			FWuwaInputCommandRule Result;
			Result.RuleName = Name;
			Result.InputTag = InputTag;
			Result.TargetAbilityTag = Target;
			return Result;
		}

		FWuwaInputEvent Input(FGameplayTag Tag, EWuwaInputPhase Phase = EWuwaInputPhase::Pressed) const
		{
			FWuwaInputEvent Result;
			Result.InputTag = Tag;
			Result.Phase = Phase;
			Result.Timestamp = World->GetTimeSeconds();
			return Result;
		}

		EWuwaCombatInputResult Press(float LifetimeSeconds = 1.f) const
		{
			return Runtime->ProcessInput(ASC, Input(InputTag), LifetimeSeconds);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandOrderedConditionsTest,
	"Wuwa.Combat.InputCommand.OrderedOwnerQueryAndConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandOrderedConditionsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	const auto Third = F.Grant(F.Skill3);
	auto* Pass = NewObject<UWuwaInputConditionTest>(F.Config);
	auto* Fail = NewObject<UWuwaInputConditionTest>(F.Config);
	auto* ContextCheck = NewObject<UWuwaInputConditionTest>(F.Config);
	Fail->bResult = false;
	ContextCheck->CheckContext = [&F](const FWuwaInputCommandContext& Context)
	{
		return Context.ASC == F.ASC && Context.Avatar == F.Character && Context.InputTag == F.InputTag
			&& Context.CurrentSkill.FightStateHandle == 0 && Context.CurrentSkill.ActiveAbility == nullptr;
	};
	FWuwaInputCommandRule Rejected = F.Rule(TEXT("FalseCondition"), F.Skill3);
	Rejected.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	Rejected.Conditions = {Pass, Fail};
	FWuwaInputCommandRule Selected = F.Rule(TEXT("OwnerWithValidContext"), F.Skill2);
	Selected.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	Selected.Conditions = {Pass, ContextCheck};
	F.Config->Rules = {Rejected, Selected, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("Empty-query fallback runs when conditional owner tag is absent"),
		F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Fallback chooses the explicitly configured first attack"), F.Active(First));
	TestEqual(TEXT("False owner queries do not invoke the extension conditions"), Pass->EvaluationCount, 0);
	if (!F.EndCurrent(*this)) return false;
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	TestTrue(TEXT("The first fully matching rule activates"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("AND conditions reject the earlier rule and choose the second attack"), F.Active(Second));
	TestNull(TEXT("A single false extension condition blocks that rule"), F.Active(Third));
	TestNull(TEXT("A later fallback does not override the first matching rule"), F.Active(First));
	TestEqual(TEXT("Both eligible rules evaluate their first AND condition"), Pass->EvaluationCount, 2);
	TestEqual(TEXT("A false custom condition was actually evaluated through reflection"), Fail->EvaluationCount, 1);
	TestEqual(TEXT("The selected rule receives the real ASC/avatar/current-skill/input context"), ContextCheck->EvaluationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandCurrentSkillTest,
	"Wuwa.Combat.InputCommand.ComboUsesCurrentSpecIdentityAndResetsAfterEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandCurrentSkillTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	const auto Third = F.Grant(F.Skill3);
	FWuwaInputCommandRule OneToTwo = F.Rule(TEXT("Attack01To02"), F.Skill2);
	OneToTwo.RequiredCurrentSkillTag = F.Skill1;
	FWuwaInputCommandRule TwoToThree = F.Rule(TEXT("Attack02To03"), F.Skill3);
	TwoToThree.RequiredCurrentSkillTag = F.Skill2;
	F.Config->Rules = {OneToTwo, TwoToThree, F.Rule(TEXT("StartAttack01"), F.Skill1)};
	TestTrue(TEXT("Initial attack input selects the first stage"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	UWuwaGameplayAbilityBase* Old = F.Active(First);
	if (!TestNotNull(TEXT("First stage is active"), Old)) return false;
	// All three grants deliberately share one native class. The current execution's
	// configuration field also lies: only its actual granted Spec identity is valid.
	Old->OriginalTag = F.Skill3;
	TestTrue(TEXT("Current skill identity comes from its granted Spec"), F.ASC->HasActiveSkillAbilityTag(Old, F.Skill1));
	TestFalse(TEXT("Current skill identity is not the edited instance OriginalTag"), F.ASC->HasActiveSkillAbilityTag(Old, F.Skill3));
	TestFalse(TEXT("Current skill matching does not widen to a parent tag"),
		F.ASC->HasActiveSkillAbilityTag(Old, FGameplayTag::RequestGameplayTag(TEXT("Abilities.Skill"))));
	TestTrue(TEXT("Attack before the first stage's window is buffered"), F.Press() == EWuwaCombatInputResult::Buffered);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("The first stage's breakpoint selects the second stage"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	UWuwaGameplayAbilityBase* Next = F.Active(Second);
	if (!TestNotNull(TEXT("Second stage is active"), Next)) return false;
	TestFalse(TEXT("The first stage ended through normal skill handoff"), Old->IsSkillExecutionActive());
	TestFalse(TEXT("An ended execution is no longer an active skill identity"), F.ASC->HasActiveSkillAbilityTag(Old, F.Skill1));
	TestTrue(TEXT("A new raw attack waits behind the second stage"), F.Press() == EWuwaCombatInputResult::Buffered);
	if (!F.EndCurrent(*this)) return false;
	TestTrue(TEXT("At normal skill end the same raw input is resolved again"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Without a current skill, the command starts attack01 again"), F.Active(First));
	TestNull(TEXT("No implicit continuation timer retains attack03 after attack02 ends"), F.Active(Third));
	TestEqual(TEXT("One input submission occurs at each of the three intended opportunities"), F.Activations, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandBufferedReevaluationTest,
	"Wuwa.Combat.InputCommand.BufferedRawInputReevaluatesOwnerState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandBufferedReevaluationTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	FWuwaInputCommandRule Derived = F.Rule(TEXT("OwnerDerivedAttack"), F.Skill2);
	Derived.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	F.Config->Rules = {Derived, F.Rule(TEXT("DefaultAttack"), F.Skill1)};
	TestTrue(TEXT("Closed-window input caches before the owner tag changes"), F.Press() == EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("One raw semantic input is cached"), F.Runtime->GetBufferedInputCount(), 1);
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("A condition change alone does not create an input opportunity"), F.Activations, 1);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("Breakpoint reevaluates the cached raw input against current owner tags"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The command chooses the newly eligible derived attack"), F.Active(Second));
	TestNull(TEXT("The originally resolved default Spec was not frozen into the buffer"), F.Active(First));
	TestEqual(TEXT("Submitting the selected command consumes the raw batch"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandNoMatchBufferTest,
	"Wuwa.Combat.InputCommand.ConfiguredNoMatchBuffersButNeverBypassesRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandNoMatchBufferTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const auto Direct = F.Grant(F.InputTag);
	const auto Derived = F.Grant(F.Skill2);
	FWuwaInputCommandRule Conditional = F.Rule(TEXT("OnlyWhenOwnerTagPresent"), F.Skill2);
	Conditional.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	F.Config->Rules = {Conditional};
	TestTrue(TEXT("Configured but unmatched input reports NoMatchingRule when caching is disabled"),
		F.Press(0.f) == EWuwaCombatInputResult::NoMatchingRule);
	TestEqual(TEXT("No-match with zero lifetime does not cache"), F.Runtime->GetBufferedInputCount(), 0);
	TestTrue(TEXT("A recognized input may cache before any rule becomes eligible"), F.Press() == EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("Recognized no-match retains the raw event"), F.Runtime->GetBufferedInputCount(), 1);
	TestTrue(TEXT("Completely unknown input reports no candidate"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.UnknownInput), 1.f) == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Unknown input does not enter the recognized pending batch"), F.Runtime->GetBufferedInputCount(), 1);
	for (const auto Phase : {EWuwaInputPhase::Released, EWuwaInputPhase::Triggered, EWuwaInputPhase::Canceled})
		TestTrue(TEXT("This stage ignores non-Pressed phases"),
			F.Runtime->ProcessInput(F.ASC, F.Input(F.InputTag, Phase), 1.f) == EWuwaCombatInputResult::Ignored);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestNull(TEXT("An open skill window cannot bypass unmatched configured conditions by direct-tag lookup"), F.Active(Direct));
	TestEqual(TEXT("No-match remains pending while its lifetime is valid"), F.Runtime->GetBufferedInputCount(), 1);
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	F.Skills->CallAnimBreakPoint(Old->GetSkillHandle());
	TestTrue(TEXT("A later explicit breakpoint can resolve the previously unmatched raw input"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The newly eligible configured target activates"), F.Active(Derived));
	TestNull(TEXT("Direct-tag bypass remains unused"), F.Active(Direct));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandFirstMatchFailureTest,
	"Wuwa.Combat.InputCommand.FirstMatchedTargetMissingOrAmbiguousDoesNotFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandFirstMatchFailureTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto Fallback = F.Grant(F.Skill1);
	const auto Direct = F.Grant(F.InputTag);
	F.Config->Rules = {F.Rule(TEXT("FirstMatchedTarget"), F.MissingSkill), F.Rule(TEXT("LaterFallback"), F.Skill1)};
	TestTrue(TEXT("First matched rule with an ungranted target reports NoCandidate"), F.Press() == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Missing selected target does not execute another rule or the old direct route"), F.Activations, 0);
	TestEqual(TEXT("Missing target is not misreported as condition-no-match input"), F.Runtime->GetBufferedInputCount(), 0);
	const auto Chosen = F.Grant(F.MissingSkill);
	const auto Duplicate = F.Grant(F.MissingSkill);
	TestTrue(TEXT("Duplicate exact target identities are explicitly ambiguous"), F.Press() == EWuwaCombatInputResult::Ambiguous);
	TestEqual(TEXT("Ambiguous selected target never falls through to another rule"), F.Activations, 0);
	TestEqual(TEXT("Ambiguous target does not silently enter the buffer"), F.Runtime->GetBufferedInputCount(), 0);
	F.ASC->ClearAbility(Duplicate);
	TestTrue(TEXT("Once exactly one selected target exists, it can activate"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The first matching target is the executed Spec"), F.Active(Chosen));
	TestNull(TEXT("Later fallback remains inactive"), F.Active(Fallback));
	TestNull(TEXT("Same-input-tag direct grant remains inactive"), F.Active(Direct));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandLegacyCompatibilityTest,
	"Wuwa.Combat.InputCommand.UnconfiguredInputKeepsLegacyDirectRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandLegacyCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FGameplayTag DashTag = FWuwaGameTags::Get().Abilities_Movement_Dash;
	const auto Dash = F.Grant(DashTag);
	const auto DirectAttack = F.Grant(F.InputTag);
	FWuwaInputCommandRule Conditional = F.Rule(TEXT("ConfiguredAttack"), F.Skill2);
	Conditional.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	F.Config->Rules = {Conditional};
	TestTrue(TEXT("A character with attack rules still resolves an unconfigured Dash tag directly"),
		F.Runtime->ProcessInput(F.ASC, F.Input(DashTag), 1.f) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Existing Dash semantics select the directly tagged grant"), F.Active(Dash));
	TestTrue(TEXT("The configured attack tag cannot use the same compatibility fallback"),
		F.Press(0.f) == EWuwaCombatInputResult::NoMatchingRule);
	TestNull(TEXT("Configured no-match does not execute its direct-tag grant"), F.Active(DirectAttack));
	if (!F.EndCurrent(*this)) return false;
	F.Character->InputCommandConfig = nullptr;
	TestTrue(TEXT("A character without a command config keeps the original direct mapping"),
		F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The original directly tagged attack still executes without config"), F.Active(DirectAttack));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandInvalidRuleTest,
	"Wuwa.Combat.InputCommand.InvalidRuleDoesNotFallThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandInvalidRuleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Grant(F.Skill1);
	F.Config->Rules = {F.Rule(TEXT("MissingTargetIdentity"), {}), F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("An empty selected target identity is a configuration error"), F.Press() == EWuwaCombatInputResult::InvalidRule);
	TestEqual(TEXT("Empty selected target cannot run the later fallback"), F.Activations, 0);
	FWuwaInputCommandRule MissingCondition = F.Rule(TEXT("NullConditionObject"), F.Skill1);
	MissingCondition.Conditions.Add(nullptr);
	F.Config->Rules = {MissingCondition, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("A null condition object is not interpreted as a false or absent condition"),
		F.Press() == EWuwaCombatInputResult::InvalidRule);
	TestEqual(TEXT("Invalid condition cannot run the later fallback"), F.Activations, 0);
	TestEqual(TEXT("Invalid configuration does not enter the no-match buffer"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandConditionReentryTest,
	"Wuwa.Combat.InputCommand.ConditionCallbacksCannotSubmitStaleContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandConditionReentryTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto Target = F.Grant(F.Skill2);
	auto* Condition = NewObject<UWuwaInputConditionTest>(F.Config);
	FWuwaInputCommandRule Rule = F.Rule(TEXT("InstrumentedCondition"), F.Skill2);
	Rule.Conditions = {Condition};
	F.Config->Rules = {Rule};
	// These callbacks intentionally violate the production pure-query contract to
	// verify that an extension mistake cannot submit an already invalidated command.
	Condition->CheckContext = [&F](const FWuwaInputCommandContext&)
	{
		F.Runtime->ResetInput();
		return true;
	};
	TestTrue(TEXT("Resetting input during a condition invalidates the immediate selection"), F.Press() == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("A reset callback cannot activate from the old input context"), F.Activations, 0);
	bool bReentryIgnored = false;
	Condition->CheckContext = [&F, &bReentryIgnored](const FWuwaInputCommandContext&)
	{
		bReentryIgnored = F.Press() == EWuwaCombatInputResult::Ignored;
		return true;
	};
	TestTrue(TEXT("Outer selection remains valid when recursive processing is refused"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestTrue(TEXT("The same runtime cannot recursively process input from a condition"), bReentryIgnored);
	TestEqual(TEXT("Recursive callbacks still produce exactly one activation"), F.Activations, 1);
	TestNotNull(TEXT("The outer selected target is active"), F.Active(Target));
	if (!F.EndCurrent(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	Condition->CheckContext = nullptr;
	TestTrue(TEXT("A valid pure condition may buffer behind a closed skill window"), F.Press() == EWuwaCombatInputResult::Buffered);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	Condition->CheckContext = [&F](const FWuwaInputCommandContext&)
	{
		F.Runtime->ClearBufferedInput(F.InputTag);
		return true;
	};
	TestTrue(TEXT("Clearing pending input inside a condition invalidates its pending snapshot"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("Cleared pending snapshot cannot cause a replacement"), F.Activations, 2);
	TestEqual(TEXT("The explicit clear remains effective after the callback"), F.Runtime->GetBufferedInputCount(), 0);
	const int32 OldHandle = Old->GetSkillHandle();
	Condition->CheckContext = [Old, OldHandle](const FWuwaInputCommandContext&)
	{
		Old->TryEndSkillExecution(OldHandle);
		return true;
	};
	TestTrue(TEXT("Ending the current skill inside a condition invalidates that context"), F.Press() == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("The stale selection cannot activate after its source skill ended"), F.Activations, 2);
	TestEqual(TEXT("The source skill's own cleanup still completes"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandManagedAttributeConditionTest,
	"Wuwa.Combat.InputCommand.ManagedAttributeConditionUsesCurrentASCValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandManagedAttributeConditionTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UClass* ConditionClass = LoadClass<UWuwaInputCondition>(nullptr,
		TEXT("/Script/UnrealSharp.InputCondition_AttributeMinimum_C"));
	if (!TestNotNull(TEXT("The real generated attribute condition class loads"), ConditionClass)) return false;
	TestEqual(TEXT("Attribute condition uses managed implementation"), ConditionClass->GetClass()->GetFName(), FName(TEXT("CSClass")));
	UWuwaInputCondition* Condition = NewObject<UWuwaInputCondition>(F.Config, ConditionClass);
	FStructProperty* AttributeProperty = FindFProperty<FStructProperty>(ConditionClass, TEXT("Attribute"));
	FFloatProperty* MinimumProperty = FindFProperty<FFloatProperty>(ConditionClass, TEXT("Minimum"));
	if (!TestNotNull(TEXT("Managed condition exposes the configured attribute"), AttributeProperty)
		|| !TestNotNull(TEXT("Managed condition exposes the configured minimum"), MinimumProperty)) return false;
	FGameplayAttribute* Attribute = AttributeProperty->ContainerPtrToValuePtr<FGameplayAttribute>(Condition);
	*Attribute = UWuwaAttributeSet::GetHealthAttribute();
	MinimumProperty->SetPropertyValue_InContainer(Condition, 50.f);
	FObjectPropertyBase* ASCProperty = FindFProperty<FObjectPropertyBase>(AWuwaCharactorBase::StaticClass(), TEXT("AbilitySystemComponent"));
	if (!TestNotNull(TEXT("Character exposes its native ASC owner property"), ASCProperty)) return false;
	ASCProperty->SetObjectPropertyValue_InContainer(F.Character, F.ASC);
	UWuwaAttributeSet* Attributes = NewObject<UWuwaAttributeSet>(F.Character);
	Attributes->InitHealth(40.f);
	Attributes->InitMaxHealth(100.f);
	F.ASC->AddAttributeSetSubobject(Attributes);
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	FWuwaInputCommandRule Conditional = F.Rule(TEXT("HealthAtLeast50"), F.Skill2);
	Conditional.Conditions = {Condition};
	F.Config->Rules = {Conditional, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("Below the attribute threshold, the fallback is selected"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The fallback skill is active below the threshold"), F.Active(First));
	TestNull(TEXT("The conditional skill is inactive below the threshold"), F.Active(Second));
	if (!F.EndCurrent(*this)) return false;
	F.ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(), 50.f);
	TestTrue(TEXT("At the exact threshold, the managed condition selects its target"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The same shared condition reads the new current ASC value"), F.Active(Second));
	if (!F.EndCurrent(*this)) return false;
	F.Config->Rules = {Conditional};
	*Attribute = FGameplayAttribute();
	TestTrue(TEXT("An invalid attribute never passes the managed condition"), F.Press(0.f) == EWuwaCombatInputResult::NoMatchingRule);
	TestEqual(TEXT("Invalid attribute configuration does not start another skill"), F.Activations, 2);
	return true;
}

#endif
