// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Controller/WuwaPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "Game/Input/WuwaEnhancedInputComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"


class UEnhancedInputLocalPlayerSubsystem;

AWuwaPlayerController::AWuwaPlayerController()
{
	PlayerCameraManagerClass = AWuwaPlayerCameraManager::StaticClass();
	InputRouter = CreateDefaultSubobject<UWuwaInputRouterComponent>(TEXT("InputRouter"));
	//ASC_Input_Handler
	AbilityInputHandler =CreateDefaultSubobject<UWuwaAbilityInputHandlerComponent>(TEXT("AbilityInputHandler"));
	MoveInputHandler =CreateDefaultSubobject<UWuwaMoveInputHandler>(TEXT("MoveInputHandler"));
}

void AWuwaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	// Enhanced Input 只属于本地玩家
	if (!IsLocalController())
	{
		return;
	}
	RegisterInputRouteHandlers();
}

void AWuwaPlayerController::RegisterInputRouteHandlers()
{
	if (!InputRouter)
	{
		return;
	}

	//以后会有更多的系统注册在次
	const FWuwaGameTags& GameTags = FWuwaGameTags::Get();
	InputRouter->RegisterHandler(
		GameTags.Input_Route_Ability,
		AbilityInputHandler);
	
	InputRouter->RegisterHandler(
		GameTags.Input_Route_Movement,
		MoveInputHandler);
	if (IsValid(PlayerCameraManager))
	{
		InputRouter->RegisterHandler(GameTags.Input_Route_Camera, PlayerCameraManager);
	}
	
}

void AWuwaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	
}

void AWuwaPlayerController::SetPawn(APawn* InPawn)
{
	const bool bPawnChanged = GetPawn() != InPawn;
	if (bPawnChanged)
	{
		if (AWuwaCharacter* PreviousCharacter = Cast<AWuwaCharacter>(GetPawn()))
		{
			PreviousCharacter->ResetPlayerInputState();
		}
		if (IsValid(InputRouter))
		{
			InputRouter->ResetInputStates();
		}
		// ASC 缓存也属于旧 Pawn，不能让下一次输入发给上一个角色。
		AscComponent = nullptr;
		
		if (IsValid(AbilityInputHandler))
		{
			AbilityInputHandler->ResetRuntime();
		}
	}
	Super::SetPawn(InPawn);
	if (bPawnChanged)
	{
		if (AWuwaPlayerCameraManager* CameraManager = Cast<AWuwaPlayerCameraManager>(PlayerCameraManager))
		{
			CameraManager->NotifyPawnChanged();
		}
	}
}

void AWuwaPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	if (AWuwaCharacter* ControlledCharacter = Cast<AWuwaCharacter>(GetPawn()))
	{
		ControlledCharacter->ResetPlayerInputState();
	}
	if (IsValid(InputRouter))
	{
		InputRouter->ResetInputStates();
	}
	if (AWuwaPlayerCameraManager* CameraManager = Cast<AWuwaPlayerCameraManager>(PlayerCameraManager))
	{
		CameraManager->ResetCameraInput();
	}
	
	if (IsValid(AbilityInputHandler))
	{
		AbilityInputHandler->ResetRuntime();
	}
	
}


//将指令交给InputRouter,让Router内部消化,并且在内部会记录每个按键的按下时间
void AWuwaPlayerController::HandleRoutedInput(const FInputActionInstance& Instance,FGameplayTag InputTag,FGameplayTag RouteTag,EWuwaInputPhase Phase)
{
	if (!IsValid(InputRouter))
	{
		return;
	}

	const UInputAction* SourceAction =
		Instance.GetSourceAction();

	if (!SourceAction)
	{
		return;
	}
	
	FWuwaInputEvent InputEvent;
	InputEvent.InputTag = InputTag;
	InputEvent.RouteTag = RouteTag;
	InputEvent.Phase = Phase;
	InputEvent.Value = Instance.GetValue();
	InputEvent.SourceAction = SourceAction;
	InputEvent.Timestamp =GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Released/Canceled 时确保轴值归零
	if (Phase == EWuwaInputPhase::Released ||Phase == EWuwaInputPhase::Canceled)
	{
		InputEvent.Value.Reset();
	}

	InputRouter->DispatchInput(InputEvent);
}

//确保Router和所有的Handler都有效,有效的话就注册对应事件
void AWuwaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	//注册输入
	RegisterMappingContext();
	
	//初始化内部字段
	if (IsValid(AbilityInputHandler))
	{
		AbilityInputHandler->ResetRuntime();
	}
	
	//初始化按下事件记录
	if (IsValid(InputRouter))
	{
		InputRouter->ResetInputStates();
	}
	
	UWuwaEnhancedInputComponent* WuwaInputComponent = CastChecked<UWuwaEnhancedInputComponent>(InputComponent);
	
	if (!ensureMsgf(IsValid(InputTagMap),TEXT("InputTagMap is not configured on %s."),*GetName()))
	{
		return;
	}
	if (!ensure(IsValid(InputRouter)))
	{
		return;
	}
	
	//遍历表中的所有IA
	for (const FInputDataAsset& Binding : InputTagMap->InputDataAssetMap)
	{
		if (!Binding.IsConfigured())
		{
			UE_LOG(LogTemp,Warning,TEXT("Invalid input binding in %s."),*GetNameSafe(InputTagMap));
			continue;
		}

		// 按下
		WuwaInputComponent->BindAction(
			Binding.InputAction,
			ETriggerEvent::Started,
			this,
			&AWuwaPlayerController::HandleRoutedInput,
			Binding.InputTag,
			Binding.RouteTag,
			EWuwaInputPhase::Pressed);

		// 轴输入，例如 Move、Look
		if (Binding.InputAction->ValueType !=
			EInputActionValueType::Boolean)
		{
			WuwaInputComponent->BindAction(
				Binding.InputAction,
				ETriggerEvent::Triggered,
				this,
				&AWuwaPlayerController::HandleRoutedInput,
				Binding.InputTag,
				Binding.RouteTag,
				EWuwaInputPhase::Triggered);
		}

		// 正常松开
		WuwaInputComponent->BindAction(
			Binding.InputAction,
			ETriggerEvent::Completed,
			this,
			&AWuwaPlayerController::HandleRoutedInput,
			Binding.InputTag,
			Binding.RouteTag,
			EWuwaInputPhase::Released);

		// 输入被取消，例如切换 MappingContext
		WuwaInputComponent->BindAction(
			Binding.InputAction,
			ETriggerEvent::Canceled,
			this,
			&AWuwaPlayerController::HandleRoutedInput,
			Binding.InputTag,
			Binding.RouteTag,
			EWuwaInputPhase::Canceled);
	}
	
	
}

UWuwaAbilitySystemComponent* AWuwaPlayerController::GetASC()
{
	if (AscComponent == nullptr)
	{
		AscComponent = Cast<UWuwaAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return AscComponent;
}

void AWuwaPlayerController::RegisterMappingContext() const
{
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
	ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(this
		->GetLocalPlayer())) //considering network coding for multiple players, only local player has subsystem
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}



