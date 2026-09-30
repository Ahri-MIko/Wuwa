#pragma once

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "WuwaSkillTestAbility.h"
#include "WuwaSkillNotifyTestTypes.generated.h"

/** Exposes the engine's real montage update/notify dispatch without ticking unrelated gameplay. */
UCLASS(NotBlueprintable, Transient)
class UWuwaSkillNotifyTestAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	void AdvanceNotifyTimeline(float DeltaSeconds)
	{
		ClearQueuedAnimEvents(false);
		Montage_UpdateWeight(DeltaSeconds);
		Montage_Advance(DeltaSeconds);
		DispatchQueuedAnimEvents();
	}
};

/** Replaces only the test GA's animation graph; production activation/ownership guards still run. */
UCLASS(NotBlueprintable, Transient)
class UWuwaSkillNotifyTestAbility : public UWuwaSkillTestAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> TestMontage;

	bool bPauseTestMontage = false;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override
	{
		Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
#if WITH_DEV_AUTOMATION_TESTS
		if (!IsSkillExecutionActive() || GetSkillHandle() <= 0 || !ActorInfo) return;
		const UWuwaSkillNotifyTestAbility* Defaults = GetClass()->GetDefaultObject<UWuwaSkillNotifyTestAbility>();
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (!ASC || !Defaults->TestMontage) return;
		ASC->PlayMontage(this, ActivationInfo, Defaults->TestMontage, 1.f);
		// Keep a replacement at time zero until the test explicitly advances it. This
		// avoids depending on whether Montage_Advance also visits an instance created
		// by a branching-point callback during that same engine update.
		if (Defaults->bPauseTestMontage && ActorInfo->GetAnimInstance())
			ActorInfo->GetAnimInstance()->Montage_Pause(Defaults->TestMontage);
#endif
	}
};
