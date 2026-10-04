#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "WuwaEffectModelBillboard.generated.h"

/**
 * 对应原作 EffectModelBillboard：交给 KuroBillboardComponent 的朝向/尺寸控制配置。
 * 原作 Spec 没有读取贴图或材质字段，因此这里不凭空增加 Sprite/Material。
 * 当前没有该原生组件；这里仅保存可核实字段，初始化值是项目默认值。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelBillboard : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard")
	bool IsUpdateEveryFrame = true;

	/**
	 * 原作轴选择字段。导出脚本仅做原样传递，尚无法确认原生字段类型与枚举值含义。
	 * 暂以整数保存配置值，待补齐原生资料再解释；不能按 UE EAxis 猜测对应关系。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (DisplayName = "Orient Axis (Unverified Native Mapping)"))
	int32 OrientAxis = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard")
	bool IsFixSize = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (ClampMin = "0"))
	float ScaleSize = 1.f;

	/** 原作传给 Billboard 组件的距离参数；不在数据层推断其裁剪/缩放公式。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (ClampMin = "0"))
	float MaxDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Billboard", meta = (ClampMin = "0"))
	float MinSize = 0.f;
};
