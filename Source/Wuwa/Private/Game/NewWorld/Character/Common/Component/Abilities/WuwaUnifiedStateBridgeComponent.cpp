#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"

// C++ 基类不复制脚本规则。未装配托管子类时请求失败，而不是偷偷运行另一套状态机。
//初始化
void UWuwaUnifiedStateBridgeComponent::InitializeState_Implementation() {}
//切换状态（请求型，可能失败，所以返回 bool）
bool UWuwaUnifiedStateBridgeComponent::TrySetMoveState_Implementation(EWuwaMoveState, EWuwaGait) { return false; }//请求切换移动状态和步态，比如"跑步，快速"
bool UWuwaUnifiedStateBridgeComponent::ChangePositionState_Implementation(EWuwaPositionState) { return false; }//切换位置状态，比如地面、空中、游泳
bool UWuwaUnifiedStateBridgeComponent::ChangeDirectionState_Implementation(EWuwaDirectionState) { return false; }//切换朝向模式，比如跟随速度方向或者锁定目标，或者任务时切换
//合法性检查（const，只查询不修改）
bool UWuwaUnifiedStateBridgeComponent::IsMoveStateLegal_Implementation(EWuwaPositionState, EWuwaMoveState) const { return false; }//某个位置状态下，是否允许某个移动状态。比如在空中时不允许"冲刺起步"。
bool UWuwaUnifiedStateBridgeComponent::CanAcquireMoveState_Implementation(EWuwaMoveState, int32) const { return false; }//以当前优先级，能不能抢占这个移动状态。
bool UWuwaUnifiedStateBridgeComponent::CanAcquireMoveStateAfterRelease_Implementation(EWuwaMoveState, int32, UObject*) const { return false; }
//占用 / 释放（带所有权的状态）
int32 UWuwaUnifiedStateBridgeComponent::AcquireMoveState_Implementation(UObject*, EWuwaMoveState, int32) { return 0; }//某个对象（比如一个技能）以某个优先级占用一个移动状态，返回一个句柄（int32）。
bool UWuwaUnifiedStateBridgeComponent::ReleaseMoveState_Implementation(int32) { return false; }//用这个句柄释放占用。
//清理
void UWuwaUnifiedStateBridgeComponent::ResetActionStates_Implementation() {}//清空所有动作类的状态覆盖，对应 StateData 里的 bHasActionOverride。
void UWuwaUnifiedStateBridgeComponent::PruneStateOwners_Implementation() {}//清理失效的占用者。比如一个技能在销毁时没有调用 Release，它的占用就会一直残留。这个函数会把 Owner 已经失效的占用记录删除，防止状态被永久卡住。

void UWuwaUnifiedStateBridgeComponent::PublishState(FWuwaUnifiedStateData NewState)
{
	check(IsInGameThread());
	//如果和之前的状态一样就不播报了
	if (StateData.PositionState == NewState.PositionState && StateData.MoveState == NewState.MoveState
		&& StateData.DirectionState == NewState.DirectionState && StateData.Gait == NewState.Gait
		&& StateData.bHasActionOverride == NewState.bHasActionOverride)
	{
		return;
	}
	const FWuwaUnifiedStateData OldState = StateData;
	NewState.Revision = StateData.Revision + 1;
	StateData = NewState;
	// 使用局部副本，订阅者重入导致另一次提交时，不改变本次事件的参数。
	OnStateChanged.Broadcast(OldState, NewState);
}
