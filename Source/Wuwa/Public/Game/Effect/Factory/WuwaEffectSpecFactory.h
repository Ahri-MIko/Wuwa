#pragma once

#include "CoreMinimal.h"
#include "WuwaEffectSpecFactory.generated.h"

class UWuwaEffectModelBase;
class UWuwaEffectSpec;
class UWuwaEffectSpecPool;

/** Model 类 -> Spec 池。每个注册项持有独立的池，派生 Model 可以回退父类注册。 */
UCLASS(Transient)
class WUWA_API UWuwaEffectSpecFactory : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	bool Register(TSubclassOf<UWuwaEffectModelBase> ModelClass, TSubclassOf<UWuwaEffectSpec> SpecClass,
		TSubclassOf<UWuwaEffectSpecPool> PoolClass);
	UWuwaEffectSpecPool* FindPool(const UWuwaEffectModelBase* Model) const;
	int32 GetIdleCount() const;
	void ClearPools();
	void Close();

private:
	UPROPERTY(Transient)
	TMap<TSubclassOf<UWuwaEffectModelBase>, TObjectPtr<UWuwaEffectSpecPool>> Pools;
};
