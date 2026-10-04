#include "Game/EffectModel/Models/WuwaEffectModelGroup.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

void UWuwaEffectModelGroup::GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const
{
	for (const auto& Entry : EffectData)
	{
		OutModels.Add(Entry.Key.Get());
	}
}

EDataValidationResult UWuwaEffectModelGroup::IsDataValid(FDataValidationContext& Context) const
{
	if (Super::IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		// 包含间接循环时，不能再递归校验子资产。
		return EDataValidationResult::Invalid;
	}
	bool bValid = true;
	if (EffectData.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "EmptyGroup", "EffectData must contain at least one child model."));
		bValid = false;
	}
	for (const auto& [Child, Delay] : EffectData)
	{
		if (!IsValid(Child))
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "NullChild", "EffectData contains a missing child model."));
			bValid = false;
		}
		else if (Child->IsA<UWuwaEffectModelGroup>())
		{
			// 与原作 EffectSpec.Init 的限制一致，也同时阻止自引用。
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "NestedGroup", "A Group cannot contain another Group or itself."));
			bValid = false;
		}
		else if (Child->IsDataValid(Context) == EDataValidationResult::Invalid)
		{
			bValid = false;
		}
		if (!FMath::IsFinite(Delay) || Delay < 0.f)
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidDelay", "Child delay must be finite and non-negative."));
			bValid = false;
		}
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
