#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectModelSkeletalMesh.generated.h"

class UAnimationAsset;
class USkeletalMesh;

/**
 * 对应原作 EffectModelSkeletalMesh：例如技能中带骨骼动画的武器、道具。
 * 字段依据 EffectModelSkeletalMeshSpec 与 UsedInfo；以下初始化值是项目默认值。
 * 角色的实际 SkeletalMeshComponent 不属于这个共享资产。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelSkeletalMesh : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMeshRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimationAsset> AnimationRef;

	/** 指动画本身循环；Model 的循环播放段仍由基类 LoopTime 描述。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	bool Looping = false;

	/** 开始播放时暂缓显示的帧数，原作用于等待骨骼姿态准备完成。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0"))
	int32 HideFrames = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
	bool EnableCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
	bool CastShadow = false;

	/** 库洛卡通阴影扩展选项；保存配置不代表本项目已实现该渲染功能。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|Kuro Extension")
	bool ForbidCastToonShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Location;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transform")
	FWuwaEffectVectorCurve Scale{FVector::OneVector};
};
