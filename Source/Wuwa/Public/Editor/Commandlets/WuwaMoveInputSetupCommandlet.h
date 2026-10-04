#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaMoveInputSetupCommandlet.generated.h"

/** 默认只检查；-Apply 在资产不存在时创建移动输入配置表（走跑键 -> 切换走跑偏好），已存在的资产不覆盖。 */
UCLASS()
class WUWA_API UWuwaMoveInputSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaMoveInputSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
