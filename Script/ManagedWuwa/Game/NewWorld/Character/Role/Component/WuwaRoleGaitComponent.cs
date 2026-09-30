using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Role.Component;

//主要计算三种步态
/// <summary>Owns gait preference and sprint requests; UnifiedState owns the resolved movement state.</summary>
[UClass]
public partial class UWuwaRoleGaitComponent : UWuwaRoleGaitBridgeComponent
{
    private sealed class SprintWindow
    {
        public double BeganAt;
        public EWuwaSprintDesire Desire = EWuwaSprintDesire.Temporary;
    }

    private readonly Dictionary<TWeakObjectPtr<UObject>, SprintWindow> _windows = new();
    private readonly Dictionary<EWuwaGait, HashSet<TWeakObjectPtr<UObject>>> _blocked = new()
    {
        [EWuwaGait.Walk] = new(),
        [EWuwaGait.Run] = new(),
        [EWuwaGait.Sprint] = new()
    };

    private bool _initialized;
    private bool _refreshing;
    private bool _endingPlay;
    private EWuwaGait _desired = EWuwaGait.Run;
    private EWuwaGait _lastMovingGait = EWuwaGait.Run;
    private EWuwaPositionState _lastPosition = EWuwaPositionState.None;
    private EWuwaSprintDesire _retained = EWuwaSprintDesire.None;
    private double _temporaryExpiresAt;

    protected override void InitializePolicy_Implementation()
    {
        _endingPlay = false;
        InitializeFromContext(ReadMovementContext());
        RefreshPolicy();
    }

    protected override void RefreshPolicy_Implementation()
    {
        if (_refreshing || _endingPlay)
        {
            return;
        }

        _refreshing = true;
        try
        {
            var context = ReadMovementContext();
            var state = GetUnifiedState();
            if (state is null)
            {
                ClearSprintRuntime();
                PublishGait(_desired, EWuwaSprintDesire.None, _lastMovingGait);
                return;
            }

            // Position legality is resolved before action ownership or gait selection.
            state.ChangePositionState(context.PositionState);
            state.PruneStateOwners();//如果当前没有占用就直接清理
            PruneRuntime(context);

            if (state.StateData.HasActionOverride)
            {
                PublishGait(_desired, EvaluateSprintDesire(context), _lastMovingGait);
                return;
            }

            var move = EWuwaMoveState.Other;
            var gait = _desired;
            if (context.CanDriveState)
            {
                switch (context.PositionState)
                {
                    case EWuwaPositionState.Ground:
                        if (!context.IsCrouching)
                        {
                            ResolveGroundState(context, state.StateData.MoveState, out move, out gait);
                        }
                        break;
                    case EWuwaPositionState.Air:
                        move = context.IsFlying ? EWuwaMoveState.Flying
                            : context.VerticalSpeed > 0f ? EWuwaMoveState.Jump : EWuwaMoveState.Fall;
                        break;
                    case EWuwaPositionState.Climb:
                        move = EWuwaMoveState.NormalClimb;
                        break;
                    case EWuwaPositionState.Water:
                        move = EWuwaMoveState.NormalSwim;
                        break;
                }
            }

            // The native movement/animation consumers observe this publication with the state event.
            PublishGait(_desired, EvaluateSprintDesire(context), _lastMovingGait);
            state.TrySetMoveState(move, gait);
        }
        finally
        {
            _refreshing = false;
        }
    }

    protected override bool RequestDesiredGait_Implementation(EWuwaGait newGait)
    {
        var context = ReadMovementContext();
        InitializeFromContext(context);
        if (!IsKnownGait(newGait) || !context.CanDriveState)
        {
            return false;
        }

        if (newGait == EWuwaGait.Sprint)
        {
            if (!CanSampleSprint(context))
            {
                return false;
            }
            // Legacy SetDesiredGait(Sprint) becomes a request without overwriting Walk/Run preference.
            _retained = context.HasMoveInput ? EWuwaSprintDesire.Sustained : EWuwaSprintDesire.None;
            _temporaryExpiresAt = 0.0;
        }
        else
        {
            _desired = newGait;
        }
        RefreshPolicy();
        return true;
    }

    #region  WalkRunToggle

    protected override bool RequestWalkRunToggle_Implementation()
    {
        if (!CanRequestWalkRun())
        {
            return false;
        }
        InitializeFromContext(ReadMovementContext());
        _desired = _desired == EWuwaGait.Walk ? EWuwaGait.Run : EWuwaGait.Walk;
        RefreshPolicy();
        return true;
    }

    //从鸣潮来看这里后续要更改成为任意时刻之类的
    protected override bool CanRequestWalkRun_Implementation()
    {
        var context = ReadMovementContext();
        return context.CanDriveState && context.PositionState == EWuwaPositionState.Ground
                                     && !context.IsCrouching;
    }


    #endregion
    
    protected override void SetGaitBlocked_Implementation(UObject source, EWuwaGait gait, bool blocked)
    {
        if (!source.IsValid() || !_blocked.TryGetValue(gait, out var sources))
        {
            return;
        }
        var key = new TWeakObjectPtr<UObject>(source);
        if (blocked)
        {
            sources.Add(key);
        }
        else
        {
            sources.Remove(key);
        }
        RefreshPolicy();
    }

    protected override bool IsGaitAllowed_Implementation(EWuwaGait gait) => IsAllowed(gait, ReadMovementContext());

    protected override void OpenSprintWindow_Implementation(UObject source)
    {
        if (_endingPlay) return;
        var context = ReadMovementContext();
        InitializeFromContext(context);
        if (!source.IsValid() || !CanSampleSprint(context))
        {
            return;
        }
        // Deal with position/control exits before accepting a new window.
        PruneRuntime(context);
        _windows[new TWeakObjectPtr<UObject>(source)] = new SprintWindow { BeganAt = context.GameTimeSeconds };
        RefreshPolicy();
    }

    protected override void SampleSprintWindow_Implementation(UObject source, float inputHeldSeconds, float holdThresholdSeconds)
    {
        if (_endingPlay) return;
        if (!source.IsValid())
        {
            return;
        }
        var context = ReadMovementContext();
        PruneRuntime(context);
        if (!CanSampleSprint(context) || !_windows.TryGetValue(new TWeakObjectPtr<UObject>(source), out var window))
        {
            return;
        }

        var held = float.IsFinite(inputHeldSeconds) ? Math.Max(0f, inputHeldSeconds) : 0f;
        var threshold = float.IsFinite(holdThresholdSeconds) ? Math.Max(0f, holdThresholdSeconds) : 0.2f;
        var heldInWindow = Math.Min(Math.Max(0.0, context.GameTimeSeconds - window.BeganAt), held);
        window.Desire = heldInWindow > threshold ? EWuwaSprintDesire.Sustained : EWuwaSprintDesire.Temporary;
        RefreshPolicy();
    }

    protected override void CloseSprintWindow_Implementation(UObject source)
    {
        if (_endingPlay) return;
        if (!source.IsValid())
        {
            return;
        }
        var context = ReadMovementContext();
        PruneRuntime(context);
        if (!_windows.Remove(new TWeakObjectPtr<UObject>(source), out var window))
        {
            return; // A late/duplicate End cannot recreate a request or extend its deadline.
        }

        if (_retained != EWuwaSprintDesire.Sustained)
        {
            _retained = window.Desire;
            var duration = float.IsFinite(context.TemporarySprintDuration)
                ? Math.Max(0f, context.TemporarySprintDuration) : 1f;
            _temporaryExpiresAt = _retained == EWuwaSprintDesire.Temporary
                ? context.GameTimeSeconds + duration : 0.0;
        }
        if (!context.HasMoveInput && _retained == EWuwaSprintDesire.Sustained)
        {
            _retained = EWuwaSprintDesire.None;
            _temporaryExpiresAt = 0.0;
        }
        RefreshPolicy();
    }

    protected override void ResetSprintRequest_Implementation()
    {
        ClearSprintRuntime();
        RefreshPolicy();
    }

    protected override EWuwaSprintDesire ReadSprintDesire_Implementation() => EvaluateSprintDesire(ReadMovementContext());

    protected override void ResetRuntime_Implementation()
    {
        var wasRefreshing = _refreshing;
        _refreshing = true;
        try
        {
            ClearSprintRuntime();
            foreach (var sources in _blocked.Values)
            {
                sources.Clear();
            }
            GetUnifiedState()?.ResetActionStates();
        }
        finally
        {
            _refreshing = wasRefreshing;
        }
        RefreshPolicy();
    }

    private void InitializeFromContext(FWuwaMovementStateContext context)
    {
        if (_initialized)
        {
            return;
        }
        _desired = context.DefaultGait == EWuwaGait.Walk ? EWuwaGait.Walk : EWuwaGait.Run;
        _lastMovingGait = _desired;
        _lastPosition = context.PositionState;
        _initialized = true;
    }

    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        ClearSprintRuntime();
        foreach (var sources in _blocked.Values) sources.Clear();
        base.EndPlay(endPlayReason);
    }

    private UWuwaUnifiedStateBridgeComponent? GetUnifiedState()
    {
        if (Owner is not AWuwaCharacter character || !character.IsValid())
        {
            return null;
        }
        var state = character.UnifiedStateComponent;
        return state.IsValid() ? state : null;
    }

    //清除跑步欲望
    private void ClearSprintRuntime()
    {
        _windows.Clear();
        _retained = EWuwaSprintDesire.None;
        _temporaryExpiresAt = 0.0;
    }

    //清理失效的GA留存以及重新判断冲刺状态
    private void PruneRuntime(FWuwaMovementStateContext context)
    {
        foreach (var sources in _blocked.Values)
        {
            sources.RemoveWhere(source => !source.IsValid);
        }
        if (context.PositionState != _lastPosition || !CanSampleSprint(context))
        {
            ClearSprintRuntime();
        }
        _lastPosition = context.PositionState;
        //判断冲刺状态
        if (_retained == EWuwaSprintDesire.Sustained && !context.HasMoveInput
            || _retained == EWuwaSprintDesire.Temporary && context.GameTimeSeconds >= _temporaryExpiresAt)
        {
            _retained = EWuwaSprintDesire.None;
            _temporaryExpiresAt = 0.0;
        }
        foreach (var source in _windows.Keys.Where(source => !source.IsValid).ToArray())
        {
            _windows.Remove(source);
        }
    }

    private EWuwaSprintDesire EvaluateSprintDesire(FWuwaMovementStateContext context)
    {
        if (!CanSampleSprint(context) || context.PositionState != _lastPosition)
        {
            return EWuwaSprintDesire.None;
        }
        var result = _retained;
        if (result == EWuwaSprintDesire.Sustained && !context.HasMoveInput
            || result == EWuwaSprintDesire.Temporary && context.GameTimeSeconds >= _temporaryExpiresAt)
        {
            result = EWuwaSprintDesire.None;
        }
        if (result == EWuwaSprintDesire.Sustained)
        {
            return result;
        }
        foreach (var (source, window) in _windows)
        {
            if (!source.IsValid)
            {
                continue;
            }
            if (window.Desire == EWuwaSprintDesire.Sustained)
            {
                return EWuwaSprintDesire.Sustained;
            }
            result = EWuwaSprintDesire.Temporary;
        }
        return result;
    }

    private bool IsAllowed(EWuwaGait gait, FWuwaMovementStateContext context)
    {
        return context.CanDriveState && context.PositionState == EWuwaPositionState.Ground
            && !context.IsCrouching && _blocked.TryGetValue(gait, out var sources)
            && !sources.Any(source => source.IsValid);
    }

    private static bool IsKnownGait(EWuwaGait gait) => gait is EWuwaGait.Walk or EWuwaGait.Run or EWuwaGait.Sprint;

    private static bool CanSampleSprint(FWuwaMovementStateContext context)
    {
        return context.CanDriveState && context.PositionState == EWuwaPositionState.Ground
            && !context.IsCrouching && double.IsFinite(context.GameTimeSeconds) && context.GameTimeSeconds >= 0.0;
    }

    private void ResolveGroundState(FWuwaMovementStateContext context, EWuwaMoveState current,
        out EWuwaMoveState move, out EWuwaGait gait)
    {
        gait = _desired;
        move = EWuwaMoveState.Other;
        if (context.HasMoveInput)
        {
            if (EvaluateSprintDesire(context) != EWuwaSprintDesire.None && IsAllowed(EWuwaGait.Sprint, context))
            {
                gait = EWuwaGait.Sprint;
            }
            else if (!IsAllowed(gait, context))
            {
                gait = _desired == EWuwaGait.Walk ? EWuwaGait.Run : EWuwaGait.Walk;
                if (!IsAllowed(gait, context))
                {
                    return;
                }
            }
            _lastMovingGait = gait;
            move = gait switch
            {
                EWuwaGait.Walk => EWuwaMoveState.Walk,
                EWuwaGait.Sprint => EWuwaMoveState.Sprint,
                _ => EWuwaMoveState.Run
            };
            return;
        }

        if (!IsAllowed(EWuwaGait.Walk, context) && !IsAllowed(EWuwaGait.Run, context))
        {
            return;
        }
        move = EWuwaMoveState.Stand;
        if (!float.IsFinite(context.GroundSpeed) || context.GroundSpeed <= 2f)
        {
            return;
        }

        // Only an actual locomotion state can enter Stop; a finished Dodge cannot manufacture a stop.
        switch (current)
        {
            case EWuwaMoveState.Walk:
            case EWuwaMoveState.WalkStop:
                move = EWuwaMoveState.WalkStop;
                gait = EWuwaGait.Walk;
                break;
            case EWuwaMoveState.Run:
            case EWuwaMoveState.RunStop:
                move = EWuwaMoveState.RunStop;
                gait = EWuwaGait.Run;
                break;
            case EWuwaMoveState.Sprint:
            case EWuwaMoveState.SprintStop:
                move = EWuwaMoveState.SprintStop;
                gait = EWuwaGait.Sprint;
                break;
        }
    }
}
