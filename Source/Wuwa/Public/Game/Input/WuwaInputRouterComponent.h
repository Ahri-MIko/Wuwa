// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "WuwaInputRouterComponent.generated.h"


struct FInputDataAsset;
struct FWuwaInputEvent;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaInputRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWuwaInputRouterComponent();

public:

	//注册输入到对应系统
	bool RegisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	bool UnregisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	/** 只负责按 RouteTag 把事件交给对应系统；按键的按住状态记录在角色的输入意图组件里。 */
	bool DispatchInput(const FWuwaInputEvent& InputEvent);

private:

	// Router 不拥有 Handler，因此使用弱引用
	//存储Handler标签对应的实际处理类
	TMap<FGameplayTag, TWeakObjectPtr<UObject>> RouteHandlers;
};
