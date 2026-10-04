#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputConfig.h"

// 基类不执行任何指令；具体行为由子类实现。
bool UWuwaMoveInputAction::Execute_Implementation(const FWuwaMoveInputContext&) const { return false; }

const FWuwaMoveInputBinding* UWuwaMoveInputConfig::FindBinding(const FGameplayTag& InputTag, EWuwaInputPhase Phase) const
{
	return Bindings.FindByPredicate([&InputTag, Phase](const FWuwaMoveInputBinding& Binding)
	{
		return Binding.InputTag == InputTag && Binding.Phase == Phase;
	});
}
