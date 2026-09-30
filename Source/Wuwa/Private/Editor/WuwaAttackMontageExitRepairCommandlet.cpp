#include "Editor/WuwaAttackMontageExitRepairCommandlet.h"

#if WITH_EDITOR
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "HAL/FileManager.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAttackExitRepair, Log, All);

UWuwaAttackMontageExitRepairCommandlet::UWuwaAttackMontageExitRepairCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Preflights only GA_Attack1..5. -Apply connects empty montage terminal callbacks to self.EndAbility; never changes OnBlendOut or input rules.");
	HelpUsage = TEXT("-run=WuwaAttackMontageExitRepair [-Apply] [-RequireReadyEndForAttackMovement]");
}

#if WITH_EDITOR
namespace WuwaAttackExitRepair
{
	const FName ExitNames[] = { TEXT("OnCompleted"), TEXT("OnInterrupted"), TEXT("OnCancelled") };
	constexpr const TCHAR* MovementWindowClass = TEXT("/Script/UnrealSharp.AnimNotifyState_MovementCancelWindow_C");

	struct FPlan
	{
		UBlueprint* Blueprint = nullptr;
		UK2Node_BaseAsyncTask* Task = nullptr;
		bool bNeedsExitRepair = false;
		UAnimMontage* Montage = nullptr;
		UAnimNotifyState* MovementWindow = nullptr;
		FBoolProperty* RequireReadyEndProperty = nullptr;
		bool bNeedsMovementGuard = false;
	};

	bool IsDirectSelfEnd(UEdGraphPin* Output, const UFunction* EndFunction)
	{
		UEdGraphPin* Input = Output && Output->LinkedTo.Num() == 1 ? Output->LinkedTo[0] : nullptr;
		UK2Node_CallFunction* Call = Input ? Cast<UK2Node_CallFunction>(Input->GetOwningNode()) : nullptr;
		UEdGraphPin* Self = Call ? Call->FindPin(UEdGraphSchema_K2::PN_Self) : nullptr;
		return Input && Input->Direction == EGPD_Input && Input->PinName == UEdGraphSchema_K2::PN_Execute
			&& Input->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Call
			&& Call->GetGraph() == Output->GetOwningNode()->GetGraph() && Call->GetTargetFunction() == EndFunction
			&& Call->FunctionReference.IsSelfContext() && (!Self || (Self->LinkedTo.IsEmpty() && !Self->DefaultObject));
	}

	bool PreflightExits(UBlueprint* Blueprint, FPlan& Plan)
	{
		const UFunction* Factory = UAbilityTask_PlayMontageAndWait::StaticClass()->FindFunctionByName(TEXT("CreatePlayMontageAndWaitProxy"));
		const UFunction* EndFunction = UGameplayAbility::StaticClass()->FindFunctionByName(TEXT("K2_EndAbility"));
		if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(UWuwaGameplayAbilityBase::StaticClass())
			|| Blueprint->Status == BS_Error || !Factory || !EndFunction) return false;

		TArray<UK2Node_BaseAsyncTask*> AsyncNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AsyncNodes);
		TArray<UK2Node_BaseAsyncTask*> MontageTasks;
		for (UK2Node_BaseAsyncTask* Node : AsyncNodes)
			if (Node && Node->GetFactoryFunction() == Factory) MontageTasks.Add(Node);
		if (MontageTasks.Num() != 1)
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("%s must contain exactly one PlayMontageAndWait task (found %d)."),
				*Blueprint->GetPathName(), MontageTasks.Num());
			return false;
		}

		Plan.Blueprint = Blueprint;
		Plan.Task = MontageTasks[0];
		UEdGraphPin* Proxy = Plan.Task->FindPin(TEXT("AsyncTaskProxy"));
		if (Plan.Task->GetClass()->GetPathName() != TEXT("/Script/GameplayAbilitiesEditor.K2Node_LatentAbilityCall")
			|| !Plan.Task->GetGraph() || !Cast<UEdGraphSchema_K2>(Plan.Task->GetGraph()->GetSchema())
			|| !Proxy || Proxy->Direction != EGPD_Output
			|| Proxy->PinType.PinSubCategoryObject != UAbilityTask_PlayMontageAndWait::StaticClass()) return false;

		int32 Empty = 0;
		int32 Correct = 0;
		for (const FName Name : ExitNames)
		{
			UEdGraphPin* Pin = Plan.Task->FindPin(Name);
			if (!Pin || Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) return false;
			Empty += Pin->LinkedTo.IsEmpty();
			Correct += IsDirectSelfEnd(Pin, EndFunction);
		}
		// Never guess how a partial repair or a user-authored callback chain should be changed.
		if (Empty != 3 && Correct != 3)
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("%s has partial/custom terminal wiring. Preserving it; refusing the whole batch."), *Blueprint->GetPathName());
			return false;
		}
		Plan.bNeedsExitRepair = Empty == 3;
		return true;
	}

	bool PreflightMovementGuard(FPlan& Plan, int32 AttackIndex)
	{
		const FString Path = FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), AttackIndex, AttackIndex);
		Plan.Montage = LoadObject<UAnimMontage>(nullptr, *Path);
		UEdGraphPin* MontagePin = Plan.Task->FindPin(TEXT("MontageToPlay"));
		if (!Plan.Montage || !MontagePin || !MontagePin->LinkedTo.IsEmpty() || MontagePin->DefaultObject != Plan.Montage) return false;
		int32 Matches = 0;
		int32 ReadyEndMatches = 0;
		float ReadyEndTime = 0.f;
		float WindowEndTime = 0.f;
		for (const FAnimNotifyEvent& Event : Plan.Montage->Notifies)
		{
			if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass()))
			{
				++ReadyEndMatches;
				ReadyEndTime = Event.GetTriggerTime();
			}
			if (Event.NotifyStateClass && Event.NotifyStateClass->GetClass()->GetPathName() == MovementWindowClass)
			{
				++Matches;
				Plan.MovementWindow = Event.NotifyStateClass;
				WindowEndTime = Event.GetEndTriggerTime();
			}
		}
		if (Matches != 1 || ReadyEndMatches != 1 || ReadyEndTime >= WindowEndTime
			|| Plan.MovementWindow->GetTypedOuter<UAnimMontage>() != Plan.Montage)
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("%s must own one movement window and one ReadyEnd that occurs before that window closes."), *Path);
			return false;
		}
		Plan.RequireReadyEndProperty = FindFProperty<FBoolProperty>(Plan.MovementWindow->GetClass(), TEXT("RequireSkillReadyEnd"));
		if (!Plan.RequireReadyEndProperty)
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("RequireSkillReadyEnd is unavailable on %s. Publish the updated C# class before running this optional repair."), *Path);
			return false;
		}
		Plan.bNeedsMovementGuard = !Plan.RequireReadyEndProperty->GetPropertyValue_InContainer(Plan.MovementWindow);
		return true;
	}

	bool Backup(const TArray<UObject*>& Assets, const FString& Root)
	{
		for (const UObject* Asset : Assets)
		{
			const FString PackageName = Asset->GetOutermost()->GetName();
			FString RelativeName = PackageName;
			const FString MainFile = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			if (!RelativeName.RemoveFromStart(TEXT("/Game/")) || !IFileManager::Get().FileExists(*MainFile)) return false;
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk"), TEXT(".uptnl") })
			{
				const FString Source = FPackageName::LongPackageNameToFilename(PackageName, Extension);
				if (!IFileManager::Get().FileExists(*Source)) continue;
				const FString Destination = Root / (RelativeName + Extension);
				if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
					|| IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK)
				{
					UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("Backup failed for %s; no assets changed."), *Source);
					return false;
				}
			}
		}
		UE_LOG(LogWuwaAttackExitRepair, Display, TEXT("All original assets backed up to %s"), *Root);
		return true;
	}

	bool RepairExits(FPlan& Plan)
	{
		UEdGraph* Graph = Plan.Task->GetGraph();
		const UEdGraphSchema* Schema = Graph->GetSchema();
		Plan.Blueprint->Modify();
		Graph->Modify();
		Plan.Task->Modify();
		FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
		UK2Node_CallFunction* EndCall = Creator.CreateNode(false);
		EndCall->FunctionReference.SetSelfMember(TEXT("K2_EndAbility"));
		EndCall->NodePosX = Plan.Task->NodePosX + 480;
		EndCall->NodePosY = Plan.Task->NodePosY + 160;
		EndCall->NodeComment = TEXT("End this attack when its montage task completes, is interrupted, or is cancelled.");
		Creator.Finalize();
		UEdGraphPin* EndInput = EndCall->FindPin(UEdGraphSchema_K2::PN_Execute);
		if (!EndInput) return false;
		for (const FName Name : ExitNames)
		{
			UEdGraphPin* Output = Plan.Task->FindPin(Name);
			if (!Output || !Output->LinkedTo.IsEmpty()
				|| Schema->CanCreateConnection(Output, EndInput).Response != CONNECT_RESPONSE_MAKE
				|| !Schema->TryCreateConnection(Output, EndInput)) return false;
		}
		FBlueprintEditorUtils::MarkBlueprintAsModified(Plan.Blueprint);
		return true;
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
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("Save failed for %s. Earlier saves may have succeeded; use the logged backup directory to recover."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaAttackExitRepair, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaAttackMontageExitRepairCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaAttackExitRepair;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const bool bRequireReadyEnd = FParse::Param(*Params, TEXT("RequireReadyEndForAttackMovement"));
	TArray<FPlan> Plans;
	TArray<UObject*> AllAssets;
	bool bNeedsChanges = false;
	// Inspect every target before changing any graph, notify object, or package.
	for (int32 Index = 1; Index <= 5; ++Index)
	{
		const FString Path = FString::Printf(TEXT("/Game/Characters/Role/changli/GA/GA_Attack%d.GA_Attack%d"), Index, Index);
		FPlan Plan;
		if (!PreflightExits(LoadObject<UBlueprint>(nullptr, *Path), Plan)
			|| (bRequireReadyEnd && !PreflightMovementGuard(Plan, Index)))
		{
			UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("Preflight failed for %s. No assets changed or saved."), *Path);
			return 1;
		}
		UE_LOG(LogWuwaAttackExitRepair, Display, TEXT("%s: terminal exits=%s; movement ReadyEnd guard=%s"), *Path,
			Plan.bNeedsExitRepair ? TEXT("repair needed") : TEXT("already correct"),
			bRequireReadyEnd ? (Plan.bNeedsMovementGuard ? TEXT("enable") : TEXT("already enabled")) : TEXT("not requested"));
		bNeedsChanges |= Plan.bNeedsExitRepair || Plan.bNeedsMovementGuard;
		AllAssets.Add(Plan.Blueprint);
		if (bRequireReadyEnd) AllAssets.Add(Plan.Montage);
		Plans.Add(Plan);
	}
	if (!bApply || !bNeedsChanges)
	{
		UE_LOG(LogWuwaAttackExitRepair, Display, TEXT("%s No assets changed or saved."),
			bNeedsChanges ? TEXT("Read-only preflight passed; use this commandlet's -Apply to repair.") : TEXT("All requested repairs are already present."));
		return 0;
	}
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/AttackBuffer/Backup")
		/ (FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!Backup(AllAssets, BackupRoot)) return 1;

	TArray<UObject*> ChangedAssets;
	for (FPlan& Plan : Plans)
	{
		if (Plan.bNeedsExitRepair)
		{
			if (!RepairExits(Plan))
			{
				UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("Could not connect %s without replacing links. No assets saved."), *Plan.Blueprint->GetPathName());
				return 1;
			}
			FCompilerResultsLog Results;
			FKismetEditorUtilities::CompileBlueprint(Plan.Blueprint, EBlueprintCompileOptions::None, &Results);
			FPlan Verified;
			if (Results.NumErrors || Plan.Blueprint->Status == BS_Error || !PreflightExits(Plan.Blueprint, Verified) || Verified.bNeedsExitRepair)
			{
				UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("Compilation/verification failed for %s. No assets saved."), *Plan.Blueprint->GetPathName());
				return 1;
			}
			ChangedAssets.Add(Plan.Blueprint);
		}
		if (Plan.bNeedsMovementGuard)
		{
			// Only the opt-in boolean changes. Author-authored notify times, duration and other properties stay intact.
			Plan.Montage->Modify();
			Plan.MovementWindow->Modify();
			Plan.RequireReadyEndProperty->SetPropertyValue_InContainer(Plan.MovementWindow, true);
			ChangedAssets.Add(Plan.Montage);
		}
	}
	// A bad graph in the fifth asset cannot leave the first four saved with only part of the repair.
	for (UObject* Asset : ChangedAssets) if (!Save(Asset)) return 1;
	UE_LOG(LogWuwaAttackExitRepair, Display, TEXT("Attack repair complete: %d assets saved. OnBlendOut, notify timing, Dash and input rules were not changed. Backup: %s"),
		ChangedAssets.Num(), *BackupRoot);
	return 0;
#else
	UE_LOG(LogWuwaAttackExitRepair, Error, TEXT("WuwaAttackMontageExitRepair requires an editor build."));
	return 1;
#endif
}
