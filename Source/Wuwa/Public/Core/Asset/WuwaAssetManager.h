// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "WuwaAssetManager.generated.h"

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaAssetManager : public UAssetManager
{
	GENERATED_BODY()
	
	static  UWuwaAssetManager& Get();
protected:
	virtual  void StartInitialLoading() override;
	
};
