// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "WuwaInputTypes.h"
#include "WuwaInputComponent.generated.h"

// 前置声明
class UInputMappingContext;
class UInputAction;
class UWuwaInputConfig;
struct FInputActionInstance;


// 按键输入:统一签名 —— 事件类型(按下/长按/抬起) + 按住时长(秒)
// Press 时 Time 恒为 0;Hold / Release 时为从按下起累计的时长
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWuwaActionInput, EWuwaInputEventType, EventType, float, Time);

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaInputComponent();

	// ---------- 对蓝图广播的委托 ----------
	
	//每一个按键都创建一个委托,等待被注册
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnJump;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnAttack;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnDodge;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnClimb;
	// 仅保留旧资产兼容性，不再绑定或广播。走跑切换统一经过 InputRouter。
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input", meta = (DeprecatedProperty, DeprecationMessage = "WalkRun now uses InputRouter / MoveInputHandler."))
	FOnWuwaActionInput OnWalkRun;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnSkill1;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnEcho1;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnUltimate;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnSwitchChar1;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnSwitchChar2;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnSwitchChar3;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnLockOn;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnAim;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnInteract;
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input") FOnWuwaActionInput OnDescend;

	// ---------- BPL_Input 的等价查询(蓝图侧跨键逻辑用) ----------

	/** 某个动作当前是否按着 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Wuwa|Input")
	bool IsKeyDown(const UInputAction* Action) const;

	/** 某个动作已按住多久(秒),没按着返回 0 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Wuwa|Input")
	float GetKeyDownTime(const UInputAction* Action) const;

	
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	// ---------- 配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 长按配置表(DA 资产,IA → {触发时间, 连续触发}) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UWuwaInputConfig> InputConfig;

	
	//所有的IA都在这里了
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> AttackAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> DodgeAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> ClimbAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input", meta = (DeprecatedProperty, DeprecationMessage = "Configure IA_WalkRun in InputTagMap instead."))
	TObjectPtr<UInputAction> WalkRunAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> Skill1Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> Echo1Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> UltimateAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> SwitchChar1Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> SwitchChar2Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> SwitchChar3Action;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> LockOnAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input") TObjectPtr<UInputAction> DescendAction;

	// ---------- 运行时状态 ----------

	/** 按键状态表:每个按键动作一行 {bDown, PressTime, NextHoldTime} */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TMap<TObjectPtr<const UInputAction>, FWuwaKeyState> KeyStates;

	/** IA → 对应委托 的分发表,BeginPlay 时建立(委托成员不能进 UPROPERTY Map,存裸指针) */
	TMap<const UInputAction*, FOnWuwaActionInput*> DelegateMap;

	// ---------- 初始化 ----------

	// 注册 IMC 到 EnhancedInputSubsystem
	void RegisterMappingContext();

	// 绑定所有 IA:按键动作统一绑到 HandleActionStarted / HandleActionCompleted
	void BindInputActions();

	// ---------- 输入处理 ----------

	// 轴输入透传
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);

	// 所有按键动作共用的两个入口,通过 Instance.GetSourceAction() 区分是哪个键
	void HandleActionStarted(const FInputActionInstance& Instance);
	void HandleActionCompleted(const FInputActionInstance& Instance);

	// 找到对应委托并广播
	void BroadcastActionEvent(const UInputAction* Action, EWuwaInputEventType EventType, float Time);
};
