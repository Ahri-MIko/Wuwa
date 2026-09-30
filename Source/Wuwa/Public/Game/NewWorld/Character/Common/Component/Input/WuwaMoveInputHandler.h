// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommand.h"
#include "WuwaMoveInputHandler.generated.h"

class UWuwaMovementComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveInput, const FInputActionValue&, Value);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaMoveInputHandler : public UActorComponent,public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWuwaMoveInputHandler();

public:
	//把输入指令打包给CMC执行
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
	
	// 第一步：输入 + 当前角色上下文 -> 命令。此函数不修改角色或动画。
	static FWuwaInputCommand ResolveCommand(
		const FWuwaInputEvent& InputEvent, const UWuwaMovementComponent* Movement);
	
	#pragma  region 持续输入意图
	UPROPERTY(BlueprintAssignable, Category = "Wuwa|Input")
	FOnMoveInput OnMove;
	
	#pragma endregion
	
};
