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

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	

	//注册输入到对应系统
	bool RegisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	bool UnregisterHandler(const FGameplayTag& RouteTag,UObject* Handler);

	bool DispatchInput(const FInputDataAsset& Binding,const FWuwaInputEvent& InputEvent);

	/** 查询语义输入；不能从这里读取或指定键盘/手柄物理键。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaInputActionState GetInputActionState(FGameplayTag InputTag) const;

	/** 失焦、切换 Pawn 或重绑输入时清空；旧 Triggered 不会重新置为按住。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Input")
	void ResetInputStates();
	
private:
	void UpdateInputState(const FWuwaInputEvent& InputEvent);

	struct FHeldInput
	{
		double PressedAt = 0.0;
		// 不同 InputAction 可映射到同一个意图；释放一个来源不能解除其他来源。
		TSet<TWeakObjectPtr<const UInputAction>> ActiveSources;
	};
	TMap<FGameplayTag, FHeldInput> HeldInputs;

	// Router 不拥有 Handler，因此使用弱引用
	TMap<FGameplayTag, TWeakObjectPtr<UObject>> RouteHandlers;
};
