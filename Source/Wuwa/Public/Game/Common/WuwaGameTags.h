#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

//当前使用的输入是输入指令触发后会根据InputAction查找对应的标签然后讲输入信息打包根据Handler标签进行转发对应职能的模块
struct WUWA_API FWuwaGameTags
{
	const static  FWuwaGameTags Get() {return WuwaGamePlayTags;}
	
	static void InitializeGameTags();
	
	//GA
	FGameplayTag Abilities_Movement_Dash;
	FGameplayTag Input_Combat_Attack;
	
	
	//State_Common
	FGameplayTag Player_Common_Movement_Move;
	FGameplayTag Player_Common_Movement_WalkRun;
	FGameplayTag Player_Common_Camera_Rotate;
	FGameplayTag Player_Common_Camera_Zoom;
	
	
	
	//InputHandler
	FGameplayTag Input_Route_Ability;
	FGameplayTag Input_Route_Movement;
	FGameplayTag Input_Route_Camera;
	
	
public: 
	static FWuwaGameTags WuwaGamePlayTags;
};
