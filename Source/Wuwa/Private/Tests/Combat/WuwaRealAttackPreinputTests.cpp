#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "WuwaSkillNotifyTestTypes.h"

namespace WuwaRealAttackPreinputTests
{
	struct FFixture
	{
		FAutomationTestBase& Test;
		FString Scenario;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		AWuwaPlayerController* Controller = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaAbilityInputHandlerComponent* Handler = nullptr;
		UWuwaCombatInputRuntimeBridge* Runtime = nullptr;
		UAnimInstance* Anim = nullptr;
		UInputAction* AttackAction = nullptr;
		TArray<UClass*> AttackClasses;
		TArray<UAnimMontage*> AttackMontages;
		UClass* DashClass = nullptr;
		UAnimMontage* DashBackward = nullptr;
		UAnimMontage* SourceMontage = nullptr;
		int32 SourcePlaybackId = INDEX_NONE;
		int32 SourceSkillHandle = 0;
		int32 Activations = 0;
		int32 AbilityEnds = 0;
		float FrameStepSeconds = 1.f / 120.f;
		bool bObservedSourceReadyEnd = false;
		bool bUseRealAnimGraph = false;
		FDelegateHandle ActivationDelegate;
		FDelegateHandle EndDelegate;
		TArray<TSharedPtr<FJsonValue>> Frames;
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();

		FFixture(FAutomationTestBase& InTest, const TCHAR* InScenario, bool bInUseRealAnimGraph = false)
			: Test(InTest), Scenario(InScenario), bUseRealAnimGraph(bInUseRealAnimGraph)
		{
			Report->SetStringField(TEXT("scenario"), Scenario);
			Report->SetStringField(TEXT("auditUtc"), FDateTime::UtcNow().ToIso8601());
			Report->SetStringField(TEXT("scope"), TEXT("Real authored GA/command config/montage/notifies; transient native character and real input handler. No asset or CDO modification. Animation and input advance explicitly, without unrelated world systems."));
			Report->SetBoolField(TEXT("realAnimationGraph"), bUseRealAnimGraph);
		}

		~FFixture()
		{
			Capture(TEXT("FinalBeforeCleanup"));
			Report->SetBoolField(TEXT("observedSourceReadyEnd"), bObservedSourceReadyEnd);
			Report->SetNumberField(TEXT("activationCount"), Activations);
			Report->SetNumberField(TEXT("abilityEndCount"), AbilityEnds);
			Report->SetNumberField(TEXT("frameStepSeconds"), FrameStepSeconds);
			Report->SetArrayField(TEXT("frames"), Frames);
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/AttackPreinputRepro"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Path = Directory / (Scenario + TEXT(".json"));
			FString Json;
			const auto Writer = TJsonWriterFactory<>::Create(&Json);
			Test.TestTrue(TEXT("Real-asset reproduction writes its report even after a failed assertion"),
				FJsonSerializer::Serialize(Report, Writer) && FFileHelper::SaveStringToFile(Json, *Path));
			Test.AddInfo(FString::Printf(TEXT("Real-asset report: %s; activations=%d; source ReadyEnd observed=%s"),
				*Path, Activations, bObservedSourceReadyEnd ? TEXT("true") : TEXT("false")));
			if (Controller) Controller->SetPawn(nullptr);
			if (ASC)
			{
				ASC->AbilityActivatedCallbacks.Remove(ActivationDelegate);
				ASC->AbilityEndedCallbacks.Remove(EndDelegate);
				ASC->CancelAllAbilities();
			}
			if (Anim) Anim->UninitializeAnimation();
			if (ASC) ASC->ClearActorInfo();
			if (World) World->DestroyWorld(false);
		}

		void Capture(const TCHAR* Stage, const FString& InputResult = FString(), UGameplayAbility* Incoming = nullptr)
		{
			TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
			Frame->SetStringField(TEXT("stage"), Stage);
			Frame->SetNumberField(TEXT("worldSeconds"), World ? World->GetTimeSeconds() : -1.0);
			Frame->SetNumberField(TEXT("activations"), Activations);
			Frame->SetNumberField(TEXT("abilityEnds"), AbilityEnds);
			Frame->SetStringField(TEXT("inputResult"), InputResult);
			Frame->SetStringField(TEXT("incomingAbilityClass"), Incoming ? Incoming->GetClass()->GetPathName() : TEXT(""));
			Frame->SetNumberField(TEXT("bufferCount"), Runtime ? Runtime->GetBufferedInputCount() : -1);
			if (ASC)
			{
				Frame->SetStringField(TEXT("ascMontage"), GetPathNameSafe(ASC->GetCurrentMontage()));
				Frame->SetStringField(TEXT("ascAnimatingAbility"), GetPathNameSafe(ASC->GetAnimatingAbility()));
			}
			if (Character && Character->SkillComponent)
			{
				const FWuwaPlayerInputState InputState = Character->GetPlayerInputState();
				Frame->SetBoolField(TEXT("hasMoveInput"), InputState.bHasMoveInput);
				Frame->SetNumberField(TEXT("moveInputX"), InputState.MoveAxis.X);
				Frame->SetNumberField(TEXT("moveInputY"), InputState.MoveAxis.Y);
				const FWuwaSkillData Skill = Character->SkillComponent->GetCurrentSkillData();
				Frame->SetNumberField(TEXT("skillHandle"), Skill.FightStateHandle);
				Frame->SetNumberField(TEXT("interruptLevel"), Skill.InterruptLevel);
				Frame->SetNumberField(TEXT("opportunitySerial"), static_cast<double>(Skill.InputOpportunitySerial));
				Frame->SetNumberField(TEXT("skillStartSerial"), static_cast<double>(Skill.SkillStartSerial));
				Frame->SetBoolField(TEXT("acceptInput"), Skill.bSkillAcceptInput);
				Frame->SetBoolField(TEXT("readyEnd"), Skill.bMainSkillReadyEnd);
				Frame->SetStringField(TEXT("skillAbility"), GetPathNameSafe(Skill.ActiveAbility));
				Frame->SetStringField(TEXT("skillAbilityClass"), Skill.ActiveAbility ? Skill.ActiveAbility->GetClass()->GetPathName() : TEXT(""));
				Frame->SetBoolField(TEXT("canEndNow"), IsValid(Skill.ActiveAbility) && Skill.ActiveAbility->CanEndSkillExecutionNow());
				Frame->SetStringField(TEXT("abilityCurrentMontage"), IsValid(Skill.ActiveAbility) ? GetPathNameSafe(Skill.ActiveAbility->GetCurrentMontage()) : TEXT(""));
				if (Skill.FightStateHandle == SourceSkillHandle && SourceSkillHandle > 0 && Skill.bMainSkillReadyEnd)
					bObservedSourceReadyEnd = true;
				if (Character->FightStateComponent)
					Frame->SetNumberField(TEXT("fightHandle"), Character->FightStateComponent->GetStateData().Handle);
			}
			if (Anim && SourceMontage)
			{
				const FAnimMontageInstance* Source = Anim->GetMontageInstanceForID(SourcePlaybackId);
				const FAnimMontageInstance* Active = Anim->GetActiveInstanceForMontage(SourceMontage);
				Frame->SetNumberField(TEXT("sourcePlaybackId"), SourcePlaybackId);
				Frame->SetBoolField(TEXT("sourceInstanceExists"), Source != nullptr);
				Frame->SetBoolField(TEXT("sourceIsCurrentActiveInstance"), Source && Source == Active);
				Frame->SetNumberField(TEXT("sourcePosition"), Source ? Source->GetPosition() : -1.f);
				Frame->SetNumberField(TEXT("sourceWeight"), Source ? Source->GetWeight() : 0.f);
				Frame->SetNumberField(TEXT("sourcePlayRate"), Source ? Source->GetPlayRate() : 0.f);
				Frame->SetBoolField(TEXT("sourceStopped"), Source && Source->IsStopped());
			}
			Frames.Add(MakeShared<FJsonValueObject>(Frame));
		}

		bool Initialize()
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			World->TimeSeconds = 1.0;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UClass* CharacterClass = bUseRealAnimGraph
				? LoadClass<AWuwaCharacter>(nullptr, TEXT("/Game/Characters/Role/changli/BP_WuwaCharacterBase.BP_WuwaCharacterBase_C"))
				: AWuwaCharacter::StaticClass();
			if (!Test.TestNotNull(TEXT("Character class loads"), CharacterClass)) return false;
			Character = World->SpawnActor<AWuwaCharacter>(CharacterClass, Spawn);
			if (!Test.TestNotNull(TEXT("Transient character exists"), Character)) return false;
			Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!Test.TestTrue(TEXT("Real movement state assembles"), Character->EnsureMovementStateSystem())
				|| !Test.TestTrue(TEXT("Real skill state assembles"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RefreshPolicy();
			USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/Model/Changli.Changli"));
			if (!Test.TestNotNull(TEXT("Real Changli mesh loads"), Mesh)) return false;
			Character->GetMesh()->SetSkeletalMeshAsset(Mesh);
			UClass* AnimationClass = bUseRealAnimGraph
				? LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Role/changli/AnimationBluePrint/ABP_Changli.ABP_Changli_C"))
				: UWuwaSkillNotifyTestAnimInstance::StaticClass();
			if (!Test.TestNotNull(TEXT("Animation class loads"), AnimationClass)) return false;
			Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Character->GetMesh()->SetAnimInstanceClass(AnimationClass);
			Character->GetMesh()->InitAnim(true);
			Anim = Character->GetMesh()->GetAnimInstance();
			if (!Test.TestNotNull(TEXT("Fixture animation instance exists"), Anim)) return false;
			Report->SetStringField(TEXT("characterClass"), CharacterClass->GetPathName());
			Report->SetStringField(TEXT("animationClass"), AnimationClass->GetPathName());
			Character->InputCommandConfig = LoadObject<UWuwaInputCommandConfig>(nullptr,
				TEXT("/Game/Characters/Role/changli/Input/DA_Changli_InputCommands.DA_Changli_InputCommands"));
			if (!Test.TestNotNull(TEXT("Real authored input command table loads"), Character->InputCommandConfig.Get())) return false;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			FObjectPropertyBase* ASCProperty = FindFProperty<FObjectPropertyBase>(AWuwaCharactorBase::StaticClass(), TEXT("AbilitySystemComponent"));
			if (!Test.TestNotNull(TEXT("Native character ASC property exists"), ASCProperty)) return false;
			ASCProperty->SetObjectPropertyValue_InContainer(Character, ASC);
			// Match the project's native player AttributeSet defaults, so an authored
			// GA cost/query is not rejected merely because the fixture lacks attributes.
			ASC->AddAttributeSetSubobject(NewObject<UWuwaAttributeSet>(Character));
			for (int32 Index = 1; Index <= 5; ++Index)
			{
				const FString AbilityPath = FString::Printf(TEXT("/Game/Characters/Role/changli/GA/GA_Attack%d.GA_Attack%d_C"), Index, Index);
				const FString MontagePath = FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), Index, Index);
				UClass* Class = LoadClass<UWuwaGameplayAbilityBase>(nullptr, *AbilityPath);
				UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
				if (!Test.TestNotNull(*AbilityPath, Class) || !Test.TestNotNull(*MontagePath, Montage)) return false;
				AttackClasses.Add(Class);
				AttackMontages.Add(Montage);
				Grant(Class);
			}
			DashClass = LoadClass<UWuwaGameplayAbilityBase>(nullptr, TEXT("/Game/Characters/Role/changli/GA/GA_Dash.GA_Dash_C"));
			DashBackward = LoadObject<UAnimMontage>(nullptr,
				TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B.AM_Changli_Dash_B"));
			if (!Test.TestNotNull(TEXT("Real Dash GA loads"), DashClass)
				|| !Test.TestNotNull(TEXT("Real backward Dash montage loads"), DashBackward)) return false;
			Grant(DashClass);
			ActivationDelegate = ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility* Ability)
			{
				++Activations;
				Capture(TEXT("AbilityActivatedBeforeSkillRegistration"), FString(), Ability);
			});
			EndDelegate = ASC->AbilityEndedCallbacks.AddLambda([this](UGameplayAbility* Ability)
			{
				++AbilityEnds;
				Capture(TEXT("AbilityEndedBeforeSkillCleanup"), FString(), Ability);
			});
			Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
			if (!Test.TestNotNull(TEXT("Real native input controller exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = Controller;
			Controller->Player = LocalPlayer;
			Controller->SetPawn(Character);
			Character->SetController(Controller);
			ASC->InitAbilityActorInfo(Character, Character);
			Controller->AscComponent = ASC;
			Controller->RegisterInputRouteHandlers();
			Handler = Controller->AbilityInputHandler;
			AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Attack.IA_Attack"));
			if (!Test.TestNotNull(TEXT("Production ability handler exists"), Handler)
				|| !Test.TestNotNull(TEXT("Real attack input action loads"), AttackAction)) return false;
			Report->SetNumberField(TEXT("defaultBufferLifetimeSeconds"), Handler->DefaultBufferLifetimeSeconds);
			// Preserve the actual controller Blueprint's configured buffer lifetime in
			// this native controller fixture without running unrelated camera systems.
			if (UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr, TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C")))
			{
				if (const UWuwaAbilityInputHandlerComponent* AuthoredHandler = ControllerClass->GetDefaultObject<AWuwaPlayerController>()->AbilityInputHandler)
				{
					Handler->DefaultBufferLifetimeSeconds = AuthoredHandler->DefaultBufferLifetimeSeconds;
					Handler->BufferLifetimeOverrides = AuthoredHandler->BufferLifetimeOverrides;
					Report->SetNumberField(TEXT("authoredControllerBufferLifetimeSeconds"), Handler->DefaultBufferLifetimeSeconds);
				}
			}
			Capture(TEXT("Initialized"));
			return true;
		}

		void Grant(UClass* Class) const
		{
			FGameplayAbilitySpec Spec(Class, 1);
			Spec.GetDynamicSpecSourceTags().AddTag(Class->GetDefaultObject<UWuwaGameplayAbilityBase>()->OriginalTag);
			ASC->GiveAbility(Spec);
		}

		FWuwaInputEvent Input(FGameplayTag Tag) const
		{
			FWuwaInputEvent Event;
			Event.InputTag = Tag;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = EWuwaInputPhase::Pressed;
			Event.Timestamp = World->GetTimeSeconds();
			Event.SourceAction = AttackAction;
			return Event;
		}

		bool Start(FGameplayTag Tag, UClass* ExpectedClass, UAnimMontage* ExpectedMontage, bool bDirectASCStart = false)
		{
			FWuwaInputEvent InitialInput = Input(Tag);
			// A release establishes the real handler's runtime/subscriptions without
			// selecting Attack01, allowing a middle combo stage to be isolated below.
			if (bDirectASCStart) InitialInput.Phase = EWuwaInputPhase::Released;
			bool bRouted = Controller->GetInputRouter()->DispatchInput(InitialInput);
			if (bDirectASCStart)
			{
				const auto Handles = ASC->FindAbilityHandlesByAbilityTag(ExpectedClass->GetDefaultObject<UWuwaGameplayAbilityBase>()->OriginalTag);
				bRouted = bRouted && Handles.Num() == 1 && ASC->TryActivateAbility(Handles[0], false);
			}
			FObjectPropertyBase* RuntimeProperty = FindFProperty<FObjectPropertyBase>(UWuwaAbilityInputHandlerComponent::StaticClass(), TEXT("Runtime"));
			Runtime = RuntimeProperty ? Cast<UWuwaCombatInputRuntimeBridge>(RuntimeProperty->GetObjectPropertyValue_InContainer(Handler)) : nullptr;
			Capture(bDirectASCStart ? TEXT("StartMiddleStageThroughASC") : TEXT("StartThroughProductionRoute"),
				bRouted ? TEXT("RequestAccepted") : TEXT("RequestRejected"));
			const FWuwaSkillData Current = Character->SkillComponent->GetCurrentSkillData();
			if (!Test.TestTrue(TEXT("Initial input reaches production handler"), bRouted)
				|| !Test.TestNotNull(TEXT("Production handler created its managed runtime"), Runtime)
				|| !Test.TestNotNull(TEXT("Initial real GA retained a skill execution"), Current.ActiveAbility.Get())) return false;
			SourceMontage = ExpectedMontage;
			SourceSkillHandle = Current.FightStateHandle;
			FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(SourceMontage);
			if (Instance) SourcePlaybackId = Instance->GetInstanceID();
			Capture(TEXT("StartedSourcePlayback"));
			return Test.TestEqual(TEXT("Authored input rules select the expected real GA"), Current.ActiveAbility->GetClass(), ExpectedClass)
				&& Test.TestEqual(TEXT("Real GA task registers its montage with ASC"), ASC->GetCurrentMontage(), ExpectedMontage)
				&& Test.TestEqual(TEXT("Real GA task owns ASC animation"), ASC->GetAnimatingAbility(), static_cast<UGameplayAbility*>(Current.ActiveAbility.Get()))
				&& Test.TestNotNull(TEXT("Real GA starts an actual playback instance"), Instance);
		}

		void HoldMovement()
		{
			// This is the public input entry that populates PlayerInputState; the
			// animation notification observes exactly the same semantic move intent.
			WuwaTestInput::SetMoveAxis(Character, FVector2D(0.f, 1.f));
			Character->GetWuwaMovementComponent()->ConsumeInputVector();
			Test.TestTrue(TEXT("Public movement input is visible through PlayerInputState"), Character->GetPlayerInputState().bHasMoveInput);
			Capture(TEXT("MovementKeyHeld"));
		}

		void Advance(float DeltaSeconds)
		{
			World->TimeSeconds += DeltaSeconds;
			if (bUseRealAnimGraph)
			{
				Character->GetMesh()->TickAnimation(DeltaSeconds, true);
				Character->GetMesh()->RefreshBoneTransforms(nullptr);
			}
			else
			{
				CastChecked<UWuwaSkillNotifyTestAnimInstance>(Anim)->AdvanceNotifyTimeline(DeltaSeconds);
			}
			Capture(TEXT("AfterAnimation"));
			Handler->TickComponent(DeltaSeconds, LEVELTICK_All, &Handler->PrimaryComponentTick);
			Capture(TEXT("AfterInputTick"));
		}

		bool ReplayAttackAtReadyEnd(UClass* ExpectedNext)
		{
			float ReadyTime = -1.f;
			TArray<TSharedPtr<FJsonValue>> NotifyRows;
			for (const FAnimNotifyEvent& Event : SourceMontage->Notifies)
			{
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				const UObject* Notify = Event.Notify ? static_cast<const UObject*>(Event.Notify.Get()) : Event.NotifyStateClass.Get();
				Row->SetStringField(TEXT("class"), Notify ? Notify->GetClass()->GetPathName() : TEXT(""));
				Row->SetNumberField(TEXT("time"), Event.GetTime());
				Row->SetNumberField(TEXT("duration"), Event.GetDuration());
				Row->SetNumberField(TEXT("tickType"), Event.MontageTickType.GetValue());
				NotifyRows.Add(MakeShared<FJsonValueObject>(Row));
				if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass())
					&& (ReadyTime < 0.f || Event.GetTime() < ReadyTime)) ReadyTime = Event.GetTime();
			}
			Report->SetArrayField(TEXT("sourceNotifies"), NotifyRows);
			Report->SetStringField(TEXT("sourceMontage"), SourceMontage->GetPathName());
			Report->SetNumberField(TEXT("sourceLength"), SourceMontage->GetPlayLength());
			Report->SetNumberField(TEXT("sourceRateScale"), SourceMontage->RateScale);
			Report->SetNumberField(TEXT("sourceBlendOutTime"), SourceMontage->BlendOut.GetBlendTime());
			Report->SetNumberField(TEXT("sourceBlendOutTriggerTime"), SourceMontage->BlendOutTriggerTime);
			Report->SetNumberField(TEXT("readyEndAssetTime"), ReadyTime);
			if (!Test.TestTrue(TEXT("Original montage contains a real managed ReadyEnd notify"), ReadyTime >= 0.f)) return false;
			FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(SourcePlaybackId);
			if (!Test.TestNotNull(TEXT("Source playback is still present before advancing"), Instance)) return false;
			const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * SourceMontage->RateScale);
			if (!Test.TestTrue(TEXT("Original source playback has positive effective speed"), EffectiveRate > 0.f)) return false;
			const float BufferPosition = FMath::Max(0.f, ReadyTime - 0.1f * EffectiveRate);
			Report->SetNumberField(TEXT("bufferInjectionAssetPosition"), BufferPosition);
			for (int32 Frame = 0; Frame < 2400; ++Frame)
			{
				Instance = Anim->GetMontageInstanceForID(SourcePlaybackId);
				if (!Instance || Instance->IsStopped() || Instance->GetPosition() >= BufferPosition - KINDA_SMALL_NUMBER) break;
				const float Delta = FMath::Min(FrameStepSeconds, (BufferPosition - Instance->GetPosition()) / EffectiveRate);
				Advance(Delta);
			}
			Capture(TEXT("ImmediatelyBeforeBufferedAttack"));
			const FGameplayTag AttackTag = FWuwaGameTags::Get().Input_Combat_Attack;
			const float* Override = Handler->BufferLifetimeOverrides.Find(AttackTag);
			const float Lifetime = Override ? *Override : Handler->DefaultBufferLifetimeSeconds;
			// This is the same runtime created and subscribed by the production handler.
			// Call its reflected entry directly here only so the exact result is recorded.
			const EWuwaCombatInputResult Result = Runtime->ProcessInput(ASC, Input(AttackTag), Lifetime);
			Capture(TEXT("BufferedAttackInput"), UEnum::GetValueAsString(Result));
			Report->SetStringField(TEXT("bufferInputResult"), UEnum::GetValueAsString(Result));
			Test.TestTrue(TEXT("Attack issued 0.1 seconds before ReadyEnd is actually buffered"), Result == EWuwaCombatInputResult::Buffered);
			for (int32 Frame = 0; Frame < FMath::CeilToInt(0.3f / FrameStepSeconds); ++Frame) Advance(FrameStepSeconds);
			const FWuwaSkillData After = Character->SkillComponent->GetCurrentSkillData();
			const bool bReplaced = After.ActiveAbility && After.ActiveAbility->GetClass() == ExpectedNext;
			Report->SetBoolField(TEXT("expectedReplacementActive"), bReplaced);
			Report->SetStringField(TEXT("expectedReplacementClass"), ExpectedNext->GetPathName());
			Capture(TEXT("AfterReadyEndObservation"));
			Test.TestTrue(TEXT("Original ReadyEnd becomes visible before the synchronous skill handoff"), bObservedSourceReadyEnd);
			Test.TestTrue(TEXT("Original montage ReadyEnd consumes the attack into the expected next real GA"), bReplaced);
			Test.TestEqual(TEXT("One buffered attack produces exactly one additional activation"), Activations, 2);
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputAtReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack01To02"));
	if (!Fixture.Initialize()) return false;
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealDashAttackPreinputTest,
	"Wuwa.Combat.RealAssets.BackwardDashPreinputAtReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealDashAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("DashBackwardToAttack01"));
	if (!Fixture.Initialize()) return false;
	if (!Fixture.Start(FWuwaGameTags::Get().Abilities_Movement_Dash, Fixture.DashClass, Fixture.DashBackward)) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[0]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttackPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputWhileMovementHeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack01To02MovementHeld"));
	if (!Fixture.Initialize()) return false;
	Fixture.HoldMovement();
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingDashAttackPreinputTest,
	"Wuwa.Combat.RealAssets.BackwardDashPreinputWhileMovementHeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingDashAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("DashBackwardToAttack01MovementHeld"));
	if (!Fixture.Initialize()) return false;
	// Select the backward Dash with no input, then keep movement pressed throughout
	// the remaining source montage, including its movement-cancel window.
	if (!Fixture.Start(FWuwaGameTags::Get().Abilities_Movement_Dash, Fixture.DashClass, Fixture.DashBackward)) return false;
	Fixture.HoldMovement();
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[0]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttack02PreinputTest,
	"Wuwa.Combat.RealAssets.Attack02PreinputMovementCancelBeforeReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttack02PreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (const int32 FramesPerSecond : {120, 240})
	{
		const FString Scenario = FString::Printf(TEXT("Attack02To03MovementHeld%dHz"), FramesPerSecond);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario);
		Fixture.FrameStepSeconds = 1.f / static_cast<float>(FramesPerSecond);
		if (!Fixture.Initialize()) return false;
		Fixture.HoldMovement();
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[1], Fixture.AttackMontages[1], true)) return false;
		// In the original Attack02 asset, movement cancellation starts roughly 9 ms
		// before ReadyEnd. Fine-grained real timeline steps expose the ordering.
		Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[2]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttack02WithoutPreinputTest,
	"Wuwa.Combat.RealAssets.Attack02MovementExitWithoutPreinput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttack02WithoutPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack02MovementHeldWithoutPreinput"));
	Fixture.FrameStepSeconds = 1.f / 240.f;
	if (!Fixture.Initialize()) return false;
	Fixture.HoldMovement();
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[1], Fixture.AttackMontages[1], true)) return false;
	const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
	const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * Fixture.SourceMontage->RateScale);
	float ReadyTime = 0.f;
	for (const FAnimNotifyEvent& Event : Fixture.SourceMontage->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass())) ReadyTime = Event.GetTime();
	}
	if (!TestTrue(TEXT("Attack02 has a ReadyEnd notify and positive playback rate"), ReadyTime > 0.f && EffectiveRate > 0.f)) return false;
	for (int32 Frame = 0; Frame < FMath::CeilToInt((ReadyTime / EffectiveRate + 0.15f) / Fixture.FrameStepSeconds); ++Frame)
		Fixture.Advance(Fixture.FrameStepSeconds);
	Fixture.Capture(TEXT("MovementExitWithoutAnyAttackInput"));
	TestNull(TEXT("Held movement still exits Attack02 after ReadyEnd when no attack is buffered"),
		Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
	TestEqual(TEXT("Movement exit does not create a new attack"), Fixture.Activations, 1);
	TestEqual(TEXT("The real source GA ends exactly once"), Fixture.AbilityEnds, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackNaturalEndTest,
	"Wuwa.Combat.RealAssets.AllAttackStagesReleaseSkillAfterMontageCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackNaturalEndTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FString Scenario = FString::Printf(TEXT("Attack%02dNaturalCompletion"), Index + 1);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario);
		if (!Fixture.Initialize()) return false;
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[Index], Fixture.AttackMontages[Index], true)) return false;
		const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
		const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * Fixture.SourceMontage->RateScale);
		if (!TestTrue(TEXT("Real attack playback rate is positive"), EffectiveRate > 0.f)) return false;
		const float Duration = Fixture.SourceMontage->GetPlayLength() / EffectiveRate + Fixture.SourceMontage->BlendOut.GetBlendTime() + 0.2f;
		for (int32 Frame = 0; Frame < FMath::CeilToInt(Duration / Fixture.FrameStepSeconds); ++Frame)
			Fixture.Advance(Fixture.FrameStepSeconds);
		Fixture.Capture(TEXT("AfterNaturalMontageCompletion"));
		TestNull(FString::Printf(TEXT("Attack%02d releases skill ownership after the original montage completes"), Index + 1),
			Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
		TestEqual(FString::Printf(TEXT("Attack%02d ends its original GA exactly once"), Index + 1), Fixture.AbilityEnds, 1);
		TestEqual(TEXT("No input cannot create a new skill"), Fixture.Activations, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackGraphPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputWithActualAnimationBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackGraphPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (const bool bHoldMovement : {false, true})
	{
		WuwaRealAttackPreinputTests::FFixture Fixture(*this,
			bHoldMovement ? TEXT("Attack01ActualABPMovementHeld") : TEXT("Attack01ActualABPStanding"), true);
		if (!Fixture.Initialize()) return false;
		if (bHoldMovement) Fixture.HoldMovement();
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
		Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
	}
	return true;
}

#endif
