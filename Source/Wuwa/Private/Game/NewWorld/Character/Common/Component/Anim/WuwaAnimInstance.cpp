#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"

#include "Animation/AnimNode_StateMachine.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionMath.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"

void UWuwaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedCharacter.Reset();
	CachedMovement.Reset();
	if (!AnimLogicParams)
	{
		AnimLogicParams = NewObject<UWuwaAnimLogicParams>(this);
	}
	ResetAnimationData();
}

void UWuwaAnimInstance::NativeUninitializeAnimation()
{
	CachedCharacter.Reset();
	CachedMovement.Reset();
	ResetAnimationData();
	Super::NativeUninitializeAnimation();
}

void UWuwaAnimInstance::ResetAnimationData()
{
	LocomotionData = FWuwaLocomotionAnimData{};
	if (AnimLogicParams)
	{
		AnimLogicParams->Reset();
	}
}

void UWuwaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	check(IsInGameThread());

	// NativeUpdateAnimation 在游戏线程采集 UObject 数据。
	// 不把这些直接访问挪到 NativeThreadSafeUpdateAnimation。
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	UWuwaMovementComponent* Movement = IsValid(Character)
		? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	//切换角色就更新,但是其实一般是不会切换的,因为一个角色对应一个BP
	if (CachedCharacter.Get() != Character || CachedMovement.Get() != Movement)
	{
		CachedCharacter = Character;
		CachedMovement = Movement;
		ResetAnimationData();
	}

	if (!AnimLogicParams)
	{
		AnimLogicParams = NewObject<UWuwaAnimLogicParams>(this);
	}
	if (!UWuwaAnimDataLibrary::UpdateAnimationData(Movement, AnimLogicParams))
	{
		ResetAnimationData();
		return;
	}

	// 一次性提交本次更新结果；动画节点只消费快照，不反写 Movement/输入。
	// UE 随后调用 BlueprintUpdateAnimation，蓝图可以读到本次 NativeUpdate 的结果。
	LocomotionData = WuwaLocomotion::BuildAnimationData(AnimLogicParams->GetMoveData(),
		AnimLogicParams->GetStateData(), LocomotionData.bHasMovingSpeed,
		MovingEnterThreshold, MovingExitThreshold, MoveIntentThreshold);
}

void UWuwaAnimInstance::ResetSprintDesire()
{
	check(IsInGameThread());
	// 每次解析当前 Owner，避免切换角色后清到缓存里的旧 CMC；预览无角色时也可安全调用。
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	UWuwaMovementComponent* Movement = IsValid(Character)
		? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (IsValid(Movement))
	{
		Movement->ClearSprintDesire();
	}
	LocomotionData.SprintDesire = EWuwaSprintDesire::None;
	if (AnimLogicParams)
	{
		if (UWuwaAnimDataLibrary::UpdateAnimationData(Movement, AnimLogicParams))
		{
			// 清除需求可能已同步把 Sprint 改回 Walk/Run；本次快照必须一起刷新。
			LocomotionData = WuwaLocomotion::BuildAnimationData(AnimLogicParams->GetMoveData(),
				AnimLogicParams->GetStateData(), LocomotionData.bHasMovingSpeed,
				MovingEnterThreshold, MovingExitThreshold, MoveIntentThreshold);
		}
		else
		{
			ResetAnimationData();
		}
	}
}

#pragma region Debug

FWuwaDebugAnimStateMachine UWuwaAnimInstance::GetDebugStateMachineData(FName MachineName)
{
	check(IsInGameThread());
	FWuwaDebugAnimStateMachine Result;
	Result.MachineName = MachineName;

	int32 MachineIndex = INDEX_NONE;
	const FBakedAnimationStateMachine* Description = nullptr;
	GetStateMachineIndexAndDescription(MachineName, MachineIndex, &Description);
	const FAnimNode_StateMachine* Machine = MachineIndex != INDEX_NONE
		? GetStateMachineInstance(MachineIndex) : nullptr;
	if (!Machine || !Description)
	{
		return Result;
	}

	Result.bFound = true;
	const int32 CurrentStateIndex = Machine->GetCurrentState();
	if (!Description->States.IsValidIndex(CurrentStateIndex))
	{
		// 预览、初始化之前可能还没有当前状态。不要调用依赖内部描述已初始化的 GetStateIndex。
		return Result;
	}

	Result.bInitialized = true;
	Result.MachineWeight = GetInstanceMachineWeight(MachineIndex);
	Result.CurrentStateName = Description->States[CurrentStateIndex].StateName;
	for (int32 StateIndex = 0; StateIndex < Description->States.Num(); ++StateIndex)
	{
		const FBakedAnimationState& State = Description->States[StateIndex];
		if (State.bIsAConduit)
		{
			continue;
		}

		const float Weight = Machine->GetStateWeight(StateIndex);
		if (Weight > 0.f)
		{
			FWuwaDebugAnimStateWeight& Entry = Result.ActiveStates.AddDefaulted_GetRef();
			Entry.StateName = State.StateName;
			Entry.Weight = Weight;
			Entry.bIsCurrentState = StateIndex == CurrentStateIndex;
		}
	}
	Result.ActiveStates.StableSort([](const FWuwaDebugAnimStateWeight& A, const FWuwaDebugAnimStateWeight& B)
	{
		return A.Weight > B.Weight;
	});
	return Result;
}


FName UWuwaAnimInstance::GetDebugStateName(FName MachineName)
{
	return GetDebugStateMachineData(MachineName).CurrentStateName;
}

float UWuwaAnimInstance::GetDebugStateWeight(FName MachineName, FName StateName)
{
	const FWuwaDebugAnimStateMachine Data = GetDebugStateMachineData(MachineName);
	for (const FWuwaDebugAnimStateWeight& State : Data.ActiveStates)
	{
		if (State.StateName == StateName)
		{
			return State.Weight;
		}
	}
	return 0.f;
}

FString UWuwaAnimInstance::GetDebugStateMachineText(FName MachineName)
{
	const FWuwaDebugAnimStateMachine Data = GetDebugStateMachineData(MachineName);
	if (!Data.bFound)
	{
		return FString::Printf(TEXT("[%s] State machine not found"), *MachineName.ToString());
	}
	if (!Data.bInitialized)
	{
		return FString::Printf(TEXT("[%s] State machine not initialized"), *MachineName.ToString());
	}

	FString Text = FString::Printf(TEXT("[%s] Machine: %.4f%% | Current: %s\nState weights (inside this machine):"),
		*MachineName.ToString(), Data.MachineWeight * 100.f, *Data.CurrentStateName.ToString());
	if (Data.MachineWeight <= 0.f)
	{
		Text += TEXT("\nMachine inactive; internal state data may be stale.");
	}
	for (const FWuwaDebugAnimStateWeight& State : Data.ActiveStates)
	{
		Text += FString::Printf(TEXT("\n%s %s: %.4f%%"),
			State.bIsCurrentState ? TEXT("[*]") : TEXT("   "), *State.StateName.ToString(), State.Weight * 100.f);
	}
	return Text;
}

void UWuwaAnimInstance::PrintDebugStateMachine(FName MachineName, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// 实例路径隔离预览/不同角色，状态机名隔离嵌套状态机的输出。
	const FName MessageKey(*FString::Printf(TEXT("Wuwa.AnimDebug.%s.%s"), *GetPathName(), *MachineName.ToString()));
	UKismetSystemLibrary::PrintString(this, GetDebugStateMachineText(MachineName),
		true, false, FLinearColor(0.f, 0.8f, 1.f), FMath::Max(Duration, 0.f), MessageKey);
#endif
}

#pragma endregion Debug
