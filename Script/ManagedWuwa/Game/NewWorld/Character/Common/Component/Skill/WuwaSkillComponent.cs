using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;
using ManagedWuwa.Game.NewWorld.Input;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Skill;

/// <summary>
/// 轻量主技能管理：判断能否让位、结束旧 GA、申请 FightState、维护当前 Skill。
/// GA 执行任务，FightState 判断战斗类别；本组件不直接修改移动状态或播放动画。
/// </summary>
[UClass]
public partial class UWuwaSkillComponent : UWuwaSkillBridgeComponent
{
    private WuwaSkill? _currentSkill;
    private bool _skillAcceptInput;
    private bool _mainSkillReadyEnd;
    private bool _switching;
    private bool _endingPlay;
    private readonly HashSet<(int MontageInstanceId, int NotifyEventId)> _acceptInputWindows = new();
    private long _inputOpportunitySerial;
    private long _skillStartSerial;
    private bool AcceptsSkillInput => _skillAcceptInput || _acceptInputWindows.Count > 0;

    /*获取玩家身上的Fight组件*/
    private UWuwaFightStateBridgeComponent? GetFightState()
    {
        if (Owner is AWuwaCharacter character && character.IsValid())
            return character.FightStateComponent;
        else
        {
            return null;
        }
    }
    
    protected override bool CanBeginSkill_Implementation(UWuwaGameplayAbilityBase ability)
    {
        var fight = GetFightState();
        if (_endingPlay || _switching || !ability.IsValid() || !ability.IsMainSkill || ability.InterruptLevel is < 0 or > 255 || fight is null || !fight.IsValid() || !TryGetFightCategory(ability.SkillOverrideType, out var category))
        {
            return false;
        }

        if (_currentSkill is { } current)
        {
            var active = current.ActiveAbility.Object;
            if (active is null || !active.IsValid() || !active.CanEndSkillExecutionNow() || !(ability.InterruptLevel > current.InterruptLevel || ability.InterruptLevel == current.InterruptLevel && AcceptsSkillInput || _mainSkillReadyEnd))
            {
                return false;
            }

            // 旧主技能会先结束并释放自己的 FightState，不能让其等级再次否决 ReadyEnd。
            // 外部受击/覆盖已换成另一个句柄时，仍必须接受那个新状态的裁决。
            if (fight.StateData.Handle == current.FightStateHandle)
            {
                return true;
            }
        }

        return fight.CheckSwitchState(category, ability.InterruptLevel);
    }

    protected override int TryBeginSkill_Implementation(UWuwaGameplayAbilityBase ability)
    {
        if (!CanBeginSkill(ability) || !ability.IsSkillExecutionFor(Owner)) return 0;

        _switching = true;
        try
        {
            if (_currentSkill is { } previous)
            {
                var oldAbility = previous.ActiveAbility.Object;
                // ScopeLock 中不强行交接；必须等旧 GA 的真实 EndAbility 回报清掉记录。
                if (oldAbility is null || !oldAbility.IsValid() || !oldAbility.TryEndSkillExecution(previous.FightStateHandle)
                    || _currentSkill is not null)
                {
                    return 0;
                }
            }

            var fight = GetFightState();
            if (_endingPlay || !ability.IsValid() || !ability.IsSkillExecutionFor(Owner)
                || fight is null || !fight.IsValid() || !TryGetFightCategory(ability.SkillOverrideType, out var category))
            {
                return 0;
            }

            int handle = fight.TrySwitchState(category, ability.InterruptLevel);
            if (handle == 0) return 0;

            // FightState 广播可能结束正在启动的 GA；失败必须归还刚取得的句柄。
            if (_endingPlay || !ability.IsValid() || !ability.IsSkillExecutionFor(Owner))
            {
                fight.ExitState(handle);
                return 0;
            }

            _currentSkill = new WuwaSkill(ability, ability.InterruptLevel, handle);
            ++_skillStartSerial;
            _skillAcceptInput = false;
            _mainSkillReadyEnd = false;
            _acceptInputWindows.Clear();
            return handle;
        }
        finally
        {
            _switching = false;
        }
    }

    protected override bool EndSkill_Implementation(int fightStateHandle)
    {
        if (!OwnsSkill(fightStateHandle)) return false;
        string before = CombatInputTrace.Enabled ? CombatInputTrace.Context(this) : string.Empty;

        // InstancedPerExecution 的 GA 在 Super.EndAbility 后可能已经无效；按句柄清理。
        _currentSkill = null;
        _skillAcceptInput = false;
        _mainSkillReadyEnd = false;
        _acceptInputWindows.Clear();
        var fight = GetFightState();
        if (fight is not null && fight.IsValid()) fight.ExitState(fightStateHandle);
        // 输入层在安全的帧边界消费，不在 EndAbility 清理调用栈中再次激活。
        ++_inputOpportunitySerial;
        if (CombatInputTrace.Enabled)
            CombatInputTrace.Write("SKILL_END",
                $"endedHandle={fightStateHandle} before=[{before}] after=[{CombatInputTrace.Context(this)}]");
        return true;
    }

    protected override FWuwaSkillData GetCurrentSkillData_Implementation()
    {
        return _currentSkill is { } current ? new FWuwaSkillData
        {
            // 反射 UObject 字段允许空指针，生成绑定未标注可空；不延长已结束 GA 的生命。
            ActiveAbility = current.ActiveAbility.Object!,
            FightStateHandle = current.FightStateHandle,
            InterruptLevel = current.InterruptLevel,
            SkillAcceptInput = AcceptsSkillInput,
            MainSkillReadyEnd = _mainSkillReadyEnd,
            InputOpportunitySerial = _inputOpportunitySerial,
            SkillStartSerial = _skillStartSerial
        } : new FWuwaSkillData { InputOpportunitySerial = _inputOpportunitySerial, SkillStartSerial = _skillStartSerial };
    }

    protected override bool SetSkillAcceptInput_Implementation(int fightStateHandle, bool acceptInput)
    {
        if (_endingPlay || !OwnsSkill(fightStateHandle)) return false;
        if (acceptInput && !_skillAcceptInput) ++_inputOpportunitySerial;
        _skillAcceptInput = acceptInput;
        return true;
    }

    protected override bool SetMainSkillReadyEnd_Implementation(int fightStateHandle, bool readyEnd)
    {
        if (_endingPlay || !OwnsSkill(fightStateHandle)) return false;
        if (readyEnd && !_mainSkillReadyEnd) ++_inputOpportunitySerial;
        _mainSkillReadyEnd = readyEnd;
        return true;
    }

    protected override bool BeginSkillAcceptInputWindow_Implementation(int fightStateHandle, int montageInstanceId, int notifyEventId)
    {
        if (_endingPlay || !OwnsSkill(fightStateHandle) || montageInstanceId < 0 || notifyEventId < 0) return false;
        if (!_acceptInputWindows.Add((montageInstanceId, notifyEventId))) return false;
        ++_inputOpportunitySerial;
        return true;
    }

    protected override bool EndSkillAcceptInputWindow_Implementation(int montageInstanceId, int notifyEventId)
    {
        // 老播放的 End 只能移除老 key；不能关闭新播放或另一个仍在重叠的窗口。
        return _acceptInputWindows.Remove((montageInstanceId, notifyEventId));
    }

    protected override bool SetSkillInterruptLevel_Implementation(int fightStateHandle, int newLevel)
    {
        if (_endingPlay || !OwnsSkill(fightStateHandle) || newLevel is < 0 or > 255) return false;
        _currentSkill!.InterruptLevel = newLevel;
        return true;
    }

    protected override bool CallAnimBreakPoint_Implementation(int fightStateHandle)
    {
        if (CombatInputTrace.Enabled)
            CombatInputTrace.Write("BREAKPOINT_ENTER",
                $"requestedHandle={fightStateHandle} {CombatInputTrace.Context(this)}");
        if (_endingPlay || _switching || !OwnsSkill(fightStateHandle))
        {
            if (CombatInputTrace.Enabled)
                CombatInputTrace.Write("BREAKPOINT_REJECT",
                    $"requestedHandle={fightStateHandle} reason=component-or-owner endingPlay={_endingPlay} switching={_switching} ownsHandle={OwnsSkill(fightStateHandle)} {CombatInputTrace.Context(this)}");
            return false;
        }
        var ability = _currentSkill!.ActiveAbility.Object;
        if (ability is null || !ability.IsValid() || !ability.IsSkillExecutionFor(Owner))
        {
            if (CombatInputTrace.Enabled)
                CombatInputTrace.Write("BREAKPOINT_REJECT",
                    $"requestedHandle={fightStateHandle} reason=invalid-or-inactive-ability-or-avatar {CombatInputTrace.Context(this)}");
            return false;
        }

        ++_inputOpportunitySerial;
        // 通知处同步检查，短窗口不会等到关闭后才消费。GAS ScopeLock 中只记录机会，帧末再处理。
        // 广播可以同步结束本技能：所有状态写入必须在广播之前完成。
        if (ability.CanEndSkillExecutionNow())
        {
            if (CombatInputTrace.Enabled)
                CombatInputTrace.Write("BREAKPOINT_BROADCAST",
                    $"requestedHandle={fightStateHandle} {CombatInputTrace.Context(this)}");
            BroadcastAnimBreakPoint(fightStateHandle);
        }
        else if (CombatInputTrace.Enabled)
        {
            CombatInputTrace.Write("BREAKPOINT_DEFER",
                $"requestedHandle={fightStateHandle} reason=scope-lock wait=post-update-tick {CombatInputTrace.Context(this)}");
        }
        return true;
    }

    protected override bool RequestInputCacheClear_Implementation(int fightStateHandle, FGameplayTag inputTag)
    {
        if (_endingPlay || _switching || !OwnsSkill(fightStateHandle)) return false;
        var ability = _currentSkill!.ActiveAbility.Object;
        if (ability is null || !ability.IsValid() || !ability.IsSkillExecutionFor(Owner)) return false;
        BroadcastInputCacheClear(fightStateHandle, inputTag);
        return true;
    }

    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        if (_currentSkill is { } current)
        {
            var ability = current.ActiveAbility.Object;
            if (ability is not null && ability.IsValid()) ability.TryEndSkillExecution(current.FightStateHandle);
            EndSkill(current.FightStateHandle);
        }
        base.EndPlay(endPlayReason);
    }

    private bool OwnsSkill(int handle) => handle > 0 && _currentSkill?.FightStateHandle == handle;

    private static bool TryGetFightCategory(EWuwaSkillOverrideType type, out EWuwaFightState category)
    {
        category = type switch
        {
            EWuwaSkillOverrideType.None => EWuwaFightState.Skill,
            EWuwaSkillOverrideType.Hit => EWuwaFightState.SkillOverrideHit,
            EWuwaSkillOverrideType.Parry => EWuwaFightState.SkillOverrideParry,
            EWuwaSkillOverrideType.WeaknessBreak => EWuwaFightState.SkillOverrideWeaknessBreak,
            EWuwaSkillOverrideType.Special => EWuwaFightState.SpecialSkill,
            _ => EWuwaFightState.None
        };
        return category != EWuwaFightState.None;
    }
}
