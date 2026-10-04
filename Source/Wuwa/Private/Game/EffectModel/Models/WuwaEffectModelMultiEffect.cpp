#include "Game/EffectModel/Models/WuwaEffectModelMultiEffect.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

void UWuwaEffectModelMultiEffect::GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const
{
	OutModels.Add(EffectData.Get());
}

EDataValidationResult UWuwaEffectModelMultiEffect::IsDataValid(FDataValidationContext& Context) const
{
	if (Super::IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		// Base 已检查整个引用图；出现循环时不能继续进入 EffectData。
		return EDataValidationResult::Invalid;
	}
	bool bValid = true;
	if (Type != EWuwaMultiEffectType::BuffBall)
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "UnsupportedMultiEffectType", "MultiEffect currently supports only BuffBall."));
		bValid = false;
	}
	if (!FMath::IsFinite(BaseNum) || BaseNum < 0.f || !FMath::IsFinite(Radius) || Radius < 0.f
		|| !FMath::IsFinite(SpinSpeed))
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidMultiEffectParameters",
			"BaseNum and Radius must be finite and non-negative. SpinSpeed must be finite and may be negative."));
		bValid = false;
	}
	if (!IsValid(EffectData))
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "MissingMultiEffectData", "MultiEffect requires an EffectData model."));
		bValid = false;
	}
	else if (EffectData->IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		bValid = false;
	}
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
