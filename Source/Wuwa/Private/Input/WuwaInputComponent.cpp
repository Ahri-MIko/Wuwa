// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/WuwaInputComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Input/WuwaInputConfig.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UWuwaInputComponent::UWuwaInputComponent()
{
	// 需要 Tick 来扫描长按
	PrimaryComponentTick.bCanEverTick = true;
}

void UWuwaInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// 建立 IA → 委托 的分发表
	DelegateMap.Add(JumpAction, &OnJump);
	DelegateMap.Add(AttackAction, &OnAttack);
	DelegateMap.Add(DodgeAction, &OnDodge);
	DelegateMap.Add(ClimbAction, &OnClimb);
	// WalkRun 已迁移到 Controller -> Router -> MoveInputHandler，不能再绑定旧委托通路。
	DelegateMap.Add(Skill1Action, &OnSkill1);
	DelegateMap.Add(Echo1Action, &OnEcho1);
	DelegateMap.Add(UltimateAction, &OnUltimate);
	DelegateMap.Add(SwitchChar1Action, &OnSwitchChar1);
	DelegateMap.Add(SwitchChar2Action, &OnSwitchChar2);
	DelegateMap.Add(SwitchChar3Action, &OnSwitchChar3);
	DelegateMap.Add(LockOnAction, &OnLockOn);
	DelegateMap.Add(AimAction, &OnAim);
	DelegateMap.Add(InteractAction, &OnInteract);
	DelegateMap.Add(DescendAction, &OnDescend);
	DelegateMap.Remove(nullptr);   // 兜底:编辑器里忘了配某个 IA 时,不让空键进表

	RegisterMappingContext();
	BindInputActions();
}

//Context Map
void UWuwaInputComponent::RegisterMappingContext()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) //considering network coding for multiple players, only local player has subsystem
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void UWuwaInputComponent::BindInputActions()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent);
	if (!EIC) return;

	// 轴输入:Triggered 逐帧透传
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UWuwaInputComponent::HandleMoveInput);
		// 松开时补一次归零值,BP 侧收到 (0,0) 即停止移动
		EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &UWuwaInputComponent::HandleMoveInput);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &UWuwaInputComponent::HandleLookInput);
	}

	// 按键动作:分发表里有的全部绑定,DelegateMap 是唯一的动作清单
	for (const auto& Pair : DelegateMap)
	{
		EIC->BindAction(Pair.Key, ETriggerEvent::Started, this, &UWuwaInputComponent::HandleActionStarted);
		EIC->BindAction(Pair.Key, ETriggerEvent::Completed, this, &UWuwaInputComponent::HandleActionCompleted);
	}
}

void UWuwaInputComponent::HandleMoveInput(const FInputActionValue& Value)
{
	
}

void UWuwaInputComponent::HandleLookInput(const FInputActionValue& Value)
{
	
}

void UWuwaInputComponent::HandleActionStarted(const FInputActionInstance& Instance)
{
	const UInputAction* Action = Instance.GetSourceAction();
	if (!Action) return;

	const float Now = GetWorld()->GetTimeSeconds();

	// 更新状态表:置位 + 记按下时刻 + 算好下一次长按触发时刻
	FWuwaKeyState& State = KeyStates.FindOrAdd(Action);
	State.bDown = true;
	State.PressTime = Now;

	const FWuwaHoldConfig* HoldCfg = InputConfig ? InputConfig->FindHoldConfig(Action) : nullptr;
	State.NextHoldTime = (HoldCfg && HoldCfg->TriggerTime > 0.f) ? Now + HoldCfg->TriggerTime : -1.f;

	// 按下事件立即广播(Press 的 Time 恒为 0)
	BroadcastActionEvent(Action, EWuwaInputEventType::Press, 0.f);
}

void UWuwaInputComponent::HandleActionCompleted(const FInputActionInstance& Instance)
{
	const UInputAction* Action = Instance.GetSourceAction();
	if (!Action) return;

	FWuwaKeyState* State = KeyStates.Find(Action);
	if (!State || !State->bDown) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const float HeldTime = Now - State->PressTime;

	// 复位状态表
	State->bDown = false;
	State->NextHoldTime = -1.f;

	// 抬起事件带总按住时长
	BroadcastActionEvent(Action, EWuwaInputEventType::Release, HeldTime);
}

void UWuwaInputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetWorld()->GetTimeSeconds();

	// 扫描长按:bDown 且有待触发的 NextHoldTime 且时刻已到
	for (auto& Pair : KeyStates)
	{
		FWuwaKeyState& State = Pair.Value;
		if (!State.bDown || State.NextHoldTime < 0.f || Now < State.NextHoldTime)
		{
			continue;
		}

		const UInputAction* Action = Pair.Key;
		BroadcastActionEvent(Action, EWuwaInputEventType::Hold, Now - State.PressTime);

		// 连续触发:推进一个周期;单次触发:置哨兵值,后续帧不再扫到
		const FWuwaHoldConfig* HoldCfg = InputConfig ? InputConfig->FindHoldConfig(Action) : nullptr;
		if (!HoldCfg || !HoldCfg->bRepeat)
		{
			State.NextHoldTime = -1.f;
		}
	}
}

void UWuwaInputComponent::BroadcastActionEvent(const UInputAction* Action,
	EWuwaInputEventType EventType, float Time)
{
	if (FOnWuwaActionInput* const* Delegate = DelegateMap.Find(Action))
	{
		(*Delegate)->Broadcast(EventType, Time);
	}
}

bool UWuwaInputComponent::IsKeyDown(const UInputAction* Action) const
{
	const FWuwaKeyState* State = KeyStates.Find(Action);
	return State && State->bDown;
}

float UWuwaInputComponent::GetKeyDownTime(const UInputAction* Action) const
{
	const FWuwaKeyState* State = KeyStates.Find(Action);
	if (!State || !State->bDown) return 0.f;
	return GetWorld()->GetTimeSeconds() - State->PressTime;
}
