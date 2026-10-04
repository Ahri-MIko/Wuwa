#pragma once

#include "CoreMinimal.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "WuwaEffectSpec.generated.h"

/** 一次播放的执行对象。子类读取各自的 Model，持有运行资源，结束时调用 Finish。 */
UCLASS(Abstract, BlueprintType, Blueprintable, Transient)
class WUWA_API UWuwaEffectSpec : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	/** 仅由池取出时调用；同一个执行对象可以先后服务不同播放句柄。 */
	void PrepareForPlay();

	UFUNCTION(BlueprintNativeEvent, Category = "Effect")
	bool Play(const FWuwaEffectSpawnRequest& Request);
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request);

	UFUNCTION(BlueprintNativeEvent, Category = "Effect")
	void Stop(bool bImmediately);
	virtual void Stop_Implementation(bool bImmediately);

	bool IsFinished() const { return bFinished; }
	FSimpleDelegate OnFinished;

protected:
	/** 自然结束或停止完成后调用一次，通知 System 释放本次句柄。 */
	UFUNCTION(BlueprintCallable, Category = "Effect", meta = (BlueprintProtected))
	void Finish();

private:
	bool bFinished = false;
};
