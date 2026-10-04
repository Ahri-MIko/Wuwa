// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Controller/WuwaPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "Game/Input/WuwaEnhancedInputComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
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
		// 旧 Pawn 的输入意图（移动轴、按住的按键）随控制权一起清空。
		if (AWuwaCharacter* PreviousCharacter = Cast<AWuwaCharacter>(GetPawn()))
		{
			PreviousCharacter->ResetPlayerInputState();
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
	if (AWuwaPlayerCameraManager* CameraManager = Cast<AWuwaPlayerCameraManager>(PlayerCameraManager))
	{
		CameraManager->ResetCameraInput();
	}
	
	if (IsValid(AbilityInputHandler))
	{
		AbilityInputHandler->ResetRuntime();
	}
	HeldDispatchedActions.Empty();
}


//把 Enhanced Input 的回调整理成语义事件,交给 RouteInputEvent
void AWuwaPlayerController::HandleRoutedInput(const FInputActionInstance& Instance,FGameplayTag InputTag,FGameplayTag RouteTag,EWuwaInputPhase Phase)
{
	const UInputAction* SourceAction =
		Instance.GetSourceAction();

	if (!SourceAction)
	{
		return;
	}

	// Held 每次按下只发一次：Hold 触发器到点后每帧都会 Triggered，只取第一帧。
	if (Phase == EWuwaInputPhase::Held)
	{
		if (HeldDispatchedActions.Contains(SourceAction))
		{
			return;
		}
		HeldDispatchedActions.Add(SourceAction);
	}
	else if (Phase != EWuwaInputPhase::Triggered)
	{
		HeldDispatchedActions.Remove(SourceAction);
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

	RouteInputEvent(InputEvent);
}

bool AWuwaPlayerController::RouteInputEvent(const FWuwaInputEvent& InputEvent)
{
	// 先记录再分发：GA 拒绝激活或没有系统处理时，按住/松开也不能丢。
	// 只写给当前控制的 Pawn；没有 Pawn 时输入不记录在任何地方。
	if (const APawn* ControlledPawn = GetPawn())
	{
		if (UWuwaInputIntentComponent* Intent = ControlledPawn->FindComponentByClass<UWuwaInputIntentComponent>())
		{
			Intent->RecordInputEvent(InputEvent);
		}
	}
	return IsValid(InputRouter) && InputRouter->DispatchInput(InputEvent);
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
	
	//重新绑定输入时，清空当前 Pawn 的按下记录
	if (const APawn* ControlledPawn = GetPawn())
	{
		if (UWuwaInputIntentComponent* Intent = ControlledPawn->FindComponentByClass<UWuwaInputIntentComponent>())
		{
			Intent->ResetInputs();
		}
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

		// 按键配了 Hold 触发器：到达阈值时 Triggered，转成 Held（每次按下一次）。
		// 没配 Hold 的按键不绑 Triggered，否则按住期间每帧都会触发。
		if (Binding.InputAction->ValueType == EInputActionValueType::Boolean && HasHoldTrigger(Binding.InputAction))
		{
			WuwaInputComponent->BindAction(
				Binding.InputAction,
				ETriggerEvent::Triggered,
				this,
				&AWuwaPlayerController::HandleRoutedInput,
				Binding.InputTag,
				Binding.RouteTag,
				EWuwaInputPhase::Held);
		}

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

bool AWuwaPlayerController::HasHoldTrigger(const UInputAction* Action) const
{
	auto IsHold = [](const UInputTrigger* Trigger) { return Trigger && Trigger->IsA<UInputTriggerHold>(); };
	if (!Action) return false;
	if (Action->Triggers.ContainsByPredicate(IsHold)) return true;
	if (!DefaultMappingContext) return false;
	for (const FEnhancedActionKeyMapping& Mapping : DefaultMappingContext->GetMappings())
	{
		if (Mapping.Action == Action && Mapping.Triggers.ContainsByPredicate(IsHold)) return true;
	}
	return false;
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



