#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaInputHoldSetupCommandlet.generated.h"

/**
 * 默认只检查：列出 IMC_Character 用到的每个 IA 及映射上的触发器。
 * -Apply：给没有 Hold 触发器的按键（Boolean）IA 加 Hold（0.5 秒、非一次性），已有的不改。
 */
UCLASS()
class WUWA_API UWuwaInputHoldSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaInputHoldSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
