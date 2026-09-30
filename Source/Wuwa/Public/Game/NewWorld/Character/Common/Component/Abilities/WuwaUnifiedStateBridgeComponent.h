#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "WuwaUnifiedStateBridgeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaUnifiedStateChanged,
	const FWuwaUnifiedStateData&, OldState, const FWuwaUnifiedStateData&, NewState);

/** 仅承担反射、快照和事件桥。合法性/动作占用的规则由 C# 子类实现。 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Wuwa), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaUnifiedStateBridgeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") void InitializeState();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool TrySetMoveState(EWuwaMoveState NewState, EWuwaGait NewGait);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool ChangePositionState(EWuwaPositionState NewPosition);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool ChangeDirectionState(EWuwaDirectionState NewDirection);
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|State") bool IsMoveStateLegal(EWuwaPositionState Position, EWuwaMoveState Move) const;
	/** 纯预检查：不清理失效来源、不提交状态；真正 Acquire 时仍会复检。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|State") bool CanAcquireMoveState(EWuwaMoveState NewState, int32 Priority = 100) const;
	/** 技能交接的纯查询：仅忽略即将结束的那个来源，仍检查位置合法性和其他占用。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|State") bool CanAcquireMoveStateAfterRelease(EWuwaMoveState NewState, int32 Priority, UObject* ReleasingSource) const;
	/** 返回本次占用的句柄。旧 GA 只能释放自己的句柄，不能清除后来的动作。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") int32 AcquireMoveState(UObject* Source, EWuwaMoveState NewState, int32 Priority = 100);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool ReleaseMoveState(int32 Handle);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") void ResetActionStates();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") void PruneStateOwners();

	UFUNCTION(BlueprintPure, Category="Wuwa|State") FWuwaUnifiedStateData GetStateData() const { return StateData; }
	/** C# 唯一提交出口；同值提交不广播，不重置物理速度。 */
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void PublishState(FWuwaUnifiedStateData NewState);
	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaUnifiedStateChanged OnStateChanged;
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State", meta=(AllowPrivateAccess="true"))
	FWuwaUnifiedStateData StateData;
};
