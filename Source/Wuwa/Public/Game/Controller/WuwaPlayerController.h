// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"
#include "GameFramework/PlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "WuwaPlayerController.generated.h"

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
	AWuwaPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetPawn(APawn* InPawn) override;
	virtual void FlushPressedKeys() override;

	/** 留空时沿用现有 Dash 指令；拆分 Sprint 动作时只需配置新的输入 Tag。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Input")
	FGameplayTag SprintInputTag;

	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaInputActionState GetSprintInputState() const;
	
	//InputMap
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UWuwaInputDataAsset* InputTagMap;
	

	//InputActions
	void ActionPressed(FGameplayTag PressedTag);
	void ActionReleased(FGameplayTag PressedTag);
	void ActionHold(FGameplayTag PressedTag);
	
	
	//ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AscComponent;
	UWuwaAbilitySystemComponent* GetASC();
	
	//InputRouter
	UFUNCTION(BlueprintPure, Category="Wuwa|Input")
	UWuwaInputRouterComponent* GetInputRouter() const { return InputRouter; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wuwa|Input")
	TObjectPtr<UWuwaInputRouterComponent> InputRouter;
	
	//InputHandler
	void  RegisterInputRouteHandlers();
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaAbilityInputHandlerComponent>AbilityInputHandler;
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaMoveInputHandler> MoveInputHandler;
	
	
	
protected:
	virtual void SetupInputComponent() override;
	
private:
	void HandleRoutedInput(
		const FInputActionInstance& Instance,
		FGameplayTag InputTag,
		FGameplayTag RouteTag,
		EWuwaInputPhase Phase);
	
};
