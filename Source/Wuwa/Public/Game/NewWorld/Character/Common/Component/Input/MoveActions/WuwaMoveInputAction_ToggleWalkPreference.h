#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputConfig.h"
#include "WuwaMoveInputAction_ToggleWalkPreference.generated.h"

/**
 * 走跑键（原作 WalkPress）：CMC 允许时切换运动状态组件里的走跑偏好。
 * 只改偏好；本帧 CMC Tick 开头 RoleGait 再按新偏好决定移动状态。
 */
UCLASS(meta = (DisplayName = "切换走跑偏好"))
class WUWA_API UWuwaMoveInputAction_ToggleWalkPreference : public UWuwaMoveInputAction
{
	GENERATED_BODY()
public:
	virtual bool Execute_Implementation(const FWuwaMoveInputContext& Context) const override;
};
