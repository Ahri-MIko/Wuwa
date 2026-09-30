#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaAttackMontageExitRepairCommandlet.generated.h"

/** Narrow, opt-in repair of the five authored Changli attack montage-task exits. */
UCLASS()
class WUWA_API UWuwaAttackMontageExitRepairCommandlet : public UCommandlet
{

	GENERATED_BODY()

public:
	UWuwaAttackMontageExitRepairCommandlet();
	virtual int32 Main(const FString& Params) override;
};
