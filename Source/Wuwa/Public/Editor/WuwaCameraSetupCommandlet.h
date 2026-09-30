#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCameraSetupCommandlet.generated.h"

/** Project-specific, opt-in migration of the existing input and controller assets. */
UCLASS()
class WUWA_API UWuwaCameraSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaCameraSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
