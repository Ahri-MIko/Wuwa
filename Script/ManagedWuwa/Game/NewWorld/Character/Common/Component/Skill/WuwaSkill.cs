using UnrealSharp;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Skill;

/// <summary>一次主技能执行的运行记录。不是组件，也不是 GA 子类。</summary>
internal sealed class WuwaSkill(UWuwaGameplayAbilityBase ability, int interruptLevel, int fightStateHandle)
{
    public TWeakObjectPtr<UWuwaGameplayAbilityBase> ActiveAbility { get; } = new(ability);
    public int InterruptLevel { get; set; } = interruptLevel;
    public int FightStateHandle { get; } = fightStateHandle;
}
