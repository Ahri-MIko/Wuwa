// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"
#include "GameFramework/PlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "WuwaPlayerController.generated.h"

class UInputMappingContext;
class UWuwaMoveInputHandler;
struct FInputActionInstance;
class UWuwaAbilityInputHandlerComponent;

class UWuwaInputRouterComponent;
/**
 * 
 */
UCLASS()
class WUWA_API AWuwaPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	
#pragma region Lifecycle
	//类声明周期事件
	AWuwaPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	
	//输入上下文失效
	/*Possess(新角色) 这个函数通常用来清理之前角色遗留下来的状态
     └ OnPossess
		├ 如果已经控制着一个角色 → 先 UnPossess() → SetPawn(nullptr)   ← 第 1 次调用
		├ 新角色->PossessedBy(this)
		└ SetPawn(新角色)                                              ← 第 2 次调用*/
	virtual void SetPawn(APawn* InPawn) override;
	//游戏窗口失焦后会导致操作一直按住没有松开
	virtual void FlushPressedKeys() override;
	
#pragma endregion
	//InputMap
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UWuwaInputDataAsset* InputTagMap;
	
	//ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AscComponent;
	UWuwaAbilitySystemComponent* GetASC();
	
#pragma  region IMC注册输入
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	void RegisterMappingContext() const;

#pragma  endregion
	
#pragma region InputRouter and Handlers
	//InputRouter 中转组件引用
	UFUNCTION(BlueprintPure, Category="Wuwa|Input")
	UWuwaInputRouterComponent* GetInputRouter() const { return InputRouter; }
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wuwa|Input")
	TObjectPtr<UWuwaInputRouterComponent> InputRouter;
	
	//InputHandler 注册所有模块的输入中转类
	void  RegisterInputRouteHandlers();
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaAbilityInputHandlerComponent>AbilityInputHandler;
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaMoveInputHandler> MoveInputHandler;

#pragma endregion
	
protected:
	//注册所有的Action事件,按下对应按键后会将
	virtual void SetupInputComponent() override;
	
private:
	void HandleRoutedInput(const FInputActionInstance& Instance,FGameplayTag InputTag,FGameplayTag RouteTag,EWuwaInputPhase Phase);
	
};
