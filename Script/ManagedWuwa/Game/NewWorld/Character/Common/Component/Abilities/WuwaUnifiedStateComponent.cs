using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Abilities;

/// <summary>
/// 运动状态，对应原作 CharacterUnifiedStateComponent。
/// 位置、移动、朝向三个维度分别设置、分别广播；SetMoveState 用合法组合表拒绝当前位置下不合法的移动状态。
/// 本类不决定普通移动（由 RoleGait 每帧决定），也没有动作占用：与原作一样，后写入的状态直接覆盖之前的状态。
/// 已提交状态只保存在原生桥的 StateData 中；本类额外只保存玩家的走跑偏好（原作 Okr）。
/// </summary>
[UClass]
public partial class UWuwaUnifiedStateComponent : UWuwaUnifiedStateBridgeComponent
{
    private bool _prefersWalk;//原作 Okr：玩家保存的走/跑偏好，true 为走
    private bool _walkPreferenceInitialized;
    private bool _endingPlay;//是否游戏结束

    #region LifeCycle Methods

    //按当前物理位置初始化快照；走跑偏好只在第一次初始化时采用出生配置。两个值都由组装者从 CMC 读出后传入
    protected override void InitializeState_Implementation(EWuwaPositionState initialPosition, EWuwaGait initialWalkPreference)
    {
        _endingPlay = false;
        var state = StateData;
        state.PositionState = initialPosition;
        if (!_walkPreferenceInitialized)
        {
            _prefersWalk = initialWalkPreference == EWuwaGait.Walk;
            _walkPreferenceInitialized = true;
        }

        state.MoveState = DefaultMoveState(state.PositionState);
        state.DirectionState = EWuwaDirectionState.FaceDirection;
        state.Gait = PreferredGait;
        state.HasActionOverride = IsActionMoveState(state.MoveState);
        // 与原作 InitCharState 一样只写初始值、不广播；Character 装配完成后让 CMC 按快照应用一次配置。
        CommitStateData(state);
    }

    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        base.EndPlay(endPlayReason);
    }

    #endregion

    #region Core

    //原作 SetPositionState：只有主控端能修改；位置变化后执行 OnPositionStateChange
    protected override bool SetPositionState_Implementation(EWuwaPositionState newPosition)
    {
        if (_endingPlay || !CanDriveState() || !IsKnownPosition(newPosition))
        {
            return false;
        }

        var state = StateData;
        var oldPosition = state.PositionState;
        if (oldPosition == newPosition)
        {
            return false;
        }

        state.PositionState = newPosition;
        CommitStateData(state);
        OnPositionStateChange(oldPosition, newPosition);
        return true;
    }

    //原作 OnPositionStateChange：进入 Ground 先执行 OnLand，再广播位置变化
    private void OnPositionStateChange(EWuwaPositionState oldPosition, EWuwaPositionState newPosition)
    {
        if (newPosition == EWuwaPositionState.Ground)
        {
            OnLand();
        }

        BroadcastPositionStateChanged(oldPosition, newPosition);
    }

    //原作 OnLand：KnockUp 转 StandUp、受击中保持不变，其余写 Other。本项目没有这两类状态，落地统一写 Other，之后由 RoleGait 决定
    private void OnLand()
    {
        SetMoveState(EWuwaMoveState.Other, PreferredGait);
    }

    //原作 SetMoveState：当前位置下不合法的移动状态直接拒绝。Gait 是本项目额外保存的速度配置维度
    protected override bool SetMoveState_Implementation(EWuwaMoveState newState, EWuwaGait newGait)
    {
        if (_endingPlay || !CanDriveState() || !IsKnownGait(newGait))
        {
            return false;
        }

        var state = StateData;
        if (!IsMoveStateLegal(state.PositionState, newState))
        {
            return false;
        }

        var oldMove = state.MoveState;
        var oldGait = state.Gait;
        if (oldMove == newState && oldGait == newGait)
        {
            // 同值请求是合法请求，但不重复广播。
            return true;
        }

        state.MoveState = newState;
        state.Gait = newGait;
        state.HasActionOverride = IsActionMoveState(newState);
        CommitStateData(state);
        // 先写入再广播；Revision 由原生提交出口递增。
        if (oldMove != newState)
        {
            BroadcastMoveStateChanged(oldMove, newState);
        }

        if (oldGait != newGait)
        {
            BroadcastGaitChanged(oldGait, newGait);
        }

        return true;
    }

    //原作 SetDirectionState
    protected override bool SetDirectionState_Implementation(EWuwaDirectionState newDirection)
    {
        if (_endingPlay || !CanDriveState() || newDirection is not (EWuwaDirectionState.FaceDirection
                or EWuwaDirectionState.LockDirection or EWuwaDirectionState.AimDirection))
        {
            return false;
        }

        var state = StateData;
        var oldDirection = state.DirectionState;
        if (oldDirection == newDirection)
        {
            return false;
        }

        state.DirectionState = newDirection;
        CommitStateData(state);
        BroadcastDirectionStateChanged(oldDirection, newDirection);
        return true;
    }

    //原作监听 CharMovementModeChanged 的处理：物理移动模式决定位置，部分模式同时写入移动状态
    protected override void HandleMovementModeChanged_Implementation(EWuwaPositionState newPosition, EMovementMode newMode)
    {
        if (_endingPlay)
        {
            return;
        }

        SetPositionState(newPosition);
        switch (newMode)
        {
            case EMovementMode.MOVE_None:
                SetMoveState(EWuwaMoveState.Other, PreferredGait);
                break;
            case EMovementMode.MOVE_Falling:
                // 原作：KnockUp、Captured 以外写 Other；本项目没有这两个状态。
                SetMoveState(EWuwaMoveState.Other, PreferredGait);
                break;
            case EMovementMode.MOVE_Flying:
                SetMoveState(EWuwaMoveState.Flying, PreferredGait);
                break;
        }
    }

    #endregion

    #region Walk / Run

    //偏好是不是"走"（原作 IsWalkBaseMode）
    protected override bool IsWalkPreferred_Implementation() => _prefersWalk;

    //直接设定偏好（原作 MarkWalkOrRun）：只修改偏好并广播，不直接改移动状态；移动状态由 RoleGait 按偏好决定
    protected override bool SetWalkPreference_Implementation(bool walk)
    {
        if (_endingPlay || !CanDriveState() || walk == _prefersWalk)
        {
            return false;
        }

        var wasWalk = _prefersWalk;
        _prefersWalk = walk;
        _walkPreferenceInitialized = true;
        BroadcastWalkPreferenceChanged(wasWalk, walk);
        return true;
    }

    //在走/跑之间切换（原作 WalkPress）：原作在这里先问 CMC 的 CanWalkPress；本项目由调用方 CMC 先判断 CanToggleWalkPreference，本组件不依赖 CMC
    protected override bool ToggleWalkPreference_Implementation() => SetWalkPreference(!_prefersWalk);

    private EWuwaGait PreferredGait => _prefersWalk ? EWuwaGait.Walk : EWuwaGait.Run;

    #endregion

    //原作 legalMoveStates：按位置维度列出允许的移动状态，这里用本项目的枚举表达
    protected override bool IsMoveStateLegal_Implementation(EWuwaPositionState position, EWuwaMoveState move)
    {
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

    //初始化时每种位置对应的默认移动状态（本项目规则）
    private static EWuwaMoveState DefaultMoveState(EWuwaPositionState position) => position switch
    {
        EWuwaPositionState.Ground => EWuwaMoveState.Stand,
        EWuwaPositionState.Air => EWuwaMoveState.Fall,
        EWuwaPositionState.Climb => EWuwaMoveState.NormalClimb,
        EWuwaPositionState.Water => EWuwaMoveState.NormalSwim,
        _ => EWuwaMoveState.Other
    };

    //由 GA 写入、不属于普通移动的动作状态；StateData.bHasActionOverride 只是它的兼容读数，不是占用
    private static bool IsActionMoveState(EWuwaMoveState move) => move == EWuwaMoveState.Dodge;

    //是在表中记录过的位置状态
    private static bool IsKnownPosition(EWuwaPositionState position) => position is EWuwaPositionState.None
        or EWuwaPositionState.Ground or EWuwaPositionState.Air or EWuwaPositionState.Climb or EWuwaPositionState.Water;

    //是知道的步态
    private static bool IsKnownGait(EWuwaGait gait) => gait is EWuwaGait.Walk or EWuwaGait.Run or EWuwaGait.Sprint;
}
