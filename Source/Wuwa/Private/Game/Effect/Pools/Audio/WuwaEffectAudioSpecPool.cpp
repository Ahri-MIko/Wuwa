#include "Game/Effect/Pools/Audio/WuwaEffectAudioSpecPool.h"
#include "Game/Effect/Specs/Audio/WuwaEffectAudioSpec.h"

UWuwaEffectSpec* UWuwaEffectAudioSpecPool::CreateSpec_Implementation()
{
	return SpecClass && SpecClass->IsChildOf(UWuwaEffectAudioSpec::StaticClass())
		? Super::CreateSpec_Implementation() : nullptr;
}

void UWuwaEffectAudioSpecPool::ResetSpec_Implementation(UWuwaEffectSpec* Spec)
{
	CastChecked<UWuwaEffectAudioSpec>(Spec)->ResetForPool();
}

void UWuwaEffectAudioSpecPool::DestroySpec_Implementation(UWuwaEffectSpec* Spec)
{
	CastChecked<UWuwaEffectAudioSpec>(Spec)->ReleaseAudioComponent();
}
