#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaInputCommandConfig.generated.h"

class UWuwaAbilitySystemComponent;
class AWuwaCharacter;

/** 一次选招的只读上下文。Condition 不得缓存角色引用或修改技能/输入状态。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommandContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UWuwaAbilitySystemComponent> ASC = nullptr;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AWuwaCharacter> Avatar = nullptr;
	UPROPERTY(BlueprintReadOnly) FWuwaSkillData CurrentSkill;
	UPROPERTY(BlueprintReadOnly) FGameplayTag InputTag;
};

/** 配置对象可能被多个角色共享；只保存参数，运行数据从 Context 查询。 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WUWA_API UWuwaInputCondition : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat|Input")
	bool Evaluate(const FWuwaInputCommandContext& Context) const;
};

/** 仅解释 Pressed 输入。按数组顺序首命中，不按 InterruptLevel 排序。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommandRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RuleName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag InputTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag TargetAbilityTag;
	/** 空 = 不限制当前主技能；非空 = 精确匹配当前执行 Spec 的身份 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag RequiredCurrentSkillTag;
	/** 空 = 不限制角色 Tag。查询 ASC 拥有的 Tag，而不是 GA 身份 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagQuery OwnerTagQuery;
	/** 所有条件都满足才选中此行；空数组 = 无附加限制，空对象 = 配置错误。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced) TArray<TObjectPtr<UWuwaInputCondition>> Conditions;
};

UCLASS(BlueprintType)
class WUWA_API UWuwaInputCommandConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Input", meta = (TitleProperty = "RuleName"))
	TArray<FWuwaInputCommandRule> Rules;
};
