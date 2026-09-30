using System.Globalization;
using UnrealSharp.CoreUObject;
using UnrealSharp.Log;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Input;

// 临时诊断开关。只在输入、缓存变化和技能断点打印，不逐帧刷屏。
internal static class CombatInputTrace
{
    public static bool Enabled { get; set; } = true;

    public static void Write(string stage, FormattableString details)
    {
        if (Enabled)
            UnrealLogger.Log("LogCombatBuffer", $"[{stage}] {details.ToString(CultureInfo.InvariantCulture)}");
    }

    public static string Name(UObject? value) => value is not null && value.IsValid() ? value.Name.ToString() : "None";

    public static string Describe(FWuwaSkillData state)
    {
        var ability = state.ActiveAbility;
        bool canEnd = ability is not null && ability.IsValid() && ability.CanEndSkillExecutionNow();
        return FormattableString.Invariant($"skill={Name(ability)} handle={state.FightStateHandle} ready={state.MainSkillReadyEnd} accept={state.SkillAcceptInput} priority={state.InterruptLevel} canEnd={canEnd} start={state.SkillStartSerial} opportunity={state.InputOpportunitySerial}");
    }

    public static string Context(UWuwaAbilitySystemComponent asc)
    {
        if (!asc.IsValid()) return "asc=None";
        var avatar = asc.InputAvatar;
        return avatar.IsValid() ? Context(avatar.SkillComponent) : "avatar=None";
    }

    public static string Context(UWuwaSkillBridgeComponent skills)
    {
        if (!skills.IsValid()) return "skills=None";
        var character = skills.Owner as AWuwaCharacter;
        var asc = character is not null && character.IsValid() ? character.AbilitySystemComponent as UWuwaAbilitySystemComponent : null;
        double now = asc is not null && asc.IsValid() ? asc.InputTimeSeconds : -1;
        return FormattableString.Invariant($"t={now:F3} avatar={Name(character)} {Describe(skills.GetCurrentSkillData())}");
    }
}
