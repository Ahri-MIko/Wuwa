#pragma once

#include "CoreMinimal.h"
#include "WuwaSkillTypes.generated.h"

class UWuwaGameplayAbilityBase;

/** 对应技能配置的 OverrideType，只描述技能能申请的战斗类别。 */
UENUM(BlueprintType)
enum class EWuwaSkillOverrideType : uint8
{
	None,
	Hit,
	Parry,
	WeaknessBreak,
	Special
};

/** C# 当前主技能的只读查询结果；实际运行记录保存在 WuwaSkill 中。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSkillData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TObjectPtr<UWuwaGameplayAbilityBase> ActiveAbility = nullptr;
	//和下面的Serial有区别,这个是记录当前的技能是谁,而Skill是负责大局也就是当前是第几个主技能切换了
	UPROPERTY(BlueprintReadOnly) int32 FightStateHandle = 0;
	UPROPERTY(BlueprintReadOnly) int32 InterruptLevel = 0;
	UPROPERTY(BlueprintReadOnly) bool bSkillAcceptInput = false;
	UPROPERTY(BlueprintReadOnly) bool bMainSkillReadyEnd = false;
	/** 动画断点/技能结束的递增编号；输入层每个新编号最多检查一批缓存。 */
	UPROPERTY(BlueprintReadOnly) int64 InputOpportunitySerial = 0;
	/** 成功开始主技能的递增编号；即使该技能在一次输入采样前已经结束也保留。 */
	UPROPERTY(BlueprintReadOnly) int64 SkillStartSerial = 0;
};
