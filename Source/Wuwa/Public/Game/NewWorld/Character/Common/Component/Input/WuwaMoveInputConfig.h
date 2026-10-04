#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Game/Input/WuwaInputTypes.h"
#include "WuwaMoveInputConfig.generated.h"

class AWuwaCharacter;

/** 一次移动类指令的执行上下文：哪个输入，作用于当前控制的哪个角色。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMoveInputContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Input") FWuwaInputEvent InputEvent;
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<AWuwaCharacter> Character = nullptr;
};

/**
 * 一个移动类指令（比如切换走跑）= 一个子类；C++、C# 或蓝图都可以继承。
 * 配置对象可能被多个控制器共享：只保存参数，运行数据从 Context 取，不缓存角色。
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WUWA_API UWuwaMoveInputAction : public UObject
{
	GENERATED_BODY()
public:
	/** 执行成功返回 true；条件不满足（比如在空中不能切走跑）返回 false。 */
	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|Input")
	bool Execute(const FWuwaMoveInputContext& Context) const;
};

/** 配置表的一行：哪个语义输入、在哪个阶段触发哪个指令。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMoveInputBinding
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") FGameplayTag InputTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") EWuwaInputPhase Phase = EWuwaInputPhase::Pressed;
	/** 空对象视为配置错误，这一行不会执行。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Wuwa|Input") TObjectPtr<UWuwaMoveInputAction> Action;
};

/** 移动类输入的配置表。加一个移动类按键 = 写一个指令类 + 在这里加一行，处理器代码不变。 */
UCLASS(BlueprintType)
class WUWA_API UWuwaMoveInputConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	/** 按数组顺序取第一条 Tag 和阶段都匹配的行；没有则返回空。 */
	const FWuwaMoveInputBinding* FindBinding(const FGameplayTag& InputTag, EWuwaInputPhase Phase) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input", meta = (TitleProperty = "InputTag"))
	TArray<FWuwaMoveInputBinding> Bindings;
};
