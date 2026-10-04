#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"

UWorld* UWuwaEffectSpec::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UWuwaEffectSpec::PrepareForPlay()
{
	OnFinished.Unbind();
	bFinished = false;
}

bool UWuwaEffectSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	return false;
}

void UWuwaEffectSpec::Stop_Implementation(bool bImmediately)
{
	Finish();
}

void UWuwaEffectSpec::Finish()
{
	if (bFinished) return;
	bFinished = true;
	// 回调可能立即归还对象。先移出委托，回调之后不再修改本次 Spec。
	FSimpleDelegate FinishedCallback = MoveTemp(OnFinished);
	FinishedCallback.ExecuteIfBound();
}
