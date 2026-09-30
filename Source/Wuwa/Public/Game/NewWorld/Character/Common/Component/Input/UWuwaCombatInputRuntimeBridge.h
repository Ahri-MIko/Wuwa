#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Input/WuwaInputTypes.h"
#include "UWuwaCombatInputRuntimeBridge.generated.h"

class UWuwaAbilitySystemComponent;

UENUM(BlueprintType)
enum class EWuwaCombatInputResult : uint8
{
	Ignored,
	NoCandidate,
	Ambiguous,
	ActivationRequested,
	AlreadyActive,
	Rejected,
	Buffered,
	NoMatchingRule,       // 该输入有配置，但当前条件都不满足
	InvalidRule          // 规则配置错误；不绕过它选择其他技能
};

UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaCombatInputRuntimeBridge : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|Combat")
	EWuwaCombatInputResult ProcessInput(UWuwaAbilitySystemComponent* ASC, FWuwaInputEvent Input, float BufferLifetimeSeconds = 0.3f);

	/** 动画断点或帧末：清理过期项，每个新机会只重新选取一次缓存。 */
	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|Combat")
	EWuwaCombatInputResult ProcessPendingInput(UWuwaAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat")
	int32 GetBufferedInputCount() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Input")
	int32 ClearBufferedInput(FGameplayTag InputTag);

	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|Combat")
	void ResetInput();
};
