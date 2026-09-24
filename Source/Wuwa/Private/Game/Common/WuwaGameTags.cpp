#include "Game/Common/WuwaGameTags.h"
#include "GameplayTagsManager.h"

//声明类静态成员变量必须有这个
FWuwaGameTags FWuwaGameTags::WuwaGamePlayTags;

void FWuwaGameTags::InitializeGameTags()
{
	WuwaGamePlayTags.Abilities_Movement_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Movement.Dash"),
		FString("玩家冲刺能力")
		);
	
	WuwaGamePlayTags.Input_Route_Ability = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Route.Ability"),
		FString("能力系统输入句柄")
		);
	WuwaGamePlayTags.Input_Route_Movement = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Input.Route.Movement"),
		FString("玩家移动输入句柄")
		);
	WuwaGamePlayTags.Player_Common_Movement_Move = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Common.Movement.Move"),
		FString("玩家移动")
		);
	WuwaGamePlayTags.Player_Common_Movement_WalkRun = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Common.Movement.WalkRun"),
		FString("请求切换走路/跑步，不表示当前步态")
		);
	
	WuwaGamePlayTags.Player_Common_Camera_Rotate = UGameplayTagsManager::Get().AddNativeGameplayTag(
	FName("Player.Common.Camera.Rotate"),
	FString("玩家移动")
	);
}
