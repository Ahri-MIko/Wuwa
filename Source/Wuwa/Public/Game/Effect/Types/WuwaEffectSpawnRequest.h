#pragma once

#include "CoreMinimal.h"
#include "WuwaEffectSpawnRequest.generated.h"

class USceneComponent;
class UWuwaEffectModelBase;

/** 一次播放的输入。Model 是共享配置，其余字段描述本次播放的位置和挂接目标。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UWuwaEffectModelBase> Model;

	/** 无 AttachTo 时为世界变换；有 AttachTo 时为相对 SocketName 的变换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FTransform Transform = FTransform::Identity;

	/** 留空则在世界中播放。弱引用避免播放请求延长场景组件的生命周期。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TWeakObjectPtr<USceneComponent> AttachTo;

	/** 留空使用组件原点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName SocketName;
};
