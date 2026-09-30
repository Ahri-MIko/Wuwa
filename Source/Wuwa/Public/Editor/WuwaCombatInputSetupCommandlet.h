#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCombatInputSetupCommandlet.generated.h"

/** Read-only by default; -Apply configures the five existing Changli attack abilities. */
UCLASS()
class WUWA_API UWuwaCombatInputSetupCommandlet : public UCommandlet
{

	GENERATED_BODY()

public:
	UWuwaCombatInputSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
