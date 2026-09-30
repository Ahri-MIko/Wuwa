#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaSkillBridgeComponent.generated.h"

class UWuwaSkillBridgeComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaAnimBreakPoint, UWuwaSkillBridgeComponent*, Skills, int32, SkillHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FWuwaInputCacheClearRequest, UWuwaSkillBridgeComponent*, Skills, int32, SkillHandle, FGameplayTag, InputTag);

/** 主技能管理的 UE 接口。当前技能、打断规则及 FightState 句柄由 C# 管理。 */
UCLASS(Abstract, Blueprintable, ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaSkillBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaSkillBridgeComponent();

	/** 纯查询。Ability 可以是本次待激活能力的 CDO。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool CanBeginSkill(UWuwaGameplayAbilityBase* Ability) const;

	/** GA PreActivate 的登记入口；必须传入属于本角色的活动实例。失败返回 0。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill", meta = (BlueprintProtected))
	int32 TryBeginSkill(UWuwaGameplayAbilityBase* Ability);

	/** GA 完成结束清理后回报；旧句柄不能清除新主技能。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill", meta = (BlueprintProtected))
	bool EndSkill(int32 FightStateHandle);

	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	FWuwaSkillData GetCurrentSkillData() const;

	/** 窗口必须带本次技能句柄，防止迟到 Notify 改写新技能。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool SetSkillAcceptInput(int32 FightStateHandle, bool bAcceptInput);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool SetMainSkillReadyEnd(int32 FightStateHandle, bool bReadyEnd);

	/** 原生通知桥传入真实播放实例及通知轨道事件索引；运行状态保存在脚本组件中。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool BeginSkillAcceptInputWindow(int32 FightStateHandle, int32 MontageInstanceId, int32 NotifyEventId);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool EndSkillAcceptInputWindow(int32 MontageInstanceId, int32 NotifyEventId);

	/** 修改当前 Skill 的运行时等级，不改写已经取得的 FightState 类别/子优先级。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill")
	bool SetSkillInterruptLevel(int32 FightStateHandle, int32 NewLevel);

	/** 显式检查一次预输入，不自行开放打断权限。EndAbility 清理中不要调用。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Input")
	bool CallAnimBreakPoint(int32 FightStateHandle);

	/** 请求清除本角色预输入；空 Tag 清全部，非空 Tag 精确匹配。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Input")
	bool RequestInputCacheClear(int32 FightStateHandle, FGameplayTag InputTag);

	// 只广播事实，不持有 Controller 或输入 Runtime；输入模块自行订阅。
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Combat|Input")
	FWuwaAnimBreakPoint OnAnimBreakPoint;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Combat|Input")
	FWuwaInputCacheClearRequest OnInputCacheClearRequested;

	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Input", meta = (BlueprintProtected))
	void BroadcastAnimBreakPoint(int32 SkillHandle);
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Input", meta = (BlueprintProtected))
	void BroadcastInputCacheClear(int32 SkillHandle, FGameplayTag InputTag);
};
