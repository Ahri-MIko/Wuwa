#include "Game/EffectModel/Base/WuwaEffectModelBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "Templates/Function.h"

void UWuwaEffectModelBase::GetChildModels(TArray<const UWuwaEffectModelBase*>& OutModels) const
{
}

EDataValidationResult UWuwaEffectModelBase::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	if (ParentResult == EDataValidationResult::Invalid)
	{
		return ParentResult;
	}
	if (!FMath::IsFinite(StartTime) || !FMath::IsFinite(LoopTime) || !FMath::IsFinite(EndTime)
		|| LoopTime < 0.f || EndTime < 0.f)
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidLifetime",
			"StartTime must be finite. LoopTime and EndTime must be finite and non-negative."));
		return EDataValidationResult::Invalid;
	}

	// 先检查整张引用图，子类才能安全地继续调用子资产的 IsDataValid。
	// ActivePath 才代表回到祖先；Finished 中的资产只是被多处复用，属于合法 DAG。
	TSet<const UWuwaEffectModelBase*> ActivePath;
	TSet<const UWuwaEffectModelBase*> Finished;
	TArray<const UWuwaEffectModelBase*> Path;
	TFunction<bool(const UWuwaEffectModelBase*)> Visit = [&](const UWuwaEffectModelBase* Model)
	{
		if (!IsValid(Model) || Finished.Contains(Model))
		{
			// 缺失引用由具体容器报告；此处只负责检测循环。
			return true;
		}
		if (ActivePath.Contains(Model))
		{
			FString CyclePath;
			for (const UWuwaEffectModelBase* Step : Path)
			{
				CyclePath += Step->GetPathName() + TEXT(" -> ");
			}
			CyclePath += Model->GetPathName();
			Context.AddError(FText::Format(NSLOCTEXT("WuwaEffectModel", "CyclicModelReference",
				"Effect model references contain a cycle: {0}"), FText::FromString(CyclePath)));
			return false;
		}

		ActivePath.Add(Model);
		Path.Add(Model);
		TArray<const UWuwaEffectModelBase*> Children;
		Model->GetChildModels(Children);
		bool bAcyclic = true;
		for (const UWuwaEffectModelBase* Child : Children)
		{
			if (!Visit(Child))
			{
				bAcyclic = false;
				break;
			}
		}
		Path.Pop();
		ActivePath.Remove(Model);
		if (bAcyclic)
		{
			Finished.Add(Model);
		}
		return bAcyclic;
	};
	return Visit(this) ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
