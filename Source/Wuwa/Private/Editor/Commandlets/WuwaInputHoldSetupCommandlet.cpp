#include "Editor/Commandlets/WuwaInputHoldSetupCommandlet.h"

#if WITH_EDITOR
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaInputHoldSetup, Log, All);

UWuwaInputHoldSetupCommandlet::UWuwaInputHoldSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Lists triggers on IMC_Character actions; -Apply adds a 0.5s repeating Hold trigger to Boolean actions without one.");
	HelpUsage = TEXT("-run=WuwaInputHoldSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaInputHoldSetup
{
	constexpr const TCHAR* ContextPath = TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character");
	constexpr float HoldSeconds = 0.5f;

	FString DescribeTriggers(const TArray<TObjectPtr<UInputTrigger>>& Triggers)
	{
		TArray<FString> Parts;
		for (const UInputTrigger* Trigger : Triggers)
		{
			if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger))
			{
				Parts.Add(FString::Printf(TEXT("Hold(%.2fs, oneShot=%d)"), Hold->HoldTimeThreshold, Hold->bIsOneShot ? 1 : 0));
			}
			else
			{
				Parts.Add(GetNameSafe(Trigger ? Trigger->GetClass() : nullptr));
			}
		}
		return Parts.IsEmpty() ? TEXT("none") : FString::Join(Parts, TEXT(", "));
	}

	bool HasHold(const TArray<TObjectPtr<UInputTrigger>>& Triggers)
	{
		return Triggers.ContainsByPredicate([](const UInputTrigger* Trigger) { return Trigger && Trigger->IsA<UInputTriggerHold>(); });
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
	}
}
#endif

int32 UWuwaInputHoldSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaInputHoldSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
	if (!Context)
	{
		UE_LOG(LogWuwaInputHoldSetup, Error, TEXT("Missing %s"), ContextPath);
		return 1;
	}

	// 映射层的触发器只列出来，不修改。
	TArray<UInputAction*> Actions;
	TSet<const UInputAction*> MappedHold;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		UE_LOG(LogWuwaInputHoldSetup, Display, TEXT("Mapping %s -> %s, mapping triggers: %s"),
			*Mapping.Key.ToString(), *GetNameSafe(Mapping.Action), *DescribeTriggers(Mapping.Triggers));
		if (!Mapping.Action) continue;
		Actions.AddUnique(const_cast<UInputAction*>(Mapping.Action.Get()));
		if (HasHold(Mapping.Triggers)) MappedHold.Add(Mapping.Action);
	}

	int32 Saved = 0;
	for (UInputAction* Action : Actions)
	{
		const bool bButton = Action->ValueType == EInputActionValueType::Boolean;
		UE_LOG(LogWuwaInputHoldSetup, Display, TEXT("Action %s (%s), action triggers: %s"),
			*Action->GetName(), bButton ? TEXT("button") : TEXT("axis"), *DescribeTriggers(Action->Triggers));
		// 映射上已经配了 Hold 的（比如左键攻击）不再在 IA 上重复添加。
		if (!bApply || !bButton || HasHold(Action->Triggers) || MappedHold.Contains(Action)) continue;

		Action->Modify();
		UInputTriggerHold* Hold = NewObject<UInputTriggerHold>(Action, NAME_None, RF_Public | RF_Transactional);
		Hold->HoldTimeThreshold = HoldSeconds;
		// 非一次性：一次性触发后会立刻回到 None，手还按着就产生 Completed（被当成松开）。
		Hold->bIsOneShot = false;
		Action->Triggers.Add(Hold);
		if (!Save(Action))
		{
			UE_LOG(LogWuwaInputHoldSetup, Error, TEXT("Could not save %s"), *Action->GetPathName());
			return 1;
		}
		++Saved;
		UE_LOG(LogWuwaInputHoldSetup, Display, TEXT("Added Hold(%.2fs) to %s"), HoldSeconds, *Action->GetName());
	}
	UE_LOG(LogWuwaInputHoldSetup, Display, TEXT("%s: %d action(s) saved."), bApply ? TEXT("Apply") : TEXT("Audit only"), Saved);
	return 0;
#else
	UE_LOG(LogWuwaInputHoldSetup, Error, TEXT("WuwaInputHoldSetup requires an editor build."));
	return 1;
#endif
}
