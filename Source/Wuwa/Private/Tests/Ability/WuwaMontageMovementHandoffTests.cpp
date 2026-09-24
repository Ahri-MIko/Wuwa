#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
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

#endif
