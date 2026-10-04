#include "Game/EffectModel/Models/WuwaEffectModelNiagara.h"
#include "NiagaraSystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UWuwaEffectModelNiagara::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	if (!IsValid(NiagaraRef))
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "MissingNiagara", "NiagaraRef must reference a Niagara System."));
		return EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
