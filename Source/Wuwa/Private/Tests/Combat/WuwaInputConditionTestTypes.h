#pragma once

#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "WuwaInputConditionTestTypes.generated.h"

/** Test instrumentation for the production reflected condition extension point. */
UCLASS(NotBlueprintable, Transient)
class UWuwaInputConditionTest : public UWuwaInputCondition
{
	GENERATED_BODY()

public:
	bool bResult = true;
	mutable int32 EvaluationCount = 0;
	TFunction<bool(const FWuwaInputCommandContext&)> CheckContext;

	virtual bool Evaluate_Implementation(const FWuwaInputCommandContext& Context) const override
	{
		++EvaluationCount;
		return bResult && (!CheckContext || CheckContext(Context));
	}
};
