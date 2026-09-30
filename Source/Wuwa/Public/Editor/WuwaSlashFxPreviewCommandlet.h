#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSlashFxPreviewCommandlet.generated.h"

/** Captures the authored Niagara presets with the real renderer; never changes assets. */
UCLASS()
class WUWA_API UWuwaSlashFxPreviewCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UWuwaSlashFxPreviewCommandlet();
	virtual int32 Main(const FString& Params) override;
};
