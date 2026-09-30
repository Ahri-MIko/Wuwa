#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateTypes.h"
#include "WuwaFightStateBridgeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaFightStateChanged,
	const FWuwaFightStateData&, OldState, const FWuwaFightStateData&, NewState);

/** C++ 只提供反射和已提交快照；战斗裁决由 C# 子类实现。 */
UCLASS(Abstract, Blueprintable, ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaFightStateBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaFightStateBridgeComponent();

	/** 纯查询，不预留占用、不分配句柄。优先级范围为 0..255。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat|FightState")
	bool CheckSwitchState(EWuwaFightState NewState, int32 SubStatePriority = 0) const;

	/** 成功返回本次占用句柄，失败返回 0。只登记战斗状态，不启动或结束 GA。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|FightState")
	int32 TrySwitchState(EWuwaFightState NewState, int32 SubStatePriority = 0);

	/** 只能释放当前匹配的占用；旧技能迟到的结束不能清除新状态。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|FightState")
	bool ExitState(int32 Handle);

	/** 角色整体重置时使用；技能正常退出应传自己的句柄给 ExitState。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|FightState")
	void ResetState();

	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|FightState")
	FWuwaFightStateData GetStateData() const { return StateData; }

	/** 托管子类的提交出口，业务调用方通过 TrySwitchState / ExitState 操作。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|FightState", meta = (BlueprintProtected))
	void PublishFightState(FWuwaFightStateData NewState);

	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Combat|FightState")
	FWuwaFightStateChanged OnFightStateChanged;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Combat|FightState", meta = (AllowPrivateAccess = "true"))
	FWuwaFightStateData StateData;
};
