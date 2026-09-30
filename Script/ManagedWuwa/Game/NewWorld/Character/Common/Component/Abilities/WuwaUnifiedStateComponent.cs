using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Abilities;

/// <summary>
/// 角色的统一状态规则。已提交状态只保存在原生桥的 StateData 中；
/// 本类只额外保留动作占用的所有权，不再缓存第二份位置、移动或朝向状态。
/// </summary>
[UClass]
public partial class UWuwaUnifiedStateComponent : UWuwaUnifiedStateBridgeComponent
{
    private readonly record struct ActionLease(int Handle, int Priority, TWeakObjectPtr<UObject> Source);

    private ActionLease? _activeAction;
    private int _lastIssuedHandle;
    private bool _endingPlay;//是否游戏结束

    
    
    #region LifeCycle Methods
    
    //初始化一些值将其保存为
    protected override void InitializeState_Implementation()
    {
        _endingPlay = false;
        _activeAction = null;
        // 初始化/换人时不重置句柄计数；上一轮动作的迟到 End 不能匹配新动作。
        var state = StateData;
        state.PositionState = EWuwaPositionState.None;
        state.Gait = EWuwaGait.Run;

        if (Owner is AWuwaCharacter character && character.IsValid())
        {
            var gait = character.GetComponentByClass<UWuwaRoleGaitBridgeComponent>();
            if (gait.IsValid())
            {
                var context = gait.ReadMovementContext();
                state.PositionState = context.PositionState;
                state.Gait = IsKnownGait(context.DefaultGait) ? context.DefaultGait : EWuwaGait.Run;
            }
        }

        state.MoveState = DefaultMoveState(state.PositionState);
        state.DirectionState = EWuwaDirectionState.FaceDirection;
        state.HasActionOverride = false;
        PublishIfChanged(state);
    }
    
    
    
    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        ResetActionStates();
        base.EndPlay(endPlayReason);
    }

    #endregion

    #region Core 

    //将自己的副本和真正的C++里面的权威内容做比较如果有变化就直接Publish
    private bool PublishIfChanged(FWuwaUnifiedStateData state)
    {
        var previous = StateData;
        if (previous.PositionState == state.PositionState && previous.MoveState == state.MoveState
                                                          && previous.DirectionState == state.DirectionState && previous.Gait == state.Gait
                                                          && previous.HasActionOverride == state.HasActionOverride)
        {
            return false;
        }

        // 所有本地所有权修改都在广播前完成；广播后不再写旧快照，避免覆盖重入提交。
        // Revision 由原生提交出口递增，本类不另维护版本或已提交状态副本。
        PublishState(state);
        return true;
    }

    #endregion
    
    //尝试改变运动模式和步态
    protected override bool TrySetMoveState_Implementation(EWuwaMoveState newState, EWuwaGait newGait)
    {
        if (_endingPlay || !IsKnownGait(newGait))
        {
            return false;
        }

        PruneStateOwners();
        // Prune 的事件可能重入并取得新占用；必须在它之后重新查询。
        var state = StateData;
        if (_endingPlay || _activeAction.HasValue || !IsMoveStateLegal(state.PositionState, newState))
        {
            return false;
        }

        state.MoveState = newState;
        state.Gait = newGait;
        state.HasActionOverride = false;
        PublishIfChanged(state);
        // 同值请求也是合法请求，但不会广播重复事件。
        return true;
    }

    //把角色的物理位置同步到运动状态里,也就是地面、空中、攀爬、水中之间的切换
    protected override bool ChangePositionState_Implementation(EWuwaPositionState newPosition)
    {
        if (_endingPlay || !IsKnownPosition(newPosition))
        {
            return false;
        }

        PruneStateOwners();
        var state = StateData;
        if (_endingPlay || state.PositionState == newPosition)
        {
            return false;
        }

        state.PositionState = newPosition;
        if (_activeAction.HasValue && !IsMoveStateLegal(newPosition, state.MoveState))
        {
            // 物理位置使占用失效；这里只归还表现权，不负责终止 GAS 能力。
            _activeAction = null;
        }

        state.HasActionOverride = _activeAction.HasValue;
        if (!state.HasActionOverride)
        {
            state.MoveState = DefaultMoveState(newPosition);
        }

        return PublishIfChanged(state);
    }

    //改变朝向
    protected override bool ChangeDirectionState_Implementation(EWuwaDirectionState newDirection)
    {
        if (_endingPlay || newDirection is not (EWuwaDirectionState.FaceDirection
                or EWuwaDirectionState.LockDirection or EWuwaDirectionState.AimDirection))
        {
            return false;
        }

        var state = StateData;
        state.DirectionState = newDirection;
        return PublishIfChanged(state);
    }

    //当前只有测试在用
    protected override bool CanAcquireMoveState_Implementation(EWuwaMoveState newState, int priority)
    {
        // CanActivate 会在 CDO 上查询多个角色；这里不 Prune，也不分配句柄或广播。
        return CanAcquireMoveStateNow(newState, priority);
    }

    
    protected override bool CanAcquireMoveStateAfterRelease_Implementation(EWuwaMoveState newState, int priority, UObject releasingSource)
    {
        return CanAcquireMoveStateNow(newState, priority, releasingSource);
    }

    //检查是否可以替换动作,判断上一个是不是合法的并且,当前的动作切换姿态是不是合法的比如空中冲刺是没有这个组合的,然后再看Priority
    private bool CanAcquireMoveStateNow(EWuwaMoveState newState, int priority, UObject? releasingSource = null)
    {
        return !_endingPlay && _lastIssuedHandle < int.MaxValue
            && IsMoveStateLegal(StateData.PositionState, newState)
            && (_activeAction is not { } current || !current.Source.IsValid || priority >= current.Priority
                || releasingSource is not null && releasingSource.IsValid()
                    && current.Source == new TWeakObjectPtr<UObject>(releasingSource));
    }

    //申请动作
    protected override int AcquireMoveState_Implementation(UObject source, EWuwaMoveState newState, int priority)
    {
        if (!source.IsValid() || !CanAcquireMoveStateNow(newState, priority))
        {
            return 0;
        }

        PruneStateOwners();
        // 预检查与取得占用之间，以及 Prune 的广播期间，都可能出现新的更高优先级动作。
        if (!source.IsValid() || !CanAcquireMoveStateNow(newState, priority))
        {
            return 0;
        }

        var state = StateData;
        // 同优先级最新请求胜出，包括同一 Source 的重新激活。
        // 占用不是栈：被替换的旧动作不会在新动作结束后重新出现。
        int handle = ++_lastIssuedHandle;
        _activeAction = new ActionLease(handle, priority, new TWeakObjectPtr<UObject>(source));
        state.MoveState = newState;
        state.HasActionOverride = true;
        PublishIfChanged(state);

        // 广播期间允许监听者释放/替换本次占用，不把已失效句柄作为成功返回。
        return _activeAction is { } current && current.Handle == handle ? handle : 0;
    }

    //归还占用句柄
    protected override bool ReleaseMoveState_Implementation(int handle)
    {
        if (handle <= 0 || _activeAction is not { } current || current.Handle != handle)
        {
            // 旧动作的 NotifyEnd / EndAbility 不能清除后来动作，也不产生状态事件。
            return false;
        }

        _activeAction = null;
        PublishDefaultMoveState();
        return true;
    }

    //如果没有动作在主位置就将其设置为null然后广播每个位置的默认移动状态
    protected override void ResetActionStates_Implementation()
    {
        if (!_activeAction.HasValue && !StateData.HasActionOverride)
        {
            return;
        }

        _activeAction = null;
        PublishDefaultMoveState();
    }

    //如果占用的动作已经不在了就重置当前的动作,根据当前的Position状态决定当前的默认状态,并且清空当前占用动作
    protected override void PruneStateOwners_Implementation()
    {
        if (_activeAction is not { } current || current.Source.IsValid)
        {
            return;
        }

        _activeAction = null;
        PublishDefaultMoveState();
    }
    
    //设置默认的移动状态
    private void PublishDefaultMoveState()
    {
        var state = StateData;
        state.MoveState = DefaultMoveState(state.PositionState);
        state.HasActionOverride = false;
        PublishIfChanged(state);
    }
    
    
    //按照组合维度查看是否合法
    protected override bool IsMoveStateLegal_Implementation(EWuwaPositionState position, EWuwaMoveState move)
    {
        // 原作的分维度合法组合表在此用本项目的枚举表达，不复用原作数值。
        return position switch
        {
            EWuwaPositionState.Ground => move is EWuwaMoveState.Other or EWuwaMoveState.Stand
                or EWuwaMoveState.Walk or EWuwaMoveState.WalkStop
                or EWuwaMoveState.Run or EWuwaMoveState.RunStop
                or EWuwaMoveState.Sprint or EWuwaMoveState.SprintStop or EWuwaMoveState.Dodge,
            EWuwaPositionState.Air => move is EWuwaMoveState.Other or EWuwaMoveState.Dodge
                or EWuwaMoveState.Jump or EWuwaMoveState.Fall or EWuwaMoveState.Glide or EWuwaMoveState.Flying,
            EWuwaPositionState.Climb => move is EWuwaMoveState.Other
                or EWuwaMoveState.NormalClimb or EWuwaMoveState.FastClimb,
            EWuwaPositionState.Water => move is EWuwaMoveState.Other
                or EWuwaMoveState.NormalSwim or EWuwaMoveState.FastSwim,
            EWuwaPositionState.None => move == EWuwaMoveState.Other,
            _ => false
        };
    }

    //默认每种位置对应的模式
    private static EWuwaMoveState DefaultMoveState(EWuwaPositionState position) => position switch
    {
        EWuwaPositionState.Ground => EWuwaMoveState.Stand,
        EWuwaPositionState.Air => EWuwaMoveState.Fall,
        EWuwaPositionState.Climb => EWuwaMoveState.NormalClimb,
        EWuwaPositionState.Water => EWuwaMoveState.NormalSwim,
        _ => EWuwaMoveState.Other
    };

    //是在表中记录过的位置状态
    private static bool IsKnownPosition(EWuwaPositionState position) => position is EWuwaPositionState.None
        or EWuwaPositionState.Ground or EWuwaPositionState.Air or EWuwaPositionState.Climb or EWuwaPositionState.Water;

    //是知道的步态
    private static bool IsKnownGait(EWuwaGait gait) => gait is EWuwaGait.Walk or EWuwaGait.Run or EWuwaGait.Sprint;
}
