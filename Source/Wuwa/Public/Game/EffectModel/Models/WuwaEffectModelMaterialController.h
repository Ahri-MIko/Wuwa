#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "UObject/SoftObjectPath.h"
#include "WuwaEffectModelMaterialController.generated.h"

/**
 * 对应原作 EffectModelMaterialController 的两种材质控制器资源引用。
 * 原作 Spec 把资源交给目标角色的 CharRenderingComponent，并在停止时移除返回的控制器句柄。
 * 本项目尚未实现该材质控制器后端；这里仅保存资源路径，不把它伪装成 MaterialInstance。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelMaterialController : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	/** 原作单个角色材质控制器数据；相关通知引用 PD_CharacterControllerData_C。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FSoftObjectPath MaterialControllerData;

	/** 原作控制器组数据；相关通知引用 PD_CharacterControllerDataGroup_C。允许与单个控制器同时配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FSoftObjectPath MaterialControllerGroupData;
};
