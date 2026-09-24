#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommand.h"
#include "Game/Input/WuwaInputComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "UObject/UnrealType.h"
#include "Game/Common/WuwaGameTags.h"

namespace
{
	// CharacterMovement 的地面判断还要求 UpdatedComponent，不能仅 new 一个游离组件。
	struct FScopedWalkRunTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

		~FScopedWalkRunTestWorld()
		{
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
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParameters.ObjectFlags |= RF_Transient;
			return World->SpawnActor<AWuwaCharacter>(SpawnParameters);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunResolveTest, "Wuwa.Input.WalkRun.ResolveCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunResolveTest::RunTest(const FString& Parameters)
{
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	if (!TestTrue(TEXT("Walk/run input tag is registered"), Tags.Player_Common_Movement_WalkRun.IsValid()))
	{
		return false;
	}

	FScopedWalkRunTestWorld TestWorld;
	AWuwaCharacter* Character = TestWorld.SpawnCharacter();
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
	if (!TestNotNull(TEXT("Resolve test has a character-owned movement component"), Movement))
	{
		return false;
	}
	// 不运行 BeginPlay/物理 Tick，但保留真实 Character、Capsule 和 UpdatedComponent。
	Movement->MovementMode = MOVE_Walking;

	FWuwaInputEvent Event;
	Event.InputTag = Tags.Player_Common_Movement_WalkRun;
	Event.Phase = EWuwaInputPhase::Pressed;
	const FWuwaInputCommand PressCommand = UWuwaMoveInputHandler::ResolveCommand(Event, Movement);
	TestTrue(TEXT("A ground press resolves to SwitchWalk"), PressCommand.Type == EWuwaInputCommandType::SwitchWalk);
	TestTrue(TEXT("Resolving a command does not change gait"), Movement->GetDesiredGait() == EWuwaGait::Run);

	for (const EWuwaInputPhase Phase : { EWuwaInputPhase::Triggered, EWuwaInputPhase::Released, EWuwaInputPhase::Canceled })
	{
		Event.Phase = Phase;
		TestTrue(FString::Printf(TEXT("Phase %d does not toggle walk/run"), static_cast<int32>(Phase)),
			UWuwaMoveInputHandler::ResolveCommand(Event, Movement).Type == EWuwaInputCommandType::None);
	}

	Event.Phase = EWuwaInputPhase::Pressed;
	Event.InputTag = Tags.Player_Common_Movement_Move;
	TestTrue(TEXT("Move axis input is not a walk/run command"),
		UWuwaMoveInputHandler::ResolveCommand(Event, Movement).Type == EWuwaInputCommandType::None);
	Event.InputTag = FGameplayTag();
	TestTrue(TEXT("An empty input tag produces no command"),
		UWuwaMoveInputHandler::ResolveCommand(Event, Movement).Type == EWuwaInputCommandType::None);
	Event.InputTag = Tags.Player_Common_Movement_WalkRun;
	TestTrue(TEXT("No movement component produces no command"),
		UWuwaMoveInputHandler::ResolveCommand(Event, nullptr).Type == EWuwaInputCommandType::None);
	Character->bIsCrouched = true;
	TestTrue(TEXT("Crouching cannot resolve SwitchWalk"),
		UWuwaMoveInputHandler::ResolveCommand(Event, Movement).Type == EWuwaInputCommandType::None);
	Character->bIsCrouched = false;

	for (const EMovementMode Mode : { MOVE_Falling, MOVE_Custom, MOVE_Swimming, MOVE_Flying, MOVE_None })
	{
		Movement->MovementMode = Mode;
		Movement->CustomMovementMode = ECustomMoveMode::MOVE_Climb;
		TestTrue(FString::Printf(TEXT("Movement mode %d cannot resolve SwitchWalk"), static_cast<int32>(Mode)),
			UWuwaMoveInputHandler::ResolveCommand(Event, Movement).Type == EWuwaInputCommandType::None);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunExecuteTest, "Wuwa.Input.WalkRun.ExecuteCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunExecuteTest::RunTest(const FString& Parameters)
{
	FScopedWalkRunTestWorld TestWorld;
	AWuwaCharacter* Character = TestWorld.SpawnCharacter();
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
	if (!TestNotNull(TEXT("Execution test has a character-owned movement component"), Movement))
	{
		return false;
	}
	Movement->MovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 650.f;
	const FVector OriginalVelocity(123.f, 45.f, 67.f);
	Movement->Velocity = OriginalVelocity;

	FWuwaInputCommand Command;
	Command.Type = EWuwaInputCommandType::SwitchWalk;
	TestTrue(TEXT("Ground movement permits walk/run switching"), Movement->CanSwitchWalk());
	TestTrue(TEXT("Ground SwitchWalk command is accepted"), Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Run switches to Walk"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("Walk has a lower speed cap"), Movement->GetMaxSpeed() < Movement->MaxWalkSpeed);
	TestEqual(TEXT("Switching does not overwrite configured Run speed"), Movement->MaxWalkSpeed, 650.f);
	TestEqual(TEXT("Switching does not overwrite actual velocity"), Movement->Velocity, OriginalVelocity);
	TestTrue(TEXT("A second SwitchWalk command is accepted"), Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Walk switches back to Run"), Movement->GetDesiredGait() == EWuwaGait::Run);
	TestEqual(TEXT("Run retains its configured speed"), Movement->GetMaxSpeed(), 650.f);

	const FWuwaInputCommand NoCommand;
	TestFalse(TEXT("None is not an executable movement command"), Movement->ExecuteInputCommand(NoCommand));
	TestTrue(TEXT("None does not change gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
	Character->bIsCrouched = true;
	TestFalse(TEXT("Crouching rejects switching"), Movement->CanSwitchWalk());
	TestFalse(TEXT("Crouching rejects the command"), Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("A rejected crouching command preserves gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
	Character->bIsCrouched = false;

	for (const EMovementMode Mode : { MOVE_Falling, MOVE_Custom, MOVE_Swimming, MOVE_Flying, MOVE_None })
	{
		Movement->MovementMode = Mode;
		Movement->CustomMovementMode = ECustomMoveMode::MOVE_Climb;
		TestFalse(FString::Printf(TEXT("Movement mode %d rejects switching"), static_cast<int32>(Mode)), Movement->CanSwitchWalk());
		TestFalse(FString::Printf(TEXT("Movement mode %d rejects the command"), static_cast<int32>(Mode)), Movement->ExecuteInputCommand(Command));
		TestTrue(TEXT("Rejected commands preserve gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
		Movement->ToggleWalkRun();
		TestTrue(TEXT("The legacy toggle API cannot bypass permission"), Movement->GetDesiredGait() == EWuwaGait::Run);
	}

	Movement->MovementMode = MOVE_Walking;
	FWuwaInputEvent Event;
	Event.InputTag = FWuwaGameTags::Get().Player_Common_Movement_WalkRun;
	Event.Phase = EWuwaInputPhase::Pressed;
	const FWuwaInputCommand PendingCommand = UWuwaMoveInputHandler::ResolveCommand(Event, Movement);
	TestTrue(TEXT("A valid command was resolved before leaving the ground"), PendingCommand.Type == EWuwaInputCommandType::SwitchWalk);
	Movement->MovementMode = MOVE_Falling;
	TestFalse(TEXT("Execution rechecks permission after context changes"), Movement->ExecuteInputCommand(PendingCommand));
	TestTrue(TEXT("A stale command preserves gait"), Movement->GetDesiredGait() == EWuwaGait::Run);

	Movement->MovementMode = MOVE_Walking;
	Movement->Velocity = FVector::ZeroVector;
	TestTrue(TEXT("Idle also permits switching the desired gait"), Movement->ExecuteInputCommand(Command));
	TestTrue(TEXT("Idle switching records Walk"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	TestEqual(TEXT("Idle switching does not create movement"), Movement->Velocity, FVector::ZeroVector);
	Movement->MovementMode = MOVE_Custom;
	Movement->CustomMovementMode = ECustomMoveMode::MOVE_Climb;
	TestEqual(TEXT("Walk preference does not change existing climbing speed"), Movement->GetMaxSpeed(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunRouterIntegrationTest, "Wuwa.Input.WalkRun.RouterIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunRouterIntegrationTest::RunTest(const FString& Parameters)
{
	// 独立临时世界，不加载/修改关卡，也不调用 BeginPlay 或 Possess。
	// SetPawn 只建立本测试要验证的“当前角色”关系，避免触发无关的 GAS/HUD 初始化。
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("A temporary integration-test world was created"), TestWorld))
	{
		return false;
	}
	AWuwaPlayerController* Controller = nullptr;
	ON_SCOPE_EXIT
	{
		if (IsValid(Controller))
		{
			Controller->SetPawn(nullptr);
		}
		TestWorld->DestroyWorld(false);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	Controller = TestWorld->SpawnActor<AWuwaPlayerController>(SpawnParameters);
	AWuwaCharacter* FirstCharacter = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	AWuwaCharacter* SecondCharacter = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	const bool bHasController = TestNotNull(TEXT("The controller was spawned"), Controller);
	const bool bHasFirstCharacter = TestNotNull(TEXT("The first character was spawned"), FirstCharacter);
	const bool bHasSecondCharacter = TestNotNull(TEXT("The second character was spawned"), SecondCharacter);
	if (!bHasController || !bHasFirstCharacter || !bHasSecondCharacter)
	{
		return false;
	}

	UWuwaMovementComponent* FirstMovement = FirstCharacter->GetWuwaMovementComponent();
	UWuwaMovementComponent* SecondMovement = SecondCharacter->GetWuwaMovementComponent();
	UWuwaInputRouterComponent* Router = Controller->GetInputRouter();
	const bool bHasFirstMovement = TestNotNull(TEXT("The first character owns Wuwa movement"), FirstMovement);
	const bool bHasSecondMovement = TestNotNull(TEXT("The second character owns Wuwa movement"), SecondMovement);
	const bool bHasRouter = TestNotNull(TEXT("The controller owns the input router"), Router);
	if (!bHasFirstMovement || !bHasSecondMovement || !bHasRouter)
	{
		return false;
	}
	FirstMovement->MovementMode = MOVE_Walking;
	SecondMovement->MovementMode = MOVE_Walking;
	Controller->RegisterInputRouteHandlers();
	Controller->SetPawn(FirstCharacter);

	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	FInputDataAsset Binding{};
	Binding.InputAction = NewObject<UInputAction>(Controller);
	Binding.InputTag = Tags.Player_Common_Movement_WalkRun;
	Binding.RouteTag = Tags.Input_Route_Movement;
	if (!TestTrue(TEXT("The integration binding is configured"), Binding.IsConfigured()))
	{
		return false;
	}

	FWuwaInputEvent Event;
	Event.Phase = EWuwaInputPhase::Pressed;
	Event.Value = FInputActionValue(true);
	// 故意不手填 Event 的 Tag/SourceAction，验证 Router 使用 Binding 补齐后调用接口。
	TestTrue(TEXT("Router dispatch reaches the handler and movement component"), Router->DispatchInput(Binding, Event));
	TestTrue(TEXT("The current character switches to Walk"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The other character remains Run"), SecondMovement->GetDesiredGait() == EWuwaGait::Run);

	Event.Phase = EWuwaInputPhase::Released;
	Event.Value.Reset();
	TestFalse(TEXT("Release produces no executable movement command"), Router->DispatchInput(Binding, Event));
	TestTrue(TEXT("Release does not toggle the current character again"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);

	Controller->SetPawn(SecondCharacter);
	Event.Phase = EWuwaInputPhase::Pressed;
	Event.Value = FInputActionValue(true);
	TestTrue(TEXT("After switching Pawn, dispatch uses the new character"), Router->DispatchInput(Binding, Event));
	TestTrue(TEXT("The new character switches to Walk"), SecondMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The previous character is not toggled back to Run"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);

	Controller->SetPawn(nullptr);
	TestFalse(TEXT("No controlled Pawn safely rejects the input"), Router->DispatchInput(Binding, Event));
	TestTrue(TEXT("No-Pawn input leaves the first character unchanged"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("No-Pawn input leaves the second character unchanged"), SecondMovement->GetDesiredGait() == EWuwaGait::Walk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunAssetsTest, "Wuwa.Input.WalkRun.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunAssetsTest::RunTest(const FString& Parameters)
{
	// 只读加载项目真实配置，覆盖“代码正确但 IA/IMC/路由未接线”的问题。
	const UInputAction* WalkRunAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_WalkRun.IA_WalkRun"));
	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	const UWuwaInputDataAsset* InputTagMap = LoadObject<UWuwaInputDataAsset>(nullptr, TEXT("/Game/DataAsset/DA_InputActionTagAsset.DA_InputActionTagAsset"));
	const bool bHasAction = TestNotNull(TEXT("Walk/run input action exists"), WalkRunAction);
	const bool bHasContext = TestNotNull(TEXT("Character mapping context exists"), MappingContext);
	const bool bHasTagMap = TestNotNull(TEXT("Input tag map exists"), InputTagMap);
	if (!bHasAction || !bHasContext || !bHasTagMap)
	{
		return false;
	}

	TestTrue(TEXT("Walk/run is a Boolean action"), WalkRunAction->ValueType == EInputActionValueType::Boolean);
	int32 AltMappingCount = 0;
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action == WalkRunAction && Mapping.Key == EKeys::LeftAlt)
		{
			++AltMappingCount;
		}
	}
	TestEqual(TEXT("Left Alt maps to WalkRun exactly once"), AltMappingCount, 1);

	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	int32 WalkRunRowCount = 0;
	int32 WalkRunTagCount = 0;
	for (const FInputDataAsset& Binding : InputTagMap->InputDataAssetMap)
	{
		if (Binding.InputTag == Tags.Player_Common_Movement_WalkRun)
		{
			++WalkRunTagCount;
		}
		if (Binding.InputAction == WalkRunAction)
		{
			++WalkRunRowCount;
			TestTrue(TEXT("WalkRun binding is configured"), Binding.IsConfigured());
			TestTrue(TEXT("WalkRun binding has the semantic input tag"), Binding.InputTag == Tags.Player_Common_Movement_WalkRun);
			TestTrue(TEXT("WalkRun is routed to Movement"), Binding.RouteTag == Tags.Input_Route_Movement);
		}
	}
	TestEqual(TEXT("The WalkRun action has exactly one binding"), WalkRunRowCount, 1);
	TestEqual(TEXT("The WalkRun semantic tag occurs exactly once"), WalkRunTagCount, 1);

	// 沿真实蓝图 CDO 确认这套配置被角色/控制器引用，而不仅是资产单独存在。
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Core/BP_WuwaGameMode.BP_WuwaGameMode_C"));
	UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr, TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C"));
	UClass* CharacterClass = LoadClass<AWuwaCharacter>(nullptr, TEXT("/Game/Characters/Player/BP_WuwaCharacterBase.BP_WuwaCharacterBase_C"));
	const bool bHasGameModeClass = TestNotNull(TEXT("The project GameMode blueprint loads"), GameModeClass);
	const bool bHasControllerClass = TestNotNull(TEXT("The project controller blueprint loads"), ControllerClass);
	const bool bHasCharacterClass = TestNotNull(TEXT("The project character blueprint loads"), CharacterClass);
	if (!bHasGameModeClass || !bHasControllerClass || !bHasCharacterClass)
	{
		return false;
	}
	const AGameModeBase* GameModeDefaults = GameModeClass->GetDefaultObject<AGameModeBase>();
	const AWuwaPlayerController* ControllerDefaults = ControllerClass->GetDefaultObject<AWuwaPlayerController>();
	const AWuwaCharacter* CharacterDefaults = CharacterClass->GetDefaultObject<AWuwaCharacter>();
	TestTrue(TEXT("GameMode uses the expected player controller"), GameModeDefaults->PlayerControllerClass.Get() == ControllerClass);
	TestTrue(TEXT("GameMode uses the expected character"), GameModeDefaults->DefaultPawnClass.Get() == CharacterClass);
	TestTrue(TEXT("The player controller uses the configured input tag map"), ControllerDefaults->InputTagMap == InputTagMap);

	const UWuwaInputComponent* CharacterInput = CharacterDefaults->WuwaInputComponent;
	if (!TestNotNull(TEXT("The character has its mapping-context component"), CharacterInput))
	{
		return false;
	}
	const FObjectPropertyBase* MappingProperty = FindFProperty<FObjectPropertyBase>(CharacterInput->GetClass(), TEXT("DefaultMappingContext"));
	if (!TestNotNull(TEXT("The mapping-context property is available for read-only inspection"), MappingProperty))
	{
		return false;
	}
	TestTrue(TEXT("The character uses IMC_Character"), MappingProperty->GetObjectPropertyValue_InContainer(CharacterInput) == MappingContext);
	return true;
}

#endif
