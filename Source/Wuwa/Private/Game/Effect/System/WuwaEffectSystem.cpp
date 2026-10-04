#include "Game/Effect/System/WuwaEffectSystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Effect/Factory/WuwaEffectSpecFactory.h"
#include "Game/Effect/Pools/Audio/WuwaEffectAudioSpecPool.h"
#include "Game/Effect/Specs/Audio/WuwaEffectAudioSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelAudio.h"

void UWuwaEffectSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Factory = NewObject<UWuwaEffectSpecFactory>(this);
	RegisterEffectSpec(UWuwaEffectModelAudio::StaticClass(), UWuwaEffectAudioSpec::StaticClass(),
		UWuwaEffectAudioSpecPool::StaticClass());
}

bool UWuwaEffectSystem::RegisterEffectSpec(TSubclassOf<UWuwaEffectModelBase> ModelClass,
	TSubclassOf<UWuwaEffectSpec> SpecClass, TSubclassOf<UWuwaEffectSpecPool> PoolClass)
{
	return !bShuttingDown && Factory && Factory->Register(ModelClass, SpecClass, PoolClass);
}

bool UWuwaEffectSystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UWuwaEffectSystem* UWuwaEffectSystem::GetEffectSystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
		EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaEffectSystem>() : nullptr;
}


FWuwaEffectHandle UWuwaEffectSystem::SpawnEffect(const FWuwaEffectSpawnRequest& Request)
{
	if (bShuttingDown || !IsValid(Request.Model) || !Factory) return {};
	UWuwaEffectSpecPool* Pool = Factory->FindPool(Request.Model);
	if (!Pool)
	{
		UE_LOG(LogTemp, Warning, TEXT("EffectSystem: no Spec registered for %s (%s)."),
			*Request.Model->GetClass()->GetName(), *Request.Model->GetName());
		return {};
	}
	UWuwaEffectSpec* Spec = Pool->Acquire();
	if (!Spec) return {};

	FWuwaEffectHandle Handle;
	Handle.Id = FGuid::NewGuid();
	// 先登记再播放，使同步播放失败/结束也能通过同一回调正确释放。
	ActiveEffects.Add(Handle.Id, {Spec, Pool});
	Spec->OnFinished.BindUObject(this, &ThisClass::HandleFinished, Handle.Id);
	if (!Spec->Play(Request))
	{
		// Play 可能已同步 Finish 并归还，不能再操作这个已被回收的 Spec。
		if (ActiveEffects.Contains(Handle.Id))
		{
			Spec->Stop(true);
			HandleFinished(Handle.Id);
		}
		return {};
	}
	return IsEffectActive(Handle) ? Handle : FWuwaEffectHandle{};
}

bool UWuwaEffectSystem::StopEffect(FWuwaEffectHandle Handle, bool bImmediately)
{
	const auto* Entry = ActiveEffects.Find(Handle.Id);
	if (!Entry) return false;
	UWuwaEffectSpec* Spec = Entry->Spec;
	Spec->Stop(bImmediately);
	return true;
}

bool UWuwaEffectSystem::IsEffectActive(FWuwaEffectHandle Handle) const
{
	return ActiveEffects.Contains(Handle.Id);
}

void UWuwaEffectSystem::HandleFinished(FGuid Id)
{
	FWuwaActiveEffect Finished;
	if (ActiveEffects.RemoveAndCopyValue(Id, Finished))
	{
		Finished.Pool->Release(Finished.Spec);
	}
}

int32 UWuwaEffectSystem::GetPooledEffectCount() const
{
	return Factory ? Factory->GetIdleCount() : 0;
}

void UWuwaEffectSystem::ClearEffectPools()
{
	if (Factory) Factory->ClearPools();
}

void UWuwaEffectSystem::StopAllEffects()
{
	// Stop 可同步回调 HandleFinished；先移出表，避免在遍历中修改同一容器。
	auto StoppingEffects = MoveTemp(ActiveEffects);
	for (const auto& Entry : StoppingEffects)
	{
		Entry.Value.Spec->OnFinished.Unbind();
		Entry.Value.Spec->Stop(true);
		Entry.Value.Pool->Release(Entry.Value.Spec);
	}
}

void UWuwaEffectSystem::OnWorldEndPlay(UWorld& InWorld)
{
	bShuttingDown = true;
	StopAllEffects();
	if (Factory) Factory->Close();
	Super::OnWorldEndPlay(InWorld);
}

void UWuwaEffectSystem::Deinitialize()
{
	bShuttingDown = true;
	StopAllEffects();
	if (Factory) Factory->Close();
	Factory = nullptr;
	Super::Deinitialize();
}
