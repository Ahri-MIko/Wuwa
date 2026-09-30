using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Input;

[UClass]
public partial class UWuwaCombatInputRuntime : UWuwaCombatInputRuntimeBridge
{
    private readonly CombatInputBuffer _buffer = new();
    private TWeakObjectPtr<UWuwaAbilitySystemComponent> _asc;
    private TWeakObjectPtr<AWuwaCharacter> _avatar;
    private long _observedSkillStart;
    private long _observedOpportunity;
    private double _lastTime;
    private bool _processing;
    private long _inputGeneration;

    private readonly record struct Candidate(FGameplayAbilitySpecHandle Handle, UWuwaGameplayAbilityBase Definition);

    //大多都是防止重入的操作
    public override EWuwaCombatInputResult ProcessInput(UWuwaAbilitySystemComponent asc, FWuwaInputEvent input, float bufferLifetimeSeconds)
    {
        //检查是否没有其他处理Input的函数正在执行,并且当前的Input是Pressed阶段
        if (_processing || input.Phase != EWuwaInputPhase.Pressed || !input.InputTag.IsValid)
        {
            if (input.Phase == EWuwaInputPhase.Pressed)
                CombatInputTrace.Write("PRESS_IGNORED", $"runtime={Name} input={input.InputTag} reentrant={_processing} validTag={input.InputTag.IsValid}");
            return EWuwaCombatInputResult.Ignored;
        }
        _processing = true;//上锁
        try
        {
            //读取当前的技能信息
            if (!ReadContext(asc, out var skills, out var state, out double now)) return EWuwaCombatInputResult.Ignored;
            Trace("PRESS", now, state, $"input={input.InputTag} timestamp={input.Timestamp:F3} lifetime={bufferLifetimeSeconds:F3}s");
            long generation = _inputGeneration;
            int fightHandle = asc.InputAvatar.FightStateComponent.StateData.Handle;
            var resolution = Resolve(asc, state, input.InputTag, out var candidate);
            if (!IsContextCurrent(asc, skills!, state, generation, fightHandle))
            {
                Trace("CONTEXT_CHANGED", now, state, $"during=PressResolve input={input.InputTag}");
                return EWuwaCombatInputResult.Ignored;
            }
            // 已配置但暂时没有条件命中，也保存原始意图，断点时可能已满足派生条件。
            if (resolution == EWuwaCombatInputResult.NoMatchingRule)
                return Store(input, now, bufferLifetimeSeconds, state, "NoMatchingRule") ? EWuwaCombatInputResult.Buffered : resolution;
            if (resolution != EWuwaCombatInputResult.ActivationRequested)
            {
                Trace("RESOLVE_REJECT", now, state, $"input={input.InputTag} result={resolution}");
                return resolution;
            }
            bool canCreate = CanCreateCommand(asc, skills!, candidate, out var gate);
            Trace("RESOLVE", now, state, $"input={input.InputTag} target={candidate.Definition.OriginalTag} ga={CombatInputTrace.Name(candidate.Definition)} gate={gate}");
            if (!IsContextCurrent(asc, skills!, state, generation, fightHandle))
            {
                Trace("CONTEXT_CHANGED", now, state, $"during=PressEligibility input={input.InputTag}");
                return EWuwaCombatInputResult.Ignored;
            }
            if (canCreate) return Submit(asc, candidate, now, state, "Pressed");

            // 缓存原始按下输入，不保存此刻解析出的 GA 或 Spec。
            return candidate.Definition.IsMainSkill && Store(input, now, bufferLifetimeSeconds, state, gate)
                ? EWuwaCombatInputResult.Buffered : EWuwaCombatInputResult.Rejected;
        }
        finally { _processing = false; }
    }

    public override EWuwaCombatInputResult ProcessPendingInput(UWuwaAbilitySystemComponent asc)
    {
        //同一帧的事件触发多次可能会造成重复触发
        if (_processing)
        {
            CombatInputTrace.Write("CHECK_DEFER", $"runtime={Name} reason=RuntimeReentrant count={_buffer.Count}");
            return EWuwaCombatInputResult.Ignored;
        }
        _processing = true;
        try
        {
            if (!ReadContext(asc, out var skills, out var state, out double now)) return EWuwaCombatInputResult.Ignored;
            // 通知事件和帧末共用此入口；同一个机会只处理一次，不轮询重试激活。
            if (_observedOpportunity == state.InputOpportunitySerial) return EWuwaCombatInputResult.Ignored;
            Trace("CHECK", now, state, $"previousOpportunity={_observedOpportunity} count={_buffer.Count}");
            _observedOpportunity = state.InputOpportunitySerial;
            long generation = _inputGeneration;
            int fightHandle = asc.InputAvatar.FightStateComponent.StateData.Handle;

            Candidate? best = null;
            foreach (var input in _buffer.Snapshot(now))
            {
                var resolution = Resolve(asc, state, input.InputTag, out var candidate);
                if (!IsContextCurrent(asc, skills!, state, generation, fightHandle))
                {
                    Trace("CONTEXT_CHANGED", now, state, $"during=PendingResolve input={input.InputTag}");
                    return EWuwaCombatInputResult.Ignored;
                }
                if (resolution != EWuwaCombatInputResult.ActivationRequested)
                {
                    Trace("CANDIDATE_REJECT", now, state, $"input={input.InputTag} result={resolution}");
                    continue;
                }
                bool canCreate = CanCreateCommand(asc, skills!, candidate, out var gate);
                Trace("CANDIDATE", now, state, $"input={input.InputTag} target={candidate.Definition.OriginalTag} ga={CombatInputTrace.Name(candidate.Definition)} gate={gate} remaining={input.ExpiresAtSeconds - now:F3}s");
                if (!IsContextCurrent(asc, skills!, state, generation, fightHandle))
                {
                    Trace("CONTEXT_CHANGED", now, state, $"during=PendingEligibility input={input.InputTag}");
                    return EWuwaCombatInputResult.Ignored;
                }
                if (!canCreate) continue;

                // 严格大于：同优先级保留较早到达的输入。
                if (best is null || candidate.Definition.InterruptLevel > best.Value.Definition.InterruptLevel)
                    best = candidate;
            }
            if (best is { } selected) return Submit(asc, selected, now, state, "Buffered");
            Trace("NO_COMMAND", now, state, $"reason={(_buffer.Count == 0 ? "BufferEmpty" : "NoEligibleCandidate")}");
            return EWuwaCombatInputResult.Ignored;
        }
        finally { _processing = false; }
    }

    protected override int GetBufferedInputCount_Implementation() => _buffer.Count;

    protected override int ClearBufferedInput_Implementation(FGameplayTag inputTag)
    {
        ++_inputGeneration;
        int removed = _buffer.Remove(inputTag);
        CombatInputTrace.Write("CLEAR_REQUEST", $"runtime={Name} t={_lastTime:F3} tag={inputTag} removed={removed} remaining={_buffer.Count}");
        return removed;
    }

    
    public override void ResetInput()
    {
        if (_buffer.Count > 0 || _asc.Object is not null)
            CombatInputTrace.Write("RESET", $"runtime={Name} t={_lastTime:F3} reason=InputContextReset dropping={_buffer.Count} cache={_buffer.Describe(_lastTime)}");
        //在这之前的所有缓存和判断结果,全部作废 因为编号加一
        ++_inputGeneration;
        _buffer.Clear();
        _asc = default;
        _avatar = default;
        _observedSkillStart = 0;
        _observedOpportunity = 0;
        _lastTime = 0;
    }

    private bool ReadContext(UWuwaAbilitySystemComponent asc, out UWuwaSkillBridgeComponent? skills, out FWuwaSkillData state, out double now)
    {
        skills = null;
        state = default;
        now = 0;
        
        if (!asc.IsValid()) { ResetInput(); return false; }
        var avatar = asc.InputAvatar;
        if (!avatar.IsValid() || !avatar.SkillComponent.IsValid() || !avatar.FightStateComponent.IsValid()) { ResetInput(); return false; }
        skills = avatar.SkillComponent;
        state = skills.GetCurrentSkillData();
        now = asc.InputTimeSeconds;
        if (!double.IsFinite(now) || now < 0) { ResetInput(); return false; }

        if (_asc != new TWeakObjectPtr<UWuwaAbilitySystemComponent>(asc) || _avatar != new TWeakObjectPtr<AWuwaCharacter>(avatar) || now < _lastTime)
        {
            ResetInput();
            _asc = new(asc);
            _avatar = new(avatar);
            _observedOpportunity = state.InputOpportunitySerial;
            Trace("CONTEXT", now, state, $"traceVersion=1 asc={CombatInputTrace.Name(asc)}");
        }

        // 新技能即使在两次采样之间已经开始并结束，也必须使旧批次失效。
        // 原技能正常结束不增加 StartSerial，仍允许在结束断点消费。
        if (state.SkillStartSerial != _observedSkillStart)
        {
            if (_buffer.Count > 0) Trace("CLEAR", now, state, $"reason=NewSkill previousStart={_observedSkillStart} dropping={_buffer.Count}");
            _buffer.Clear();
        }
        _observedSkillStart = state.SkillStartSerial;
        _lastTime = now;
        if (CombatInputTrace.Enabled && _buffer.Count > 0)
        {
            double pruneTime = now;
            FWuwaSkillData pruneState = state;
            _buffer.Prune(now, item => Trace("EXPIRE", pruneTime, pruneState, $"input={item.InputTag} pressed={item.InputTimeSeconds:F3} expires={item.ExpiresAtSeconds:F3} overdue={pruneTime - item.ExpiresAtSeconds:F3}s"));
        }
        else _buffer.Prune(now);

        // 外部受击等接管后，不继续执行上一技能留下的预输入。
        var fight = avatar.FightStateComponent;
        if (fight.IsValid() && fight.StateData.Handle != 0 && fight.StateData.Handle != state.FightStateHandle)
        {
            if (_buffer.Count > 0) Trace("CLEAR", now, state, $"reason=ExternalFightState fightHandle={fight.StateData.Handle} dropping={_buffer.Count}");
            _buffer.Clear();
        }
        return true;
    }

    //把一次按键翻译成"要放哪个技能"
    private static EWuwaCombatInputResult Resolve(UWuwaAbilitySystemComponent asc, FWuwaSkillData state, FGameplayTag inputTag,
        out Candidate candidate)
    {
        candidate = default;
        var context = new FWuwaInputCommandContext { ASC = asc, Avatar = asc.InputAvatar, CurrentSkill = state, InputTag = inputTag };
        var resolution = WuwaInputCommandResolver.SelectAbilityTag(context, out var abilityTag);
        if (resolution != EWuwaCombatInputResult.ActivationRequested) return resolution;
        var handles = asc.FindAbilityHandlesByAbilityTag(abilityTag);
        if (handles.Count == 0) return EWuwaCombatInputResult.NoCandidate;
        // 一个技能身份必须精确定位一个已授予 Spec，不能猜测重复配置的目标。
        if (handles.Count != 1) return EWuwaCombatInputResult.Ambiguous;
        var definition = asc.GetAbilityForInput(handles[0]);
        if (!definition.IsValid()) return EWuwaCombatInputResult.NoCandidate;
        candidate = new(handles[0], definition);
        return EWuwaCombatInputResult.ActivationRequested;
    }

    // 自定义条件应当是纯查询；若扩展代码意外结束技能/清输入/换角色，不提交旧快照。
    private bool IsContextCurrent(UWuwaAbilitySystemComponent asc, UWuwaSkillBridgeComponent skills,
        FWuwaSkillData state, long generation, int fightHandle)
    {
        if (generation != _inputGeneration || !asc.IsValid() || !skills.IsValid()) return false;
        var avatar = asc.InputAvatar;
        if (!avatar.IsValid() || _avatar != new TWeakObjectPtr<AWuwaCharacter>(avatar)
            || avatar.SkillComponent != skills || !avatar.FightStateComponent.IsValid()) return false;
        var current = skills.GetCurrentSkillData();
        return current.SkillStartSerial == state.SkillStartSerial && current.FightStateHandle == state.FightStateHandle
            && current.InputOpportunitySerial == state.InputOpportunitySerial
            && avatar.FightStateComponent.StateData.Handle == fightHandle;
    }

    private static bool CanCreateCommand(UWuwaAbilitySystemComponent asc, UWuwaSkillBridgeComponent skills, Candidate candidate, out string gate)
    {
        //asc组件层允不允许
        if (!asc.CanRequestAbilityFromInput(candidate.Handle))
        {
            gate = "ASC.CanRequestAbilityFromInput=false";
            return false;
        }
        
        //自己的优先级机制等等,允不允许
        if (candidate.Definition.IsMainSkill && !skills.CanBeginSkill(candidate.Definition))
        {
            gate = "Skill.CanBeginSkill=false";
            return false;
        }
        gate = "Allowed";
        return true;
    }

    private EWuwaCombatInputResult Submit(UWuwaAbilitySystemComponent asc, Candidate candidate, double now, FWuwaSkillData state, string source)
    {
        Trace("SUBMIT", now, state, $"source={source} target={candidate.Definition.OriginalTag} ga={CombatInputTrace.Name(candidate.Definition)} consumeBatch={_buffer.Count}");
        // 一批只提交一次：即便 GAS 因冷却/消耗拒绝，也不再尝试第二候选。
        // 原作提交后清批；这里提前清批，避免同步回调再次消费同一份输入。
        _buffer.Clear();
        var result = asc.RequestAbilityActivation(candidate.Handle) switch
        {
            EWuwaAbilityRequestResult.ActivationRequested => EWuwaCombatInputResult.ActivationRequested,
            EWuwaAbilityRequestResult.AlreadyActive => EWuwaCombatInputResult.AlreadyActive,
            _ => EWuwaCombatInputResult.Rejected
        };
        if (CombatInputTrace.Enabled)
            CombatInputTrace.Write("RESULT", $"runtime={Name} result={result} target={candidate.Definition.OriginalTag} current=[{CombatInputTrace.Context(asc)}] cache={_buffer.Describe(now)}");
        return result;
    }

    private bool Store(FWuwaInputEvent input, double now, float lifetime, FWuwaSkillData state, string reason)
    {
        bool stored = _buffer.Store(input, now, lifetime);
        Trace(stored ? "STORE" : "STORE_REJECT", now, state, $"input={input.InputTag} reason={reason} lifetime={lifetime:F3}s expires={now + lifetime:F3}");
        return stored;
    }

    private void Trace(string stage, double now, FWuwaSkillData state, FormattableString details)
    {
        if (!CombatInputTrace.Enabled) return;
        CombatInputTrace.Write(stage, $"t={now:F3} runtime={Name} avatar={CombatInputTrace.Name(_avatar.Object)} {CombatInputTrace.Describe(state)} cache={_buffer.Describe(now)} {details.ToString(System.Globalization.CultureInfo.InvariantCulture)}");
    }
}
