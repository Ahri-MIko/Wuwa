#include "Game/Effect/Factory/WuwaEffectSpecFactory.h"
#include "Game/Effect/Pools/Base/WuwaEffectSpecPool.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"

UWorld* UWuwaEffectSpecFactory::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool UWuwaEffectSpecFactory::Register(TSubclassOf<UWuwaEffectModelBase> ModelClass,TSubclassOf<UWuwaEffectSpec> SpecClass, TSubclassOf<UWuwaEffectSpecPool> PoolClass)
{
	if (!PoolClass) PoolClass = UWuwaEffectSpecPool::StaticClass();
	if (!ModelClass || !SpecClass || SpecClass->HasAnyClassFlags(CLASS_Abstract)
		|| PoolClass->HasAnyClassFlags(CLASS_Abstract)) return false;
	if (const auto* Existing = Pools.Find(ModelClass))
	{
		if ((*Existing)->GetSpecClass() == SpecClass && (*Existing)->GetClass() == PoolClass) return true;
		// 正在播放的项由 System 保留原池引用；结束后仍归还原池，不会混入新注册。
		(*Existing)->Close();
	}
	UWuwaEffectSpecPool* Pool = NewObject<UWuwaEffectSpecPool>(this, PoolClass);
	Pool->Initialize(SpecClass);
	Pools.Add(ModelClass, Pool);
	return true;
}

UWuwaEffectSpecPool* UWuwaEffectSpecFactory::FindPool(const UWuwaEffectModelBase* Model) const
{
	// 自定义 Model 子类没有单独注册时，沿父类查找已有执行器。
	for (UClass* Type = Model->GetClass(); Type; Type = Type->GetSuperClass())
	{
		if (const auto* Pool = Pools.Find(Type))
		{
			return Pool->Get();
		}
	}
	return nullptr;
}

int32 UWuwaEffectSpecFactory::GetIdleCount() const
{
	int32 Count = 0;
	for (const auto& Entry : Pools) Count += Entry.Value->GetIdleCount();
	return Count;
}

void UWuwaEffectSpecFactory::ClearPools()
{
	for (const auto& Entry : Pools) Entry.Value->Clear();
}

void UWuwaEffectSpecFactory::Close()
{
	for (const auto& Entry : Pools) Entry.Value->Close();
	Pools.Reset();
}
