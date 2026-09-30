#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaMovementStateEditorLibrary.generated.h"

/** Explicit editor migration; never runs during gameplay or asset loading. */
UCLASS()
class WUWA_API UWuwaMovementStateEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Reconnect Changli ground animation and opt its Dash GA into the Dodge state override.
	 * Both Blueprints must compile successfully before either is saved.
	 * bSave=false changes the loaded asset in memory only. The caller must back up assets
	 * and resolve any unsaved editor work before using bSave=true. Returns ERROR on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Movement")
	static FString MigrateGroundStateAssets(bool bSave = false);
};
