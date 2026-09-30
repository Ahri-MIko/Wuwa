#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSlashFxImportCommandlet.generated.h"

/** Imports the audited first-attack source art; keeps gameplay and the shared FX playback path. */
UCLASS()
class WUWA_API UWuwaSlashFxImportCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UWuwaSlashFxImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
