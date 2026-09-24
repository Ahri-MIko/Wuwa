// WuwaInputTypes.h
// 输入系统 C++ 侧类型定义(与蓝图侧 EInputState / SInputHoldConfig 镜像)

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputAction.h"

// generated.h 必须是最后一个 include
#include "WuwaInputTypes.generated.h"

UENUM(BlueprintType)
enum class EWuwaInputPhase : uint8
{
	Pressed,
	Triggered,
	Released,
	Canceled
};

USTRUCT(BlueprintType)
struct FWuwaInputEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly)
	EWuwaInputPhase Phase = EWuwaInputPhase::Canceled;

	UPROPERTY(BlueprintReadOnly)
	FInputActionValue Value;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<const UInputAction> SourceAction = nullptr;

	UPROPERTY(BlueprintReadOnly)
	double Timestamp = 0.0;
};

/** 路由中的语义输入状态，与具体物理键、设备以及 GA 是否激活无关。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputActionState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	bool bHeld = false;

	/** 与 FWuwaInputEvent.Timestamp 使用同一游戏时钟。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	float HeldSeconds = 0.f;
};


/**
 * 输入事件三态,C++ 侧镜像蓝图的 EInputState。
 * 枚举值顺序刻意与蓝图 EInputState 保持一致(None/Press/Release/Hold),
 * 这样两侧如果需要 byte 级互转,数值直接对齐,不需要映射表。
 * None 仅用于占位对齐,组件永远不会广播 None。
 */
UENUM(BlueprintType)
enum class EWuwaInputEventType : uint8
{
	None = 0,
	Press = 1,
	Release = 2,
	Hold = 3,
};



/**
 * 单个动作的长按配置,镜像蓝图的 SInputHoldConfig{触发时间, 连续触发}。
 * 约定:TriggerTime <= 0 表示该动作没有长按行为,分类器不做 Hold 判定。
 */
USTRUCT(BlueprintType)
struct FWuwaHoldConfig
{
	GENERATED_BODY()

	/** 按住多少秒后触发长按(<=0 表示无长按) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	float TriggerTime = 0.f;

	/** true = 到达阈值后按周期(每 TriggerTime 秒)重复广播;false = 只广播一次 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	bool bRepeat = false;
};

/**
 * 单个动作的运行时按键状态(你设计的那张状态表的一行)。
 * 只存两个时间戳,不做逐帧累加,时长永远用 Now - PressTime 现算。
 */
USTRUCT()
struct FWuwaKeyState
{
	GENERATED_BODY()

	/** 当前是否按着 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	bool bDown = false;

	/** 按下时刻(GetWorld()->GetTimeSeconds()) */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	float PressTime = 0.f;

	/**
	 * 下一次长按触发的时刻(绝对时间)。
	 * < 0 表示当前没有待触发的长按:要么该动作无长按配置,
	 * 要么 bRepeat=false 且已经触发过一次。
	 * Tick 扫描条件:bDown && NextHoldTime >= 0 && Now >= NextHoldTime
	 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	float NextHoldTime = -1.f;
};
