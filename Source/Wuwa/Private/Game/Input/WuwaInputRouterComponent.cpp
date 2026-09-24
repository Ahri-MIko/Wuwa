// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Input/WuwaInputRouterComponent.h"

#include "GameplayTagContainer.h"
#include "Game/Input/WuwaInputRouteHandler.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Engine/World.h"

//创建了一个名叫 LogWuwaInputRouter 的日志分类，之后这个文件里就可以这样打印日志
DEFINE_LOG_CATEGORY_STATIC(LogWuwaInputRouter, Log, All);

//没有名字的 namespace { } 叫匿名命名空间。写在里面的函数只在当前这个 .cpp 文件里可见，其他 .cpp 文件看不到，也调用不到。
namespace
{
	bool IsValidRouteHandler(const UObject* Handler)
	{
		return IsValid(Handler)
			&& Handler->GetClass()->ImplementsInterface(
				UWuwaInputRouteHandler::StaticClass());
	}
}
// Sets default values for this component's properties
UWuwaInputRouterComponent::UWuwaInputRouterComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	
}

bool UWuwaInputRouterComponent::RegisterHandler(const FGameplayTag& RouteTag,UObject* Handler)
{
	check(IsInGameThread());

	//合法性检查
	if (!RouteTag.IsValid())
	{
		UE_LOG(LogWuwaInputRouter,Warning,TEXT("RegisterHandler failed: RouteTag is invalid."));
		return false;
	}
	if (!IsValidRouteHandler(Handler))
	{
		UE_LOG(LogWuwaInputRouter,Warning,TEXT("%s does not implement WuwaInputRouteHandler."),*GetNameSafe(Handler));
		return false;
	}

	if (TWeakObjectPtr<UObject>* Existing =
		RouteHandlers.Find(RouteTag))
	{
		UObject* ExistingHandler = Existing->Get();

		// 原 Handler 已销毁，允许新 Handler 接管
		if (!IsValidRouteHandler(ExistingHandler))
		{
			RouteHandlers.Remove(RouteTag);
		}
		else if (ExistingHandler == Handler)
		{
			// 重复注册同一个对象，按成功处理
			return true;
		}
		else
		{
			// 不允许静默覆盖另一个有效 Handler
			UE_LOG(
				LogWuwaInputRouter,
				Error,
				TEXT("Route [%s] is already owned by [%s]. "
					 "Registration from [%s] was rejected."),
				*RouteTag.ToString(),
				*GetNameSafe(ExistingHandler),
				*GetNameSafe(Handler));

			return false;
		}
	}

	RouteHandlers.Emplace(
		RouteTag,
		TWeakObjectPtr<UObject>(Handler));

	return true;
}

//把某个 RouteTag 上登记的 Handler 移除，但只允许"当前登记的那个 Handler"自己移除自己。
bool UWuwaInputRouterComponent::UnregisterHandler(const FGameplayTag& RouteTag,UObject* Handler)
{
	check(IsInGameThread());

	if (!RouteTag.IsValid())
	{
		return false;
	}

	TWeakObjectPtr<UObject>* Existing =
		RouteHandlers.Find(RouteTag);

	if (!Existing)
	{
		return false;
	}

	UObject* ExistingHandler = Existing->Get();

	// Map 中只剩无效弱引用，直接清理
	if (!IsValidRouteHandler(ExistingHandler))
	{
		RouteHandlers.Remove(RouteTag);
		return true;
	}

	// 防止旧角色注销时把新角色的 Handler 删除
	if (ExistingHandler != Handler)
	{
		UE_LOG(LogWuwaInputRouter,Warning,TEXT("[%s] tried to unregister Route [%s], ""but its current owner is [%s]."),*GetNameSafe(Handler),*RouteTag.ToString(),*GetNameSafe(ExistingHandler));
		return false;
	}

	RouteHandlers.Remove(RouteTag);
	return true;
}


bool UWuwaInputRouterComponent::DispatchInput(const FInputDataAsset& Binding,const FWuwaInputEvent& InputEvent)
{
	check(IsInGameThread());

	if (!Binding.IsConfigured())
	{
		UE_LOG(LogWuwaInputRouter,Warning,TEXT("Dispatch rejected: invalid input binding."));
		return false;
	}

	// 先记录语义输入，再交给系统处理。GA 拒绝激活或没有 Handler 不能吞掉松开事件。
	FWuwaInputEvent RoutedEvent = InputEvent;
	RoutedEvent.InputTag = Binding.InputTag;
	RoutedEvent.SourceAction = Binding.InputAction;
	if ((!FMath::IsFinite(RoutedEvent.Timestamp) || RoutedEvent.Timestamp <= 0.0) && GetWorld())
	{
		RoutedEvent.Timestamp = GetWorld()->GetTimeSeconds();
	}
	UpdateInputState(RoutedEvent);

	const TWeakObjectPtr<UObject>* FoundHandler =
		RouteHandlers.Find(Binding.RouteTag);

	if (!FoundHandler)
	{
		UE_LOG(LogWuwaInputRouter,Verbose,TEXT("No handler registered for Route [%s]."),*Binding.RouteTag.ToString());
		return false;
	}

	// 调用接口前复制弱指针。
	// Handler 可以在回调中注销自己，不会让 Map 指针失效。
	const TWeakObjectPtr<UObject> HandlerWeak =
		*FoundHandler;

	UObject* Handler = HandlerWeak.Get();

	if (!IsValidRouteHandler(Handler))
	{
		// 仅当 Map 中还是刚才那个失效对象时才删除
		if (const TWeakObjectPtr<UObject>* Current =
			RouteHandlers.Find(Binding.RouteTag))
		{
			if (Current->HasSameIndexAndSerialNumber(
				HandlerWeak))
			{
				RouteHandlers.Remove(Binding.RouteTag);
			}
		}

		return false;
	}

	return IWuwaInputRouteHandler::
		Execute_HandleWuwaInput(
			Handler,
			RoutedEvent);
}

void UWuwaInputRouterComponent::UpdateInputState(const FWuwaInputEvent& InputEvent)
{
	if (!InputEvent.InputTag.IsValid() || !IsValid(InputEvent.SourceAction) || !GetWorld())
	{
		return;
	}

	const TWeakObjectPtr<const UInputAction> Source(InputEvent.SourceAction.Get());
	if (InputEvent.Phase == EWuwaInputPhase::Pressed)
	{
		FHeldInput& State = HeldInputs.FindOrAdd(InputEvent.InputTag);
		for (auto It = State.ActiveSources.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
		if (State.ActiveSources.IsEmpty())
		{
			State.PressedAt = FMath::Clamp(InputEvent.Timestamp, 0.0, GetWorld()->GetTimeSeconds());
		}
		State.ActiveSources.Add(Source);
	}
	else if (InputEvent.Phase == EWuwaInputPhase::Released || InputEvent.Phase == EWuwaInputPhase::Canceled)
	{
		if (FHeldInput* State = HeldInputs.Find(InputEvent.InputTag))
		{
			State->ActiveSources.Remove(Source);
			if (State->ActiveSources.IsEmpty())
			{
				HeldInputs.Remove(InputEvent.InputTag);
			}
		}
	}
	// Triggered 只表达持续采样，不重新计时，也不让 Flush/Cancel 后的输入复活。
}

FWuwaInputActionState UWuwaInputRouterComponent::GetInputActionState(FGameplayTag InputTag) const
{
	FWuwaInputActionState Result;
	const FHeldInput* State = HeldInputs.Find(InputTag);
	if (!State || !GetWorld())
	{
		return Result;
	}
	for (const auto& Source : State->ActiveSources)
	{
		if (Source.IsValid())
		{
			Result.bHeld = true;
			Result.HeldSeconds = static_cast<float>(FMath::Max(0.0, GetWorld()->GetTimeSeconds() - State->PressedAt));
			break;
		}
	}
	return Result;
}

void UWuwaInputRouterComponent::ResetInputStates()
{
	HeldInputs.Empty();
}

void UWuwaInputRouterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetInputStates();
	Super::EndPlay(EndPlayReason);
}
