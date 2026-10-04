#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "WuwaEffectModelGhost.generated.h"

class UMaterialInterface;

/** 原作 EffectModelGhostSpec.GetComponentName 中能确认的组件选择值。 */
UENUM(BlueprintType)
enum class EWuwaEffectGhostMesh : uint8
{
	Body = 0,
	WeaponCase0 = 1,
	WeaponCase1 = 2,
	WeaponCase2 = 3,
	WeaponCase3 = 4,
	WeaponCase4 = 5,
	HuluCase = 6,
	OtherCase0 = 7,
	OtherCase1 = 8,
	OtherCase2 = 9,
	OtherCase3 = 10,
	OtherCase4 = 11
};

/**
 * 对应原作 EffectModelGhost：指定哪些骨骼网格需要留下残影，以及残影材质/透明度。
 * 目标 SkeletalMeshComp、生成间隔和单个残影寿命在原作来自 EffectContext，不放入 Model。
 * 原作原生默认值没有导出；本项目默认选择 Body，透明度常量为 1。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelGhost : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	/** 原作将此材质应用到每个残影网格的所有材质槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TObjectPtr<UMaterialInterface> MaterialRef;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TArray<EWuwaEffectGhostMesh> MeshComponentsToUse{EWuwaEffectGhostMesh::Body};

	/** 除上面的标准部位外，按角色组件名称额外选择骨骼网格。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TArray<FName> CustomComponentNames;

	/** 原作采样横轴是剩余寿命比例：刚生成为 1，消失前为 0，并非已播放秒数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FWuwaEffectFloatCurve AlphaCurve{1.f};
};
