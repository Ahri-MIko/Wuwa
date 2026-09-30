#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "WuwaRoleGaitBridgeComponent.generated.h"

/** 原生只提供生命周期与访问桥；输入规则、计时、禁止来源均由 C# 持有。 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Wuwa), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaRoleGaitBridgeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void InitializePolicy();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void RefreshPolicy();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") bool RequestDesiredGait(EWuwaGait NewGait);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") bool RequestWalkRunToggle();
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait") bool CanRequestWalkRun() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void SetGaitBlocked(UObject* Source, EWuwaGait Gait, bool bBlocked);
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait") bool IsGaitAllowed(EWuwaGait Gait) const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void OpenSprintWindow(UObject* Source);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void SampleSprintWindow(UObject* Source, float InputHeldSeconds, float HoldThresholdSeconds);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void CloseSprintWindow(UObject* Source);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void ResetSprintRequest();
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") EWuwaSprintDesire ReadSprintDesire() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void ResetRuntime();

	UFUNCTION(BlueprintPure, Category="Wuwa|Gait") FWuwaMovementStateContext ReadMovementContext() const;
	UFUNCTION(BlueprintCallable, Category="Wuwa|Gait", meta=(BlueprintProtected))
	void PublishGait(EWuwaGait Desired, EWuwaSprintDesire Desire, EWuwaGait LastMovingGait);
	UFUNCTION() void HandleUnifiedStateChanged(const FWuwaUnifiedStateData& OldState, const FWuwaUnifiedStateData& NewState);

	//这三个给动画读取
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Gait") EWuwaGait DesiredGait = EWuwaGait::Run;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Gait") EWuwaSprintDesire SprintDesire = EWuwaSprintDesire::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Gait") EWuwaGait StopGait = EWuwaGait::Run;
};
