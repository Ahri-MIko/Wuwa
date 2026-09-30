#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"

namespace WuwaMontageMovementHandoffTests
{
	// The verification project can read the original assets without copying or saving them.
	// Normal editor test runs use their existing /Game/ mount unchanged.
	struct FOptionalContentMount
	{
		FString ContentDirectory;

		FOptionalContentMount()
		{
			if (FParse::Value(FCommandLine::Get(), TEXT("KuroSourceContent="), ContentDirectory))
			{
				ContentDirectory = FPaths::ConvertRelativePathToFull(ContentDirectory);
				FPaths::NormalizeDirectoryName(ContentDirectory);
				ContentDirectory += TEXT("/");
				FPackageName::RegisterMountPoint(TEXT("/Game/"), ContentDirectory);
			}
		}

		~FOptionalContentMount()
		{
			if (!ContentDirectory.IsEmpty())
			{
				FPackageName::UnRegisterMountPoint(TEXT("/Game/"), ContentDirectory);
			}
		}
	};

	struct FFixture
	{
		FEditorScriptExecutionGuard ScriptExecutionGuard;
		FOptionalContentMount ContentMount;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		UAnimInstance* AnimInstance = nullptr;
		UAnimMontage* Montage = nullptr;
		UWuwaGameplayAbilityBase* Ability = nullptr;
		UWuwaGameplayAbilityBase* OtherAbility = nullptr;

		~FFixture()
		{
			// ActorInfo is owned by ASC; detach test ability contexts before destroying the world.
			const FGameplayAbilitySpec EmptySpec;
			if (Ability)
			{
				Ability->OnGiveAbility(nullptr, EmptySpec);
			}
			if (OtherAbility)
			{
				OtherAbility->OnGiveAbility(nullptr, EmptySpec);
			}
			if (AnimInstance)
			{
				AnimInstance->UninitializeAnimation();
			}
			if (ASC)
			{
				ASC->ClearActorInfo();
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Transient world exists"), World))
			{
				return false;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags |= RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AWuwaCharacter>(SpawnParameters);
			USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Characters/Role/changli/Model/Changli.Changli"));
			Montage = LoadObject<UAnimMontage>(nullptr,
				TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_F.AM_Changli_Dash_F"));
			if (!Test.TestNotNull(TEXT("Native character exists"), Character)
				|| !Test.TestNotNull(TEXT("Existing Changli mesh loads"), MeshAsset)
				|| !Test.TestNotNull(TEXT("Existing forward dash montage loads"), Montage))
			{
				return false;
			}

			USkeletalMeshComponent* Mesh = Character->GetMesh();
			Mesh->SetSkeletalMeshAsset(MeshAsset);
			// An empty native graph isolates montage extraction from Run_End and editor plugins.
			Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());
			Mesh->InitAnim(true);
			AnimInstance = Mesh->GetAnimInstance();
			if (!Test.TestNotNull(TEXT("Native animation instance is initialized"), AnimInstance))
			{
				return false;
			}
			AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
			ASC = NewObject<UAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			Ability = NewObject<UWuwaGameplayAbilityBase>(ASC);
			OtherAbility = NewObject<UWuwaGameplayAbilityBase>(ASC);
			const FGameplayAbilitySpec EmptySpec;
			Ability->OnGiveAbility(ASC->AbilityActorInfo.Get(), EmptySpec);
			OtherAbility->OnGiveAbility(ASC->AbilityActorInfo.Get(), EmptySpec);
			return Test.TestTrue(TEXT("Fixture montage contains root motion"), Montage->HasRootMotion())
				&& Test.TestEqual(TEXT("ASC uses the same animation instance"),
					ASC->AbilityActorInfo->GetAnimInstance(), AnimInstance);
		}

		FAnimMontageInstance* Play(FAutomationTestBase& Test)
		{
			const float Duration = ASC->PlayMontage(Ability, FGameplayAbilityActivationInfo(), Montage, 1.f);
			if (!Test.TestTrue(TEXT("ASC successfully starts the dash montage"), Duration > 0.f))
			{
				return nullptr;
			}
			FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
			if (Test.TestNotNull(TEXT("A live montage instance exists"), Instance))
			{
				// Complete the blend-in without advancing into the asset's cancel notify.
				Instance->UpdateWeight(Montage->BlendIn.GetBlendTime() + 0.01f);
				Instance->SetPosition(0.1f);
			}
			return Instance;
		}
	};

	struct FDashLifecycleFixture : FFixture
	{
		UClass* DashClass = nullptr;
		UAnimMontage* BackwardMontage = nullptr;
		FGameplayAbilitySpecHandle DashHandle;
		TWeakObjectPtr<UWuwaGameplayAbilityBase> LastDash;
		int32 LastMontageInstanceID = INDEX_NONE;

		~FDashLifecycleFixture()
		{
			// Finish real ability tasks before the base fixture tears down animation/ActorInfo.
			if (ASC) ASC->CancelAllAbilities();
		}

		bool InitializeDash(FAutomationTestBase& Test)
		{
			if (!Initialize(Test)) return false;
			DashClass = LoadClass<UWuwaGameplayAbilityBase>(nullptr,
				TEXT("/Game/Characters/Role/changli/GA/GA_Dash.GA_Dash_C"));
			BackwardMontage = LoadObject<UAnimMontage>(nullptr,
				TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B.AM_Changli_Dash_B"));
			if (!Test.TestNotNull(TEXT("Real Dash Blueprint loads"), DashClass)
				|| !Test.TestNotNull(TEXT("Backward Dash montage loads"), BackwardMontage)) return false;
			const UWuwaGameplayAbilityBase* DashDefaults = DashClass->GetDefaultObject<UWuwaGameplayAbilityBase>();
			if (!Test.TestTrue(TEXT("Migrated Dash explicitly owns Dodge state"),
				DashDefaults->bOverridesMoveState && DashDefaults->ActionMoveState == EWuwaMoveState::Dodge)) return false;

			Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!Test.TestTrue(TEXT("Managed movement state is available"), Character->EnsureMovementStateSystem())) return false;
			if (!Test.TestTrue(TEXT("Managed skill system is available"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RequestDesiredGait(EWuwaGait::Run);
			Character->RoleGaitComponent->RefreshPolicy();
			// Authority GAS can activate without a local player. Queued movement-cancel/sprint
			// notifies deliberately skip this pawn, isolating the real montage task's end callbacks.
			if (!Test.TestTrue(TEXT("Authority ActorInfo can activate Dash locally"), ASC->AbilityActorInfo->IsLocallyControlled())) return false;
			DashHandle = ASC->GiveAbility(FGameplayAbilitySpec(DashClass, 1));
			return Test.TestTrue(TEXT("Real Dash ability is granted"), DashHandle.IsValid());
		}

		bool StartDash(FAutomationTestBase& Test, const FVector2D Input)
		{
			Character->HandleMoveInput(FInputActionValue(Input));
			Character->GetWuwaMovementComponent()->ConsumeInputVector();
			if (!Test.TestTrue(TEXT("Real Dash Blueprint activates"), ASC->TryActivateAbility(DashHandle, false))) return false;
			UWuwaGameplayAbilityBase* ActiveDash = Cast<UWuwaGameplayAbilityBase>(ASC->GetAnimatingAbility());
			if (!Test.TestNotNull(TEXT("Dash montage task owns the ASC playback"), ActiveDash)) return false;
			LastDash = ActiveDash;
			UAnimMontage* ExpectedMontage = Input.IsNearlyZero() ? BackwardMontage : Montage;
			bool bValid = Test.TestEqual(TEXT("Executing ability is the real Dash Blueprint"), ActiveDash->GetClass(), DashClass);
			bValid &= Test.TestTrue(TEXT("Dash GA remains active while its montage runs"), ActiveDash->IsActive());
			bValid &= Test.TestEqual(TEXT("Blueprint selects forward/backward montage from input"), ActiveDash->GetCurrentMontage(), ExpectedMontage);
			const FWuwaUnifiedStateData State = Character->UnifiedStateComponent->GetStateData();
			bValid &= Test.TestTrue(TEXT("Active Dash owns the Dodge state"), State.bHasActionOverride && State.MoveState == EWuwaMoveState::Dodge);
			FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(ExpectedMontage);
			if (!Test.TestNotNull(TEXT("Dash has a live montage playback instance"), Instance)) return false;
			LastMontageInstanceID = Instance->GetInstanceID();
			// Complete weight setup without advancing into any notify window.
			Instance->UpdateWeight(ExpectedMontage->BlendIn.GetBlendTime() + 0.01f);
			return bValid;
		}

		bool IsDashActive() const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(DashHandle);
			return Spec && Spec->IsActive();
		}

		bool CheckFinished(FAutomationTestBase& Test, const TCHAR* Reason, EWuwaMoveState ExpectedMove) const
		{
			const FString Prefix = FString(Reason) + TEXT(": ");
			bool bValid = Test.TestFalse(Prefix + TEXT("GAS spec is inactive"), IsDashActive());
			bValid &= Test.TestTrue(Prefix + TEXT("execution instance ended"), !LastDash.IsValid() || !LastDash->IsActive());
			const FWuwaUnifiedStateData State = Character->UnifiedStateComponent->GetStateData();
			bValid &= Test.TestFalse(Prefix + TEXT("action lease released"), State.bHasActionOverride);
			bValid &= Test.TestTrue(Prefix + TEXT("gait resumes from current input"), State.MoveState == ExpectedMove);
			bValid &= Test.TestNull(Prefix + TEXT("old Dash no longer owns ASC animation"), ASC->GetAnimatingAbility());
			return bValid;
		}

		void AdvanceLastMontage(float DeltaSeconds)
		{
			World->TimeSeconds += DeltaSeconds;
			AnimInstance->ClearQueuedAnimEvents(false);
			if (FAnimMontageInstance* Instance = AnimInstance->GetMontageInstanceForID(LastMontageInstanceID))
			{
				Instance->UpdateWeight(DeltaSeconds);
				FRootMotionMovementParams IgnoredRootMotion;
				Instance->Advance(DeltaSeconds, &IgnoredRootMotion, false);
				// Advance/dispatch may end the GA and delete this playback. Do not reuse Instance.
			}
			AnimInstance->DispatchQueuedAnimEvents();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMontageMovementHandoffTest,
	"Wuwa.Ability.MontageMovementHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaMontageMovementHandoffTest::RunTest(const FString& Parameters)
{
	UWuwaGameplayAbilityBase* UnboundAbility = NewObject<UWuwaGameplayAbilityBase>();
	TestFalse(TEXT("An ability without ActorInfo rejects the handoff safely"),
		UnboundAbility->StopMontageForMovement());

	WuwaMontageMovementHandoffTests::FFixture Fixture;
	if (!Fixture.Initialize(*this))
	{
		return false;
	}
	FAnimMontageInstance* Instance = Fixture.Play(*this);
	if (!Instance)
	{
		return false;
	}
	const int32 OriginalInstanceID = Instance->GetInstanceID();
	UWuwaMovementComponent* Movement = Fixture.Character->GetWuwaMovementComponent();
	const FVector InitialVelocity(500.f, 0.f, 0.f);
	Movement->Velocity = InitialVelocity;

	FRootMotionMovementParams BeforeHandoff;
	// Direct unblended extraction avoids needing a Slot node in this native test graph.
	Instance->Advance(1.f / 60.f, &BeforeHandoff, false);
	TestTrue(TEXT("The playing dash extracts nonzero displacement before handoff"),
		BeforeHandoff.bHasRootMotion && !BeforeHandoff.GetRootMotionTransform().GetTranslation().IsNearlyZero());
	TestFalse(TEXT("No movement input leaves the montage playing"),
		Fixture.Ability->StopMontageForMovement());
	TestFalse(TEXT("Rejected handoff does not disable root motion"), Instance->IsRootMotionDisabled());
	TestFalse(TEXT("Rejected handoff does not start blend-out"), Instance->IsStopped());
	TestEqual(TEXT("Rejected handoff preserves velocity"), Movement->Velocity, InitialVelocity);

	Fixture.Character->HandleMoveInput(FInputActionValue(FVector2D(0.f, 1.f)));
	// Even a stale matching montage asset on another GA does not confer ownership.
	Fixture.OtherAbility->SetCurrentMontage(Fixture.Montage);
	TestFalse(TEXT("Another ability cannot stop the owner's montage"),
		Fixture.OtherAbility->StopMontageForMovement());
	TestFalse(TEXT("Wrong-owner attempt leaves root motion enabled"), Instance->IsRootMotionDisabled());
	TestFalse(TEXT("Negative blend duration is rejected without stopping"),
		Fixture.Ability->StopMontageForMovement(-0.1f));
	TestFalse(TEXT("Invalid blend duration leaves the montage active"), Instance->IsStopped());

	if (!TestTrue(TEXT("Held movement allows the owning GA to hand off its montage"),
		Fixture.Ability->StopMontageForMovement(0.1f)))
	{
		return false;
	}
	Instance = Fixture.AnimInstance->GetMontageInstanceForID(OriginalInstanceID);
	if (!TestNotNull(TEXT("The original instance remains alive for visual blend-out"), Instance))
	{
		return false;
	}
	TestTrue(TEXT("Only the outgoing instance has root motion disabled"), Instance->IsRootMotionDisabled());
	TestTrue(TEXT("The outgoing instance is blending out"), Instance->IsStopped());
	TestTrue(TEXT("Pose weight is still positive after handoff"), Instance->GetWeight() > 0.f);
	TestTrue(TEXT("Root Motion from Everything remains enabled for graph animations such as Run_End"),
		Fixture.AnimInstance->RootMotionMode == ERootMotionMode::RootMotionFromEverything);
	TestEqual(TEXT("Handoff neither zeros nor forces character velocity"), Movement->Velocity, InitialVelocity);
	TestFalse(TEXT("Repeating handoff does not operate on a fading instance"),
		Fixture.Ability->StopMontageForMovement(0.1f));

	Instance->UpdateWeight(0.02f);
	FRootMotionMovementParams DuringFade;
	Instance->Advance(0.02f, &DuringFade, false);
	TestFalse(TEXT("Visual fade does not continue contributing dash root motion"), DuringFade.bHasRootMotion);
	TestTrue(TEXT("The pose can still fade while its root motion is disabled"), Instance->GetWeight() > 0.f);
	Instance->UpdateWeight(0.2f);
	FRootMotionMovementParams FinalFadeFrame;
	Instance->Advance(0.02f, &FinalFadeFrame, false);
	// Advance may terminate the old instance: no further dereference of Instance.
	TestFalse(TEXT("The final fade frame does not emit even an identity root-motion sample"),
		FinalFadeFrame.bHasRootMotion);

	FAnimMontageInstance* NewInstance = Fixture.Play(*this);
	if (!NewInstance)
	{
		return false;
	}
	TestNotEqual(TEXT("Replaying the same asset creates a distinct playback instance"),
		NewInstance->GetInstanceID(), OriginalInstanceID);
	TestFalse(TEXT("The next dash starts with root motion enabled"), NewInstance->IsRootMotionDisabled());
	FRootMotionMovementParams ReplayedMotion;
	NewInstance->Advance(1.f / 60.f, &ReplayedMotion, false);
	TestTrue(TEXT("The replay still extracts actual dash displacement"),
		ReplayedMotion.bHasRootMotion && !ReplayedMotion.GetRootMotionTransform().GetTranslation().IsNearlyZero());
	TestTrue(TEXT("The shared montage asset retains root motion"), Fixture.Montage->HasRootMotion());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaDashStateLifecycleTest,
	"Wuwa.Ability.DashStateLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaDashStateLifecycleTest::RunTest(const FString& Parameters)
{
	WuwaMontageMovementHandoffTests::FDashLifecycleFixture Fixture;
	// Requires the asset migration: both montage task OnInterrupted/OnCancelled outputs
	// must reach the existing EndAbility node, and the Dash CDO must opt into Dodge ownership.
	if (!Fixture.InitializeDash(*this)) return false;
	UWuwaMovementComponent* Movement = Fixture.Character->GetWuwaMovementComponent();
	Movement->Velocity = FVector(-310.f, 0.f, 0.f);
	if (!Fixture.StartDash(*this, FVector2D::ZeroVector)) return false;
	const int32 FirstPlaybackID = Fixture.LastMontageInstanceID;
	Fixture.AnimInstance->Montage_Stop(0.05f, Fixture.BackwardMontage);
	Fixture.AnimInstance->DispatchQueuedAnimEvents();
	if (!Fixture.CheckFinished(*this, TEXT("Direct montage stop"), EWuwaMoveState::Stand)) return false;
	TestEqual(TEXT("Ending a no-input Dash leaves physical velocity untouched"), Movement->Velocity, FVector(-310.f, 0.f, 0.f));

	if (!Fixture.StartDash(*this, FVector2D(0.f, 1.f))) return false;
	TestNotEqual(TEXT("Next Dash gets a new playback after interruption"), Fixture.LastMontageInstanceID, FirstPlaybackID);
	// Replacement is deliberately outside GAS. The old Dash's end callback must leave it playing.
	const float ReplacementDuration = Fixture.AnimInstance->Montage_Play(Fixture.BackwardMontage, 1.f);
	Fixture.AnimInstance->DispatchQueuedAnimEvents();
	if (!TestTrue(TEXT("A replacement montage starts"), ReplacementDuration > 0.f)
		|| !Fixture.CheckFinished(*this, TEXT("Montage replacement"), EWuwaMoveState::Run)) return false;
	FAnimMontageInstance* Replacement = Fixture.AnimInstance->GetActiveInstanceForMontage(Fixture.BackwardMontage);
	if (!TestNotNull(TEXT("Old Dash cleanup preserves the replacement playback"), Replacement)) return false;
	TestFalse(TEXT("The replacement was not stopped by the old Dash"), Replacement->IsStopped());
	Fixture.AnimInstance->Montage_Stop(0.f, Fixture.BackwardMontage);
	Fixture.AnimInstance->DispatchQueuedAnimEvents();

	if (!Fixture.StartDash(*this, FVector2D(0.f, 1.f))) return false;
	Fixture.ASC->CancelAbilityHandle(Fixture.DashHandle);
	Fixture.AnimInstance->DispatchQueuedAnimEvents();
	if (!Fixture.CheckFinished(*this, TEXT("GAS cancellation"), EWuwaMoveState::Run)) return false;

	if (!Fixture.StartDash(*this, FVector2D::ZeroVector)) return false;
	const int32 MaxFrames = FMath::CeilToInt((Fixture.BackwardMontage->GetPlayLength()
		+ Fixture.BackwardMontage->BlendOut.GetBlendTime() + 1.f) * 60.f);
	for (int32 Frame = 0; Frame < MaxFrames && Fixture.IsDashActive(); ++Frame)
	{
		Fixture.AdvanceLastMontage(1.f / 60.f);
	}
	if (!Fixture.CheckFinished(*this, TEXT("Natural montage completion"), EWuwaMoveState::Stand)) return false;
	TestTrue(TEXT("Dash can activate again after natural completion"), Fixture.StartDash(*this, FVector2D::ZeroVector));
	Fixture.ASC->CancelAbilityHandle(Fixture.DashHandle);
	Fixture.AnimInstance->DispatchQueuedAnimEvents();
	Fixture.CheckFinished(*this, TEXT("Final cleanup"), EWuwaMoveState::Stand);
	return true;
}

#endif
