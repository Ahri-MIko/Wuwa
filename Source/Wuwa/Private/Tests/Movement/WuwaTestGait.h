#pragma once

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"

/** 测试用：冲刺需求和走跑偏好不再经 CMC 转发，测试直接操作角色身上的 RoleGait / UnifiedState。 */
namespace WuwaTestGait
{
	/** 按 CMC 找到同一角色的 RoleGait。只有测试这样查找；正式代码由角色组装时注入。 */
	inline UWuwaRoleGaitBridgeComponent* Of(const UWuwaMovementComponent* Movement)
	{
		const AWuwaCharacter* Character = Movement ? Cast<AWuwaCharacter>(Movement->GetOwner()) : nullptr;
		check(Character && Character->RoleGaitComponent);
		return Character->RoleGaitComponent;
	}

	inline UWuwaUnifiedStateBridgeComponent* StateOf(const UWuwaMovementComponent* Movement)
	{
		const AWuwaCharacter* Character = Movement ? Cast<AWuwaCharacter>(Movement->GetOwner()) : nullptr;
		check(Character && Character->UnifiedStateComponent);
		return Character->UnifiedStateComponent;
	}

	/** 走/跑：写入偏好并立刻重算一次（测试不跑 Tick）；Sprint：记一次长期冲刺请求，不改偏好。 */
	inline void SetDesiredGait(const UWuwaMovementComponent* Movement, EWuwaGait Gait)
	{
		if (Gait == EWuwaGait::Sprint)
		{
			Of(Movement)->RequestSprint();
		}
		else if (StateOf(Movement)->SetWalkPreference(Gait == EWuwaGait::Walk))
		{
			Of(Movement)->RefreshPolicy();
		}
	}

	/** 按一次走跑键：允许时切换偏好，再立刻重算一次（测试不跑 Tick）。 */
	inline void ToggleWalkPreference(const UWuwaMovementComponent* Movement)
	{
		if (Movement->CanToggleWalkPreference() && StateOf(Movement)->ToggleWalkPreference())
		{
			Of(Movement)->RefreshPolicy();
		}
	}

	/** 以 CMC 作为限制来源开关冲刺；只解除这一个来源的限制。 */
	inline void SetSprintAllowed(UWuwaMovementComponent* Movement, bool bAllowed)
	{
		Of(Movement)->SetGaitBlocked(Movement, EWuwaGait::Sprint, !bAllowed);
	}
}
