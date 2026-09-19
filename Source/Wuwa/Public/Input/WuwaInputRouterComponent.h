// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
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

protected:


public:	

	//注册输入到对应系统
	bool RegisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	bool UnregisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	bool DispatchInput(const FInputDataAsset& Binding,const FWuwaInputEvent& InputEvent);
	
private:
	// Router 不拥有 Handler，因此使用弱引用
	TMap<FGameplayTag, TWeakObjectPtr<UObject>> RouteHandlers;
};
