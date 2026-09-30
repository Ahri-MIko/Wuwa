// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "WuwaAbilityInputHandlerComponent.generated.h"

class UWuwaCombatInputRuntimeBridge;
class UWuwaAbilitySystemComponent;
class UWuwaSkillBridgeComponent;
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaAbilityInputHandlerComponent : public UActorComponent,public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWuwaAbilityInputHandlerComponent();

public:
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat|InputBuffer", meta = (ClampMin = "0"))
	float DefaultBufferLifetimeSeconds = 1.0f;

	// InputTag 对应的按下缓存时长；0 表示该指令不缓存。
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat|InputBuffer")
	TMap<FGameplayTag, float> BufferLifetimeOverrides;
	
	#pragma region InputBridge
	public:
		void ResetRuntime();

		/** 只清预输入，不清输入按住状态。空 Tag 清全部；返回实际移除数量。 */
		UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|InputBuffer")
		int32 ClearBufferedInput(FGameplayTag InputTag);

	protected:
		virtual void EndPlay(
			const EEndPlayReason::Type EndPlayReason) override;

	private:
		bool EnsureRuntime();
		UWuwaAbilitySystemComponent* GetInputASC() const;
		void BindSkillEvents(UWuwaAbilitySystemComponent* ASC);
		void UnbindSkillEvents();
		bool IsCurrentSkillEvent(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle) const;

		UFUNCTION()
		void HandleAnimBreakPoint(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle);
		UFUNCTION()
		void HandleInputCacheClear(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle, FGameplayTag InputTag);

		TWeakObjectPtr<UWuwaSkillBridgeComponent> BoundSkills;

		UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat")
		TSoftClassPtr<UWuwaCombatInputRuntimeBridge> RuntimeClass;
	
		//缓存输入类
		UPROPERTY(Transient)
		TObjectPtr<UWuwaCombatInputRuntimeBridge> Runtime;
	#pragma endregion
};
