#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include "WuwaInputIntentComponent.generated.h"

class UInputAction;

/** 移动输入消化后的结果：一个移动输入对应这一份数据，读的人不需要再自己换算。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMoveIntent
{
	GENERATED_BODY()

	/** 最近收到的移动轴：X 左右，Y 前后；保留摇杆幅度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	FVector2D Axis = FVector2D::ZeroVector;

	/** 移动轴是否超过阈值；不受角色速度或移动许可影响。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	bool bHasInput = false;

	/** 按控制朝向 Yaw 转换后的水平世界方向，已归一化；没有有效输入时为零。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	FVector WorldDirection = FVector::ZeroVector;
};

/**
 * 角色的输入意图，对应原作每个角色身上的 CharacterInputComponent 保存的输入缓存。
 * 输入来源（PlayerController、将来的 AI、测试）写入，玩法系统读取；
 * 写入时把原始输入消化成按输入分类的数据（移动 -> FWuwaMoveIntent，按键 -> FWuwaInputActionState）。
 * 不引用任何控制器或玩法系统，也不在写入时回调别人。
 */
UCLASS(ClassGroup=(Wuwa), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaInputIntentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaInputIntentComponent();

#pragma region 写入
	/** 记录一次语义输入：移动指令更新移动轴，其余指令更新按住状态。 */
	void RecordInputEvent(const FWuwaInputEvent& InputEvent);

	/** 直接写入移动轴；给不经过 Enhanced Input 的输入来源（AI、测试）使用。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Input")
	void SetMoveAxis(FVector2D NewAxis);

	/** 清空全部输入意图：失去控制、窗口失焦或重新绑定输入时调用。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Input")
	void ResetInputs();
#pragma endregion

#pragma region 读取
	/** 移动意图；方向在读取时按当前控制朝向计算，转镜头后不沿用旧方向。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaMoveIntent GetMoveIntent() const;

	/** 语义输入当前是否按住，以及连续按住的游戏秒数。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaInputActionState GetActionState(FGameplayTag InputTag) const;

	/** 给 GA、通知等玩法读取的组合快照：移动意图 + 冲刺（Dash 语义输入）按住状态。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;
#pragma endregion

	/** 判断是否有移动意图的阈值，作用于 Enhanced Input 已处理过的轴值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Input", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoveInputThreshold = 0.01f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RecordActionInput(const FWuwaInputEvent& InputEvent);

	struct FHeldInput
	{
		double PressedAt = 0.0;
		// 不同 InputAction 可映射到同一个意图；释放一个来源不能解除其他来源。
		TSet<TWeakObjectPtr<const UInputAction>> ActiveSources;
	};

	TMap<FGameplayTag, FHeldInput> HeldInputs;
	FVector2D MoveAxis = FVector2D::ZeroVector;
};
