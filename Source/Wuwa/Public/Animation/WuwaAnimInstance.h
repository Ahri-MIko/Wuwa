#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/WuwaLocomotionTypes.h"
#include "WuwaAnimInstance.generated.h"

class ACharacter;
class UWuwaMovementComponent;
class UWuwaAnimLogicParams;

#pragma region Debug

/** 单个状态在所属状态机内部的权重，不是最终全身姿势权重。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDebugAnimStateWeight
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName StateName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	float Weight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bIsCurrentState = false;
};

/** 最近一次状态机更新结果；在 Blueprint Update Animation 中读取可能滞后一帧。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDebugAnimStateMachine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName MachineName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bInitialized = false;

	/** 状态机本身的记录权重。为零时，内部状态可能只是上次激活时留下的结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	float MachineWeight = 0.f;

	/** 逻辑上的当前状态，不一定是权重最大的状态。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName CurrentStateName = NAME_None;

	/** 所有权重大于零的非 Conduit 状态，按权重降序排列。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	TArray<FWuwaDebugAnimStateWeight> ActiveStates;
};

#pragma endregion Debug

/** Common 动画父类：采集角色数据，AnimBP 再组织走、跑、起停和动画层。 */
UCLASS(Transient, Blueprintable)
class WUWA_API UWuwaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Locomotion")
	FWuwaLocomotionAnimData LocomotionData;

	/** 每个 AnimInstance 独占的采样对象；游戏线程更新。AnimGraph 请读 LocomotionData。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Animation")
	TObjectPtr<UWuwaAnimLogicParams> AnimLogicParams;


#pragma region Debug

	/** 游戏线程调试接口：指定状态机的当前逻辑状态；找不到或尚未初始化时返回 None。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FName GetDebugStateName(FName MachineName);

	/** 返回指定状态的内部权重 0..1；找不到、未初始化或没有贡献时返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	float GetDebugStateWeight(FName MachineName, FName StateName);

	/** 一次采集当前状态和所有混合状态；每个嵌套状态机需使用自己的名称查询。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FWuwaDebugAnimStateMachine GetDebugStateMachineData(FName MachineName);

	/** 可直接接 Print String 或 UMG；包含状态机权重、当前状态和各状态百分比。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FString GetDebugStateMachineText(FName MachineName);

	/** 主动调用才显示；同一实例和状态机替换同一条屏幕消息，不刷日志。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Animation|Debug", meta = (DevelopmentOnly))
	void PrintDebugStateMachine(FName MachineName, float Duration = 0.1f);

#pragma endregion Debug

protected:
	/** 以下阈值是 Demo 起点，不是从原作还原的数值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", Units = "cm/s"))
	float MovingEnterThreshold = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", Units = "cm/s"))
	float MovingExitThreshold = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float MoveIntentThreshold = 0.01f;

private:
	/** 不长期强引用旧 Pawn；切人、重新初始化和预览无 Pawn 时会清空快照。 */
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UWuwaMovementComponent> CachedMovement;
	void ResetAnimationData();
};
