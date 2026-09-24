#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct WUWA_API FWuwaGameTags
{
	const static  FWuwaGameTags Get() {return WuwaGamePlayTags;}
	
	static void InitializeGameTags();
	
	//GA
	FGameplayTag Abilities_Movement_Dash;
	
	
	//Common
	FGameplayTag Player_Common_Movement_Move;
	FGameplayTag Player_Common_Movement_WalkRun;
	FGameplayTag Player_Common_Camera_Rotate;
	
	
	
	//InputHandler
	FGameplayTag Input_Route_Ability;
	FGameplayTag Input_Route_Movement;
	
	
public: 
	static FWuwaGameTags WuwaGamePlayTags;
};
