#pragma once

#include "CoreMinimal.h"
#include "WuwaEffectSpecPool.generated.h"

class UWuwaEffectSpec;

/** 一个池服务一种 Spec 类；基类管理借还和容量，派生池管理具体资源的清理。 */
UCLASS(BlueprintType, Blueprintable, Transient)
class WUWA_API UWuwaEffectSpecPool : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	void Initialize(TSubclassOf<UWuwaEffectSpec> InSpecClass);
	UWuwaEffectSpec* Acquire();
	void Release(UWuwaEffectSpec* Spec);
	void Clear();
	/** 停用旧注册或退出世界：清空空闲项，仍在播放的对象归还时直接销毁资源。 */
	void Close();

	TSubclassOf<UWuwaEffectSpec> GetSpecClass() const { return SpecClass; }
	int32 GetIdleCount() const { return IdleSpecs.Num(); }

protected:
	/** 仅限制空闲缓存，不限制同时播放数量。0 表示归还时不缓存。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|Pool", meta = (ClampMin = "0"))
	int32 MaxIdleSpecs = 32;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Effect|Pool")
	TSubclassOf<UWuwaEffectSpec> SpecClass;

	UFUNCTION(BlueprintNativeEvent, Category = "Effect|Pool", meta = (BlueprintProtected))
	UWuwaEffectSpec* CreateSpec();
	virtual UWuwaEffectSpec* CreateSpec_Implementation();

	/** 每次归还调用。清除播放状态、请求引用和回调，保留可复用的资源。 */
	UFUNCTION(BlueprintNativeEvent, Category = "Effect|Pool", meta = (BlueprintProtected))
	void ResetSpec(UWuwaEffectSpec* Spec);
	virtual void ResetSpec_Implementation(UWuwaEffectSpec* Spec);

	/** 超出容量或清池时调用。释放组件等资源；Spec UObject 本身由 GC 回收。 */
	UFUNCTION(BlueprintNativeEvent, Category = "Effect|Pool", meta = (BlueprintProtected))
	void DestroySpec(UWuwaEffectSpec* Spec);
	virtual void DestroySpec_Implementation(UWuwaEffectSpec* Spec);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWuwaEffectSpec>> IdleSpecs;

	UPROPERTY(Transient)
	TSet<TObjectPtr<UWuwaEffectSpec>> InUseSpecs;

	bool bClosed = false;
};
