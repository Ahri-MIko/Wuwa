// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/WuwaMoveInputHandler.h"

#include "Character/WuwaCharacter.h"
#include "Character/CharacterMovementComponent/WuwaMovementComponent.h"
#include "PlayerController/WuwaPlayerController.h"
// Sets default values for this component's properties
UWuwaMoveInputHandler::UWuwaMoveInputHandler()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

bool UWuwaMoveInputHandler::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	if (InputEvent.InputTag == Tags.Player_Common_Movement_WalkRun)
	{
		// 每次都取当前控制的 Pawn；切人后不能继续切换旧角色的步态。
		const APlayerController* PC = Cast<APlayerController>(GetOwner());
		const AWuwaCharacter* Character = PC ? Cast<AWuwaCharacter>(PC->GetPawn()) : nullptr;
		UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
		if (!Movement)
		{
			return false;
		}

		const FWuwaInputCommand Command = ResolveCommand(InputEvent, Movement);
		// 第二步：交给拥有移动状态的组件执行，不直接改速度或 AnimInstance。
		return Movement->ExecuteInputCommand(Command);
	}

	// Move/Look 是连续值，Started 不再额外消费一遍，避免首帧重复移动/转镜头。
	const bool bAxisUpdate = InputEvent.Phase == EWuwaInputPhase::Triggered
		|| InputEvent.Phase == EWuwaInputPhase::Released
		|| InputEvent.Phase == EWuwaInputPhase::Canceled;
	if (!bAxisUpdate)
	{
		return false;
	}

	if (InputEvent.InputTag == Tags.Player_Common_Movement_Move)
	{
		OnMove.Broadcast(InputEvent.Value);
		return true;
	}
	if (InputEvent.InputTag == Tags.Player_Common_Camera_Rotate)
	{
		OnLook.Broadcast(InputEvent.Value);
		return true;
	}
	return false;
}

FWuwaInputCommand UWuwaMoveInputHandler::ResolveCommand(
	const FWuwaInputEvent& InputEvent, const UWuwaMovementComponent* Movement)
{
	FWuwaInputCommand Command;
	if (InputEvent.InputTag == FWuwaGameTags::Get().Player_Common_Movement_WalkRun
		&& InputEvent.Phase == EWuwaInputPhase::Pressed
		&& Movement && Movement->CanSwitchWalk())
	{
		Command.Type = EWuwaInputCommandType::SwitchWalk;
	}
	return Command;
}

