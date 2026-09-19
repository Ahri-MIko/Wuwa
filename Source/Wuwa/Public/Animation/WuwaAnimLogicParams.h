#pragma once

#include "CoreMinimal.h"
#include "Animation/WuwaAnimDataTypes.h"
#include "UObject/Object.h"
#include "WuwaAnimLogicParams.generated.h"

/**
 * 参考原作 BP_ABPLogicParams 的参数对象职责，不复制未知实现。
 * 每个 AnimInstance 独立拥有一个实例；只在游戏线程采集、调试。
 * AnimGraph / 工作线程读取 AnimInstance.LocomotionData，不解引用这个对象。
 */
UCLASS(BlueprintType)
class WUWA_API UWuwaAnimLogicParams : public UObject
{
	GENERATED_BODY()

public:
	const FWuwaAnimMoveData& GetMoveData() const { return MoveData; }
	const FWuwaAnimStateData& GetStateData() const { return StateData; }
	bool HasValidData() const { return bHasValidData; }
	void Reset();

private:
	friend class UWuwaAnimDataLibrary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Animation", meta = (AllowPrivateAccess = "true"))
	bool bHasValidData = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Animation", meta = (AllowPrivateAccess = "true"))
	FWuwaAnimMoveData MoveData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Animation", meta = (AllowPrivateAccess = "true"))
	FWuwaAnimStateData StateData;
};
