using UnrealSharp;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Input;

// 对应鸣潮 InputCommandTransform：先按行顺序选招，再由 Skill/GAS 判断能否执行。
// 不持有角色状态，也不缓存目标 GA；每次输入和动画断点都重新解释原始输入。
internal static class WuwaInputCommandResolver
{
    public static EWuwaCombatInputResult SelectAbilityTag(FWuwaInputCommandContext context,
        out FGameplayTag abilityTag)
    {
        abilityTag = default;
        var config = context.Avatar.InputCommandConfig;
        bool configured = false;
        if (config.IsValid())
        {
            foreach (var rule in config.Rules)
            {
                if (!rule.InputTag.Equals(context.InputTag)) continue;
                configured = true;
                if (rule.RequiredCurrentSkillTag.IsValid
                    && !context.ASC.HasActiveSkillAbilityTag(context.CurrentSkill.ActiveAbility, rule.RequiredCurrentSkillTag)) continue;
                if (!context.ASC.MatchesOwnedTagQuery(rule.OwnerTagQuery)) continue;

                bool matched = true;
                foreach (var condition in rule.Conditions)
                {
                    // 空对象不是“无条件”；不能因为漏配而意外落入后面的兜底技能。
                    if (!condition.IsValid()) return EWuwaCombatInputResult.InvalidRule;
                    if (!condition.Evaluate(context)) { matched = false; break; }
                }
                if (!matched) continue;

                // 首命中即结束。目标缺失/冷却/打断失败都不能继续选下一行。
                abilityTag = rule.TargetAbilityTag;
                return abilityTag.IsValid ? EWuwaCombatInputResult.ActivationRequested : EWuwaCombatInputResult.InvalidRule;
            }
        }

        if (configured) return EWuwaCombatInputResult.NoMatchingRule;
        // 没有配置转换的旧输入（如 Dash）仍可直接使用技能身份 Tag。
        abilityTag = context.InputTag;
        return EWuwaCombatInputResult.ActivationRequested;
    }
}
