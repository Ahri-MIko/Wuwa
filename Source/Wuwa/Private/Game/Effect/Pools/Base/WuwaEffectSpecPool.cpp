#include "Game/Effect/Pools/Base/WuwaEffectSpecPool.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"

UWorld* UWuwaEffectSpecPool::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UWuwaEffectSpecPool::Initialize(TSubclassOf<UWuwaEffectSpec> InSpecClass)
{
	SpecClass = InSpecClass;
}

UWuwaEffectSpec* UWuwaEffectSpecPool::Acquire()
{
	if (bClosed) return nullptr;
	UWuwaEffectSpec* Spec = IdleSpecs.IsEmpty() ? CreateSpec() : IdleSpecs.Pop(EAllowShrinking::No).Get();
	if (!Spec) return nullptr;
	Spec->PrepareForPlay();
	InUseSpecs.Add(Spec);
	return Spec;
}

void UWuwaEffectSpecPool::Release(UWuwaEffectSpec* Spec)
{
	// 防止重复归还，或误把其他池的对象放进来。
	if (!InUseSpecs.Remove(Spec)) return;
	Spec->OnFinished.Unbind();
	ResetSpec(Spec);
	if (!bClosed && IdleSpecs.Num() < MaxIdleSpecs)
	{
		IdleSpecs.Add(Spec);
	}
	else
	{
		DestroySpec(Spec);
	}
}

void UWuwaEffectSpecPool::Clear()
{
	auto DiscardingSpecs = MoveTemp(IdleSpecs);
	for (UWuwaEffectSpec* Spec : DiscardingSpecs)
	{
		DestroySpec(Spec);
	}
}

void UWuwaEffectSpecPool::Close()
{
	bClosed = true;
	Clear();
}

UWuwaEffectSpec* UWuwaEffectSpecPool::CreateSpec_Implementation()
{
	return SpecClass ? NewObject<UWuwaEffectSpec>(this, SpecClass) : nullptr;
}

void UWuwaEffectSpecPool::ResetSpec_Implementation(UWuwaEffectSpec* Spec)
{
	// 默认执行器使用 Stop 清理。持有可复用资源的执行器应提供专用池。
	Spec->Stop(true);
}

void UWuwaEffectSpecPool::DestroySpec_Implementation(UWuwaEffectSpec* Spec)
{
}
