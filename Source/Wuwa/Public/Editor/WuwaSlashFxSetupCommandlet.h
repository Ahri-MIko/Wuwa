#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSlashFxSetupCommandlet.generated.h"

/** Builds the project's original red/gold slash assets and adds only its own attack FX notifies. */
UCLASS()
class WUWA_API UWuwaSlashFxSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UWuwaSlashFxSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
