#pragma once

#include "CoreMinimal.h"
#include "WuwaPlayerInputState.generated.h"

/** 当前角色的输入意图快照；数据来自输入系统，不读取物理键或从角色速度反推。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaPlayerInputState
{
	GENERATED_BODY()

	/** 最近收到的 Enhanced Input 移动轴：X 左右，Y 前后；保留摇杆幅度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	FVector2D MoveAxis = FVector2D::ZeroVector;

	/** 按当前相机 Yaw 转换后的水平世界方向，已归一化；无有效输入时为零。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	FVector MoveWorldDirection = FVector::ZeroVector;

	/** 移动轴是否超过角色配置的输入阈值；不受角色速度或移动许可影响。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	bool bHasMoveInput = false;

	/** 冲刺语义指令是否仍按住，来自 Controller 的 InputRouter。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	bool bSprintHeld = false;

	/** 当前连续按住时长（游戏秒）；尚未按下或已经取消时为零。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	float SprintHeldSeconds = 0.f;
};
