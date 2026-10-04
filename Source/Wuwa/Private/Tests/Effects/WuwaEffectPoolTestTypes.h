#pragma once

#include "Game/Effect/Pools/Base/WuwaEffectSpecPool.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "WuwaEffectPoolTestTypes.generated.h"

/** 只用于验证同步完成、失败和容量边界，不代替真实 Audio 测试。 */
UCLASS(Transient, NotBlueprintable)
class UWuwaEffectPoolTestModel : public UWuwaEffectModelBase
{
	GENERATED_BODY()
public:
	bool bFailToPlay = false;
	bool bFinishInsidePlay = false;
	TWeakObjectPtr<UWuwaEffectSpec> LastPlayed;
};

UCLASS(Transient, NotBlueprintable)
class UWuwaEffectPoolTestSpec : public UWuwaEffectSpec
{
	GENERATED_BODY()
public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override
	{
		auto* Model = CastChecked<UWuwaEffectPoolTestModel>(Request.Model);
		Model->LastPlayed = this;
		const bool bResult = !Model->bFailToPlay;
		if (Model->bFinishInsidePlay) Finish();
		return bResult;
	}
};

UCLASS(Transient, NotBlueprintable)
class UWuwaEffectPoolTestPool : public UWuwaEffectSpecPool
{
	GENERATED_BODY()
public:
	UWuwaEffectPoolTestPool() { MaxIdleSpecs = 2; }
	int32 CreatedCount = 0;
	int32 DiscardedCount = 0;
protected:
	virtual UWuwaEffectSpec* CreateSpec_Implementation() override
	{
		++CreatedCount;
		return Super::CreateSpec_Implementation();
	}
	virtual void DestroySpec_Implementation(UWuwaEffectSpec* Spec) override
	{
		++DiscardedCount;
	}
};
