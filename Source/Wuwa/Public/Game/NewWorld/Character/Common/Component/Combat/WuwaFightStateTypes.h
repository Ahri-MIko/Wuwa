#pragma once

#include "CoreMinimal.h"
#include "WuwaFightStateTypes.generated.h"

/** 战斗类别的顺序同时表示覆盖关系；不是位置、步态或某个具体技能。 */
UENUM(BlueprintType)
enum class EWuwaFightState : uint8
{
	None = 0,
	Skill = 1 UMETA(DisplayName = "普通技能"),
	Hit = 2 UMETA(DisplayName = "普通受击"),
	SkillOverrideHit = 3 UMETA(DisplayName = "覆盖受击技能"),
	ParryHit = 4 UMETA(DisplayName = "被弹反受击"),
	SkillOverrideParry = 5 UMETA(DisplayName = "覆盖被弹反技能"),
	WeaknessBreak = 6 UMETA(DisplayName = "被破弱"),
	SkillOverrideWeaknessBreak = 7 UMETA(DisplayName = "覆盖被破弱技能"),
	Captured = 8 UMETA(DisplayName = "抓取"),
	SpecialSkill = 9 UMETA(DisplayName = "特殊技能"),
	StateMachine = 10 UMETA(DisplayName = "状态机主状态")
};

/** 角色当前的战斗占用快照；Handle 为 0 表示空闲。暂不承担网络复制。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaFightStateData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	EWuwaFightState State = EWuwaFightState::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	int32 SubStatePriority = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	int32 Handle = 0;
};
