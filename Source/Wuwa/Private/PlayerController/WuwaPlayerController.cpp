// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerController/WuwaPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Character/WuwaMoveInputHandler.h"
#include "Cores/WuwaEnhancedInputComponent.h"
#include "Input/WuwaInputRouterComponent.h"
#include "Tools/DebugHelper.h"
#include "WuwaASC/WuwaAbilityInputHandlerComponent.h"

AWuwaPlayerController::AWuwaPlayerController()
{
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
	
}

void AWuwaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	
}

void AWuwaPlayerController::ActionPressed(FGameplayTag PressedTag)
{
	GetASC()->InputWithTagPressed(PressedTag);
}

void AWuwaPlayerController::ActionReleased(FGameplayTag PressedTag)
{
	GetASC()->InputWithTagReleased(PressedTag);
}

void AWuwaPlayerController::ActionHold(FGameplayTag PressedTag)
{
}

void AWuwaPlayerController::HandleRoutedInput(
	const FInputActionInstance& Instance,
	FGameplayTag InputTag,
	FGameplayTag RouteTag,
	EWuwaInputPhase Phase)
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

	// Router 当前接口仍然要求完整 Binding，
	// 所以这里根据绑定时保存的 Tag 重建运行时 Binding。
	FInputDataAsset RuntimeBinding;
	RuntimeBinding.InputAction = SourceAction;
	RuntimeBinding.InputTag = InputTag;
	RuntimeBinding.RouteTag = RouteTag;

	FWuwaInputEvent InputEvent;
	InputEvent.InputTag = InputTag;
	InputEvent.Phase = Phase;
	InputEvent.Value = Instance.GetValue();
	InputEvent.SourceAction = SourceAction;
	InputEvent.Timestamp =
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// Released/Canceled 时确保轴值归零
	if (Phase == EWuwaInputPhase::Released ||
		Phase == EWuwaInputPhase::Canceled)
	{
		InputEvent.Value.Reset();
	}

	InputRouter->DispatchInput(
		RuntimeBinding,
		InputEvent);
}


void AWuwaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
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



