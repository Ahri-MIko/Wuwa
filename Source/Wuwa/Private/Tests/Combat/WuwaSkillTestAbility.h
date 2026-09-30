#pragma once

#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "WuwaSkillTestAbility.generated.h"

/** Native test configuration adapter; all activation and ownership logic stays in the production base. */
UCLASS(NotBlueprintable, Transient)
class UWuwaSkillTestAbility : public UWuwaGameplayAbilityBase
{
	GENERATED_BODY()

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* EndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override
	{
#if WITH_DEV_AUTOMATION_TESTS
		// Tests adjust this native CDO between requests to represent different skill
		// configurations. Native default initialization does not necessarily copy those
		// runtime CDO edits into new instances, unlike authored Blueprint defaults.
		const UWuwaSkillTestAbility* Defaults = GetClass()->GetDefaultObject<UWuwaSkillTestAbility>();
		bIsMainSkill = Defaults->bIsMainSkill;
		InterruptLevel = Defaults->InterruptLevel;
		SkillOverrideType = Defaults->SkillOverrideType;
		bOverridesMoveState = Defaults->bOverridesMoveState;
		ActionMoveState = Defaults->ActionMoveState;
		ActionMoveStatePriority = Defaults->ActionMoveStatePriority;
#endif
		Super::PreActivate(Handle, ActorInfo, ActivationInfo, EndedDelegate, TriggerEventData);
	}
};
