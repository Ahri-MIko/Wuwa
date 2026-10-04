// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "WuwaMoveInputHandler.generated.h"

class UWuwaMoveInputConfig;

/**
 * 处理移动类的一次性指令（比如走跑切换）：按配置表把语义输入映射到指令对象执行，本身不含任何按键分支。
 * 移动轴不经过这里，由角色的输入意图组件记录、CMC 每帧读取。
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaMoveInputHandler : public UActorComponent,public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:
	UWuwaMoveInputHandler();

	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;

	/** 移动类输入 -> 指令的配置表；默认指向项目里的 DA_WuwaMoveInputConfig，可在蓝图里替换。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Input")
	TSoftObjectPtr<UWuwaMoveInputConfig> Config;

private:
	// 移动轴每帧都会经过这里，配置缺失只提示一次。
	bool bReportedMissingConfig = false;
};
