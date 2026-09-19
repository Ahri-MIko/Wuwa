// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WuwaGameTags.h"
#include "Input/WuwaInputTypes.h"
#include "GameFramework/PlayerController.h"
#include "Input/DataAsset/WuwaInputDataAsset.h"
#include "WuwaASC/WuwaAbilitySystemComponent.h"
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
