#include "Editor/Commandlets/WuwaCombatInputSetupCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCombatInputSetup, Log, All);

UWuwaCombatInputSetupCommandlet::UWuwaCombatInputSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits Changli attack input; -Apply creates the ordered attack rules and updates only their input/character bindings.");
	HelpUsage = TEXT("-run=WuwaCombatInputSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaCombatInputSetup
{
	constexpr const TCHAR* ConfigPackage = TEXT("/Game/Characters/Role/changli/Input/DA_Changli_InputCommands");
	constexpr const TCHAR* ConfigPath = TEXT("/Game/Characters/Role/changli/Input/DA_Changli_InputCommands.DA_Changli_InputCommands");
	constexpr const TCHAR* CharacterPath = TEXT("/Game/Characters/Role/changli/BP_WuwaCharacterBase.BP_WuwaCharacterBase");
	constexpr const TCHAR* ControllerPath = TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C");
	constexpr const TCHAR* BindingsPath = TEXT("/Game/CoreInput/DataAsset/DA_InputActionTagAsset.DA_InputActionTagAsset");
	constexpr const TCHAR* ActionPath = TEXT("/Game/CoreInput/Actions/IA_Attack.IA_Attack");
	constexpr const TCHAR* ContextPath = TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character");

	struct FSetup
	{
		UBlueprint* CharacterBlueprint = nullptr;
		AWuwaCharacter* Character = nullptr;
		const AWuwaPlayerController* Controller = nullptr;
		UWuwaInputDataAsset* Bindings = nullptr;
		UInputAction* Action = nullptr;
		const UInputMappingContext* Context = nullptr;
		UObject* ExistingConfigObject = nullptr;
		UWuwaInputCommandConfig* Config = nullptr;
		bool bConfigPackageExists = false;
		FGameplayTag InputTag;
		FGameplayTag RouteTag;
		TArray<FGameplayTag> AbilityTags;
		TArray<TSubclassOf<UGameplayAbility>> AbilityClasses;
		TArray<FWuwaInputCommandRule> ExpectedRules;
	};

	bool IsExpectedConfig(const FSetup& Setup)
	{
		if (!Setup.Config || Setup.Config->GetClass() != UWuwaInputCommandConfig::StaticClass()
			|| Setup.Config->Rules.Num() != Setup.ExpectedRules.Num()) return false;
		for (int32 Index = 0; Index < Setup.ExpectedRules.Num(); ++Index)
		{
			const FWuwaInputCommandRule& Actual = Setup.Config->Rules[Index];
			const FWuwaInputCommandRule& Expected = Setup.ExpectedRules[Index];
			if (Actual.RuleName != Expected.RuleName || Actual.InputTag != Expected.InputTag
				|| Actual.TargetAbilityTag != Expected.TargetAbilityTag
				|| Actual.RequiredCurrentSkillTag != Expected.RequiredCurrentSkillTag
				|| !Actual.OwnerTagQuery.IsEmpty() || !Actual.Conditions.IsEmpty()) return false;
		}
		return true;
	}

	FSetup LoadSetup()
	{
		FSetup Setup;
		Setup.CharacterBlueprint = LoadObject<UBlueprint>(nullptr, CharacterPath);
		Setup.Character = Setup.CharacterBlueprint && Setup.CharacterBlueprint->GeneratedClass
			? Cast<AWuwaCharacter>(Setup.CharacterBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr, ControllerPath);
		Setup.Controller = ControllerClass ? ControllerClass->GetDefaultObject<AWuwaPlayerController>() : nullptr;
		Setup.Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, BindingsPath);
		Setup.Action = LoadObject<UInputAction>(nullptr, ActionPath);
		Setup.Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
		Setup.bConfigPackageExists = FPackageName::DoesPackageExist(ConfigPackage);
		if (Setup.bConfigPackageExists)
		{
			Setup.ExistingConfigObject = LoadObject<UObject>(nullptr, ConfigPath, nullptr, LOAD_NoWarn);
			Setup.Config = Cast<UWuwaInputCommandConfig>(Setup.ExistingConfigObject);
		}
		Setup.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
		Setup.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			const FString ClassPath = FString::Printf(TEXT("/Game/Characters/Role/changli/GA/GA_Attack%d.GA_Attack%d_C"), Index, Index);
			Setup.AbilityClasses.Add(LoadClass<UWuwaGameplayAbilityBase>(nullptr, *ClassPath));
			Setup.AbilityTags.Add(FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Abilities.Skill.Attack%02d"), Index)), false));
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			FWuwaInputCommandRule& Rule = Setup.ExpectedRules.AddDefaulted_GetRef();
			Rule.RuleName = FName(*FString::Printf(TEXT("Attack%02dTo%02d"), Index + 1, Index + 2));
			Rule.InputTag = Setup.InputTag;
			Rule.RequiredCurrentSkillTag = Setup.AbilityTags[Index];
			Rule.TargetAbilityTag = Setup.AbilityTags[Index + 1];
		}
		FWuwaInputCommandRule& DefaultRule = Setup.ExpectedRules.AddDefaulted_GetRef();
		DefaultRule.RuleName = TEXT("AttackDefault01");
		DefaultRule.InputTag = Setup.InputTag;
		DefaultRule.TargetAbilityTag = Setup.AbilityTags[0];
		return Setup;
	}

	bool WriteAudit(const FSetup& Setup, const TCHAR* Filename)
	{
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetStringField(TEXT("auditUtc"), FDateTime::UtcNow().ToIso8601());
		Report->SetStringField(TEXT("scope"), TEXT("Static asset configuration only; no ability activation or animation playback verified."));
		Report->SetStringField(TEXT("expectedConfig"), ConfigPath);
		Report->SetBoolField(TEXT("configPackageExists"), Setup.bConfigPackageExists);
		Report->SetStringField(TEXT("configClass"), Setup.ExistingConfigObject ? Setup.ExistingConfigObject->GetClass()->GetPathName() : TEXT(""));
		Report->SetBoolField(TEXT("configMatchesSetupRules"), IsExpectedConfig(Setup));
		Report->SetStringField(TEXT("characterConfig"), Setup.Character ? GetPathNameSafe(Setup.Character->InputCommandConfig.Get()) : TEXT(""));
		Report->SetBoolField(TEXT("characterUsesExpectedConfig"), Setup.Character && Setup.Config && Setup.Character->InputCommandConfig == Setup.Config);
		Report->SetBoolField(TEXT("controllerUsesInspectedBindings"), Setup.Controller && Setup.Bindings && Setup.Controller->InputTagMap == Setup.Bindings);
		TArray<TSharedPtr<FJsonValue>> Rules;
		if (Setup.Config) for (const FWuwaInputCommandRule& Rule : Setup.Config->Rules)
		{
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("ruleName"), Rule.RuleName.ToString());
			Row->SetStringField(TEXT("inputTag"), Rule.InputTag.ToString());
			Row->SetStringField(TEXT("requiredCurrentSkillTag"), Rule.RequiredCurrentSkillTag.ToString());
			Row->SetStringField(TEXT("targetAbilityTag"), Rule.TargetAbilityTag.ToString());
			Row->SetBoolField(TEXT("hasOwnerTagQuery"), !Rule.OwnerTagQuery.IsEmpty());
			Row->SetNumberField(TEXT("conditionCount"), Rule.Conditions.Num());
			Rules.Add(MakeShared<FJsonValueObject>(Row));
		}
		Report->SetArrayField(TEXT("rules"), Rules);
		TArray<TSharedPtr<FJsonValue>> BindingRows;
		if (Setup.Bindings) for (const FInputDataAsset& Binding : Setup.Bindings->InputDataAssetMap)
		{
			if (Binding.InputAction != Setup.Action && Binding.InputTag != Setup.InputTag) continue;
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("action"), GetPathNameSafe(Binding.InputAction));
			Row->SetStringField(TEXT("inputTag"), Binding.InputTag.ToString());
			Row->SetStringField(TEXT("routeTag"), Binding.RouteTag.ToString());
			BindingRows.Add(MakeShared<FJsonValueObject>(Row));
		}
		Report->SetArrayField(TEXT("attackBindings"), BindingRows);
		TArray<TSharedPtr<FJsonValue>> Keys;
		if (Setup.Context) for (const FEnhancedActionKeyMapping& Mapping : Setup.Context->GetMappings())
			if (Mapping.Action == Setup.Action) Keys.Add(MakeShared<FJsonValueString>(Mapping.Key.ToString()));
		Report->SetArrayField(TEXT("attackPhysicalKeys"), Keys);
		TArray<TSharedPtr<FJsonValue>> Abilities;
		for (int32 Index = 0; Index < Setup.AbilityClasses.Num(); ++Index)
		{
			UClass* Class = Setup.AbilityClasses[Index].Get();
			const UWuwaGameplayAbilityBase* Ability = Class ? Cast<UWuwaGameplayAbilityBase>(Class->GetDefaultObject()) : nullptr;
			int32 Count = 0;
			if (Setup.Character) for (const auto& GrantedClass : Setup.Character->CharacterAbilities) Count += Class && GrantedClass.Get() == Class;
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("class"), GetPathNameSafe(Class));
			Row->SetStringField(TEXT("expectedTag"), Setup.AbilityTags[Index].ToString());
			Row->SetStringField(TEXT("originalTag"), Ability ? Ability->OriginalTag.ToString() : TEXT(""));
			Row->SetNumberField(TEXT("characterAbilityEntries"), Count);
			Row->SetBoolField(TEXT("isMainSkill"), Ability && Ability->bIsMainSkill);
			Row->SetNumberField(TEXT("instancingPolicy"), Ability ? static_cast<int32>(Ability->GetInstancingPolicy()) : -1);
			Abilities.Add(MakeShared<FJsonValueObject>(Row));
			UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Attack%02d: class=%s, tag=%s, character entries=%d"), Index + 1,
				*GetPathNameSafe(Class), Ability ? *Ability->OriginalTag.ToString() : TEXT("missing"), Count);
		}
		Report->SetArrayField(TEXT("attackAbilities"), Abilities);
		FString Json;
		const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/CombatInputCommands"));
		const FString Path = Directory / Filename;
		const bool bWritten = IFileManager::Get().MakeDirectory(*Directory, true)
			&& FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *Path);
		UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Audit %s: config=%s, character config=%s, rule match=%d"), *Path,
			*GetPathNameSafe(Setup.Config), Setup.Character ? *GetPathNameSafe(Setup.Character->InputCommandConfig.Get()) : TEXT("missing"), IsExpectedConfig(Setup));
		return bWritten;
	}

	bool ValidateForApply(const FSetup& Setup)
	{
		if (!Setup.Character || !Setup.Bindings || !Setup.Action || !Setup.Context || !Setup.Controller
			|| Setup.Controller->InputTagMap != Setup.Bindings || !Setup.InputTag.IsValid() || !Setup.RouteTag.IsValid())
		{
			UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Required assets/native tags or the controller input binding are missing or inconsistent. No assets saved."));
			return false;
		}
		if ((Setup.bConfigPackageExists && !IsExpectedConfig(Setup))
			|| (Setup.Character->InputCommandConfig && Setup.Character->InputCommandConfig != Setup.Config))
		{
			UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("An existing config has different type/rules, or the character uses another config. Refusing to overwrite user configuration. No assets saved."));
			return false;
		}
		for (int32 Index = 0; Index < Setup.AbilityClasses.Num(); ++Index)
		{
			UClass* Class = Setup.AbilityClasses[Index].Get();
			const UWuwaGameplayAbilityBase* Ability = Class ? Cast<UWuwaGameplayAbilityBase>(Class->GetDefaultObject()) : nullptr;
			if (!Ability || !Ability->bIsMainSkill || !Setup.AbilityTags[Index].IsValid() || Ability->OriginalTag != Setup.AbilityTags[Index])
			{
				UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Attack%02d must already inherit WuwaGameplayAbilityBase, be a main skill, and use %s. The commandlet never rewrites GA defaults."),
					Index + 1, *Setup.AbilityTags[Index].ToString());
				return false;
			}
		}
		for (const auto& Class : Setup.Character->CharacterAbilities)
		{
			if (!Class || Setup.AbilityClasses.Contains(Class)) continue;
			const UWuwaGameplayAbilityBase* Ability = Cast<UWuwaGameplayAbilityBase>(Class->GetDefaultObject());
			if (Ability && Setup.AbilityTags.Contains(Ability->OriginalTag))
			{
				UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Another granted GA %s uses an attack identity tag. Refusing an ambiguous mapping; no assets saved."), *Class->GetPathName());
				return false;
			}
		}
		for (const FInputDataAsset& Row : Setup.Bindings->InputDataAssetMap)
		{
			if (Row.InputTag == Setup.InputTag && Row.InputAction != Setup.Action)
			{
				UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Input.Combat.Attack is already bound to a different action. That row is preserved; no assets saved."));
				return false;
			}
		}
		return true;
	}

	bool Backup(const TArray<UObject*>& Assets, const FString& Root)
	{
		for (const UObject* Asset : Assets)
		{
			if (!Asset) continue;
			const FString PackageName = Asset->GetOutermost()->GetName();
			FString RelativeName = PackageName;
			if (!RelativeName.RemoveFromStart(TEXT("/Game/"))) return false;
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk"), TEXT(".uptnl") })
			{
				const FString Source = FPackageName::LongPackageNameToFilename(PackageName, Extension);
				if (!IFileManager::Get().FileExists(*Source)) continue;
				const FString Destination = Root / (RelativeName + Extension);
				if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
					|| IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK)
				{
					UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Could not back up %s; no assets saved."), *Source);
					return false;
				}
			}
		}
		UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Original assets backed up to %s"), *Root);
		return true;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)) return false;
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Could not save %s. Earlier saves may have succeeded; original assets remain in the backup directory."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaCombatInputSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCombatInputSetup;
	FSetup Setup = LoadSetup();
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	if (!WriteAudit(Setup, bApply ? TEXT("BeforeApply.json") : TEXT("SetupAudit.json"))) return 1;
	if (!bApply)
	{
		UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Read-only audit complete. No assets saved. Use -Apply to configure the listed attack input/rules and character defaults."));
		return 0;
	}
	if (!ValidateForApply(Setup)) return 1;
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/CombatInputSetup")
		/ (FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!Backup({ Setup.Config, Setup.Bindings, Setup.CharacterBlueprint }, BackupRoot)) return 1;

	TArray<UObject*> ChangedAssets;
	if (!Setup.Config)
	{
		Setup.Config = NewObject<UWuwaInputCommandConfig>(CreatePackage(ConfigPackage),
			*FPackageName::GetShortName(ConfigPackage), RF_Public | RF_Standalone);
		Setup.Config->Rules = Setup.ExpectedRules;
		ChangedAssets.Add(Setup.Config);
	}
	// Keep the first attack row's position, all other rows and all physical key mappings.
	TArray<FInputDataAsset> NewBindings;
	bool bAddedAttack = false;
	int32 AttackRows = 0;
	bool bCorrectAttackRow = false;
	for (const FInputDataAsset& Row : Setup.Bindings->InputDataAssetMap)
	{
		if (Row.InputAction != Setup.Action) { NewBindings.Add(Row); continue; }
		++AttackRows;
		bCorrectAttackRow = Row.InputTag == Setup.InputTag && Row.RouteTag == Setup.RouteTag;
		if (bAddedAttack) continue;
		FInputDataAsset& Attack = NewBindings.AddDefaulted_GetRef();
		Attack.InputAction = Setup.Action;
		Attack.InputTag = Setup.InputTag;
		Attack.RouteTag = Setup.RouteTag;
		bAddedAttack = true;
	}
	if (!bAddedAttack)
	{
		FInputDataAsset& Attack = NewBindings.AddDefaulted_GetRef();
		Attack.InputAction = Setup.Action;
		Attack.InputTag = Setup.InputTag;
		Attack.RouteTag = Setup.RouteTag;
	}
	if (AttackRows != 1 || !bCorrectAttackRow)
	{
		Setup.Bindings->Modify();
		Setup.Bindings->InputDataAssetMap = MoveTemp(NewBindings);
		ChangedAssets.Add(Setup.Bindings);
	}
	TArray<TSubclassOf<UGameplayAbility>> NewAbilities;
	for (const auto& Class : Setup.Character->CharacterAbilities)
	{
		if (!Setup.AbilityClasses.Contains(Class) || !NewAbilities.Contains(Class)) NewAbilities.Add(Class);
	}
	for (const auto& Class : Setup.AbilityClasses) NewAbilities.AddUnique(Class);
	if (Setup.Character->InputCommandConfig != Setup.Config || Setup.Character->CharacterAbilities != NewAbilities)
	{
		Setup.CharacterBlueprint->Modify();
		Setup.Character->Modify();
		Setup.Character->InputCommandConfig = Setup.Config;
		Setup.Character->CharacterAbilities = NewAbilities;
		FBlueprintEditorUtils::MarkBlueprintAsModified(Setup.CharacterBlueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Setup.CharacterBlueprint, EBlueprintCompileOptions::None, &Results);
		Setup.Character = Setup.CharacterBlueprint->GeneratedClass
			? Cast<AWuwaCharacter>(Setup.CharacterBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		if (Results.NumErrors || Setup.CharacterBlueprint->Status == BS_Error || !Setup.Character
			|| Setup.Character->InputCommandConfig != Setup.Config || Setup.Character->CharacterAbilities != NewAbilities)
		{
			UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("Character Blueprint compilation failed or lost the requested defaults. No assets saved."));
			return 1;
		}
		ChangedAssets.Add(Setup.CharacterBlueprint);
	}
	for (UObject* Asset : ChangedAssets) if (!Save(Asset)) return 1;
	Setup.ExistingConfigObject = Setup.Config;
	Setup.bConfigPackageExists = true;
	if (!WriteAudit(Setup, TEXT("AfterApply.json"))) return 1;
	UE_LOG(LogWuwaCombatInputSetup, Display, TEXT("Combat input setup complete: %d assets saved. GA assets, montage windows, IMC and other bindings were not edited."), ChangedAssets.Num());
	return 0;
#else
	UE_LOG(LogWuwaCombatInputSetup, Error, TEXT("WuwaCombatInputSetup requires an editor build."));
	return 1;
#endif
}
