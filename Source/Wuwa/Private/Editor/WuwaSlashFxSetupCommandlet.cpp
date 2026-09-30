#include "Editor/WuwaSlashFxSetupCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EdGraphSchema_Niagara.h"
#include "Engine/StaticMesh.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SlashFx.h"
#include "Game/Render/Effect/WuwaSlashFxPreset.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MeshDescription.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSlashFxSetup, Log, All);

UWuwaSlashFxSetupCommandlet::UWuwaSlashFxSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Builds original procedural slash Niagara art and installs only generated FX notifies on Changli Attack01..05. Existing skill windows are preserved.");
	HelpUsage = TEXT("-run=WuwaSlashFxSetup [-Apply] [-Reference=path/to/KuroAttackFxReference.json]");
}

#if WITH_EDITOR
namespace WuwaSlashFxSetup
{
	constexpr const TCHAR* AssetRoot = TEXT("/Game/Effects/ChangliSlash/");
	constexpr const TCHAR* OwnerKey = TEXT("WuwaSlashFxGenerator");
	constexpr const TCHAR* OwnerValue = TEXT("OriginalChangliSlashV1");
	constexpr const TCHAR* NotifyPrefix = TEXT("WuwaSlashFx.Auto.");
	constexpr const TCHAR* SlashTemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst");
	constexpr const TCHAR* EmberTemplatePath = TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst");

	struct FBurst
	{
		int32 Attack = 0;
		int32 Index = 0;
		float Time = 0;
		FVector Location = FVector(0, 0, 90);
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Scale = FVector::OneVector;
		FString PresetPath;
		UWuwaSlashFxPreset* Preset = nullptr;
	};
	struct FAttack
	{
		UAnimMontage* Montage = nullptr;
		TArray<FBurst> Bursts;
	};
	struct FAssetPlan
	{
		FString Path;
		UClass* Class = nullptr;
		UObject* Existing = nullptr;
	};

	FString ObjectPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
	}

	// This is our mesh's coordinate system, not a claim about the unavailable original effect meshes.
	void ArtPlacement(FBurst& B)
	{
		switch (B.Attack)
		{
		case 1:
			B.Location = FVector(0, 0, 90); B.Rotation = FRotator(-25, 0, -8); B.Scale = FVector(1.15); break;
		case 2:
			B.Location = FVector(5, -18, 119); B.Rotation = FRotator(29, 20, 10); B.Scale = FVector(1.0, 1.14, 1.0); break;
		case 3:
			if (B.Index == 1) { B.Location = FVector(0, 0, 66); B.Rotation = FRotator(6, -70, 18); B.Scale = FVector(.68); }
			if (B.Index == 2) { B.Location = FVector(0, 0, 83); B.Rotation = FRotator(-12, 2, -10); B.Scale = FVector(.76); }
			if (B.Index == 3) { B.Location = FVector(0, 0, 103); B.Rotation = FRotator(7, 105, 4); B.Scale = FVector(1.08); }
			if (B.Index == 4) { B.Location = FVector(0, 0, 105); B.Rotation = FRotator(-6, -150, -12); B.Scale = FVector(.88); }
			break;
		case 4:
			B.Location = FVector(0, 0, 123); B.Rotation = FRotator(-27, -18, 24); B.Scale = FVector(1.20); break;
		case 5:
			B.Location = FVector(0, 0, B.Index == 1 ? 90 : 104);
			B.Rotation = FRotator(B.Index == 1 ? -7 : 11, B.Index == 2 ? -30 : B.Index == 4 ? 35 : 0, B.Index == 1 ? 5 : 72);
			B.Scale = FVector(B.Index == 1 ? 1.18 : .88); break;
		}
	}

	bool ReadAttackPlans(const FString& ReferenceFile, TArray<FAttack>& Plans)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *ReferenceFile)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) return false;
		const TArray<TSharedPtr<FJsonValue>>* Attacks = nullptr;
		if (!Root->TryGetArrayField(TEXT("attacks"), Attacks) || Attacks->Num() != 5) return false;
		const int32 ExpectedCounts[] = { 1, 1, 4, 1, 4 };
		for (int32 A = 0; A < 5; ++A)
		{
			const TSharedPtr<FJsonObject> Ref = (*Attacks)[A]->AsObject();
			const FString AttackName = FString::Printf(TEXT("Attack%02d"), A + 1);
			const FString MontagePath = FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_%s.AM_%s"), *AttackName, *AttackName);
			FString RecordedName, RecordedMontage;
			double RecordedLength = 0;
			const TArray<TSharedPtr<FJsonValue>>* Bursts = nullptr;
			if (!Ref || !Ref->TryGetStringField(TEXT("projectAttack"), RecordedName) || RecordedName != AttackName
				|| !Ref->TryGetStringField(TEXT("projectMontage"), RecordedMontage) || RecordedMontage != MontagePath
				|| !Ref->TryGetNumberField(TEXT("projectLengthSeconds"), RecordedLength)
				|| !Ref->TryGetArrayField(TEXT("proposedProjectBursts"), Bursts) || Bursts->Num() != ExpectedCounts[A]) return false;
			FAttack Plan;
			Plan.Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
			if (!Plan.Montage || !FMath::IsNearlyEqual(Plan.Montage->GetPlayLength(), float(RecordedLength), .001f))
			{
				UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Montage missing or timeline changed since reference audit: %s"), *MontagePath);
				return false;
			}
			float PreviousTime = -1;
			for (int32 I = 0; I < Bursts->Num(); ++I)
			{
				double Time;
				TSharedPtr<FJsonObject> BurstRef = (*Bursts)[I]->AsObject();
				if (!BurstRef || !BurstRef->TryGetNumberField(TEXT("projectNotifySeconds"), Time)
					|| !FMath::IsFinite(Time) || Time <= PreviousTime || Time >= RecordedLength || Time < 0) return false;
				FBurst B;
				B.Attack = A + 1; B.Index = I + 1; B.Time = float(Time);
				B.PresetPath = FString::Printf(TEXT("%sPresets/DA_Attack%02d_%02d"), AssetRoot, B.Attack, B.Index);
				ArtPlacement(B);
				Plan.Bursts.Add(B);
				PreviousTime = B.Time;
			}
			// Refuse to replace a name collision or a hand-authored notify that only happens to use our prefix.
			for (const FAnimNotifyEvent& E : Plan.Montage->Notifies)
			{
				if (!E.NotifyName.ToString().StartsWith(NotifyPrefix)) continue;
				const UWuwaAnimNotify_SlashFx* N = Cast<UWuwaAnimNotify_SlashFx>(E.Notify);
				if (!N || !N->Preset || !N->Preset->GetOutermost()->GetName().StartsWith(AssetRoot)) return false;
			}
			Plans.Add(MoveTemp(Plan));
		}
		return true;
	}

	bool PreflightAsset(FAssetPlan& Plan)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Plan.Path, FPackageName::GetAssetPackageExtension());
		Plan.Existing = LoadObject<UObject>(nullptr, *ObjectPath(Plan.Path));
		if (!Plan.Existing) return !IFileManager::Get().FileExists(*Filename);
		if (Plan.Existing->GetClass() != Plan.Class || Plan.Existing->GetOutermost()->GetMetaData().GetValue(Plan.Existing, OwnerKey) != OwnerValue)
		{
			UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Refusing to replace an asset not owned by this generator: %s"), *Plan.Path);
			return false;
		}
		return true;
	}

	bool Backup(const TArray<UObject*>& Assets, const FString& Root)
	{
		for (const UObject* Asset : Assets)
		{
			const FString Package = Asset->GetOutermost()->GetName();
			FString Relative = Package;
			if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk"), TEXT(".uptnl") })
			{
				const FString Source = FPackageName::LongPackageNameToFilename(Package, Extension);
				if (!IFileManager::Get().FileExists(*Source))
				{
					if (FCString::Strcmp(Extension, TEXT(".uasset")) == 0) return false;
					continue;
				}
				const FString Target = Root / (Relative + Extension);
				if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true)
					|| IFileManager::Get().Copy(*Target, *Source, false, true) != COPY_OK) return false;
			}
		}
		UE_LOG(LogWuwaSlashFxSetup, Display, TEXT("Original assets backed up: %s"), *Root);
		return true;
	}

	template<class T> T* GetAsset(const FString& Relative, TArray<UObject*>& Assets)
	{
		const FString PackagePath = FString(AssetRoot) + Relative;
		T* Result = LoadObject<T>(nullptr, *ObjectPath(PackagePath));
		if (!Result)
		{
			UPackage* Package = CreatePackage(*PackagePath);
			Result = NewObject<T>(Package, *FPackageName::GetLongPackageAssetName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Result);
		}
		Result->Modify();
		Result->GetOutermost()->GetMetaData().SetValue(Result, OwnerKey, OwnerValue);
		Assets.AddUnique(Result);
		return Result;
	}

	template<class T> T* AddExpression(UMaterial* Material, int32 X, int32 Y)
	{
		T* E = NewObject<T>(Material, NAME_None, RF_Transactional);
		E->MaterialExpressionEditorX = X;
		E->MaterialExpressionEditorY = Y;
		Material->GetExpressionCollection().AddExpression(E);
		return E;
	}

	UMaterial* BuildMaterial(bool bEmber, TArray<UObject*>& Assets)
	{
		UMaterial* M = GetAsset<UMaterial>(bEmber ? TEXT("Materials/M_ChangliEmber") : TEXT("Materials/M_ChangliSlash"), Assets);
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->BlendMode = BLEND_Additive;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		M->bUsedWithNiagaraSprites = bEmber;
		M->bUsedWithNiagaraMeshParticles = !bEmber;
		auto* UV = AddExpression<UMaterialExpressionTextureCoordinate>(M, -800, -100);
		auto* Age = AddExpression<UMaterialExpressionParticleRelativeTime>(M, -800, 100);
		auto* Art = AddExpression<UMaterialExpressionCustom>(M, -520, 0);
		Art->Description = bEmber ? TEXT("Original gold ember / normalized particle lifetime") : TEXT("Original red-gold blade: reveal, flowing filaments, and erosion");
		Art->OutputType = CMOT_Float4;
		Art->Inputs.Empty();
		FCustomInput UVInput; UVInput.InputName = TEXT("UV"); UVInput.Input.Connect(0, UV); Art->Inputs.Add(UVInput);
		FCustomInput AgeInput; AgeInput.InputName = TEXT("Age"); AgeInput.Input.Connect(0, Age); Art->Inputs.Add(AgeInput);
		auto ColorParameter = [&](const TCHAR* Name, FLinearColor Default, int32 Y)
		{
			auto* P = AddExpression<UMaterialExpressionVectorParameter>(M, -1100, Y);
			P->ParameterName = Name; P->DefaultValue = Default; P->Group = TEXT("Slash Art");
			FCustomInput Input; Input.InputName = Name; Input.Input.Connect(0, P); Art->Inputs.Add(Input);
		};
		auto ScalarParameter = [&](const TCHAR* Name, float Default, int32 Y)
		{
			auto* P = AddExpression<UMaterialExpressionScalarParameter>(M, -1100, Y);
			P->ParameterName = Name; P->DefaultValue = Default; P->Group = TEXT("Slash Art");
			P->SliderMin = 0.f; P->SliderMax = 3.f;
			FCustomInput Input; Input.InputName = Name; Input.Input.Connect(0, P); Art->Inputs.Add(Input);
		};
		ColorParameter(TEXT("CoreColor"), FLinearColor(2.9f, .09f, .006f), 300);
		ColorParameter(TEXT("HotColor"), FLinearColor(4.3f, .75f, .045f), 480);
		ColorParameter(TEXT("GoldColor"), FLinearColor(7.f, 4.5f, 1.7f), 660);
		ScalarParameter(TEXT("Intensity"), 1.f, 840);
		ScalarParameter(TEXT("NoiseAmount"), 1.f, 980);
		Art->Code = bEmber ? TEXT(R"HLSL(
float2 p = (UV - 0.5) * float2(2.0, 0.75);
float d = length(p) * 2.0;
float core = pow(saturate(1.0 - d), 2.0);
float glow = exp(-d * d * 7.0) * 0.25;
float fade = (1.0 - smoothstep(0.1, 1.0, Age));
float3 warm = lerp(CoreColor, GoldColor * 0.75, core);
return float4(warm * Intensity, saturate((core + glow) * fade));
)HLSL") : TEXT(R"HLSL(
float u = saturate(UV.x);
float v = saturate(UV.y);
float age = saturate(Age);
float flow = u * 39.0 - age * 12.0;
float noise = lerp(0.65, sin(flow + sin(v * 28.0 + u * 13.0) * 1.4) * 0.5 + 0.5, saturate(NoiseAmount));
float fine = sin(u * 143.0 - age * 25.0 + v * 11.0) * 0.5 + 0.5;
float reveal = 1.0 - smoothstep(age * 5.5 - 0.10, age * 5.5 + 0.035, u);
float ends = smoothstep(0.0, 0.075, u) * (1.0 - smoothstep(0.93, 1.0, u));
float envelope = smoothstep(0.0, 0.045, age) * (1.0 - smoothstep(0.28, 1.0, age));
float inner = smoothstep(0.08 + noise * 0.24, 0.31 + noise * 0.22, v);
float outer = 1.0 - smoothstep(0.975, 1.0, v);
float body = pow(saturate(sin(v * 3.14159265)), 0.75) * inner * outer;
float cuttingEdge = exp(-pow((v - 0.935) * 46.0, 2.0));
float thread1 = exp(-pow((v - (0.73 + sin(u * 24.0 - age * 8.0) * 0.022)) * 85.0, 2.0));
float thread2 = exp(-pow((v - (0.51 + sin(u * 33.0 - age * 10.0) * 0.027)) * 100.0, 2.0));
float erosion = smoothstep(age * 0.85 - 0.16, age * 0.85 + 0.2, noise * 0.72 + fine * 0.28 + v * 0.28);
float opacity = saturate(body * (0.24 + noise * 0.48) + cuttingEdge + thread1 * 0.7 + thread2 * 0.35);
opacity *= reveal * ends * envelope * erosion;
float3 red = CoreColor;
float3 orange = HotColor;
float3 gold = GoldColor;
float3 color = lerp(red, orange, saturate(v * v + noise * 0.13));
color = lerp(color, gold, saturate(cuttingEdge + thread1 * 0.72 + thread2 * 0.55));
return float4(color * Intensity, opacity);
)HLSL");
		auto* RGB = AddExpression<UMaterialExpressionComponentMask>(M, -170, -80);
		RGB->Input.Connect(0, Art); RGB->R = RGB->G = RGB->B = true; RGB->A = false;
		auto* Alpha = AddExpression<UMaterialExpressionComponentMask>(M, -170, 100);
		Alpha->Input.Connect(0, Art); Alpha->R = Alpha->G = Alpha->B = false; Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
		M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		M->PostEditChange();
		return M;
	}

	UStaticMesh* BuildArc(bool bFilament, UMaterial* Material, TArray<UObject*>& Assets)
	{
		UStaticMesh* Mesh = GetAsset<UStaticMesh>(bFilament ? TEXT("Mesh/SM_ChangliFilament") : TEXT("Mesh/SM_ChangliSlash"), Assets);
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		auto Positions = Attributes.GetVertexPositions();
		auto Normals = Attributes.GetVertexInstanceNormals();
		auto Tangents = Attributes.GetVertexInstanceTangents();
		auto Signs = Attributes.GetVertexInstanceBinormalSigns();
		auto Colors = Attributes.GetVertexInstanceColors();
		auto UVs = Attributes.GetVertexInstanceUVs(); UVs.SetNumChannels(1);
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Slash");
		constexpr int32 Steps = 96;
		const float Radius = bFilament ? 149.f : 142.f;
		const float Width = bFilament ? 3.3f : 32.f;
		TArray<FVertexInstanceID> Inner, Outer;
		for (int32 I = 0; I <= Steps; ++I)
		{
			const float U = float(I) / Steps;
			const float Angle = FMath::DegreesToRadians(-104.f + U * 208.f);
			const float Taper = FMath::Pow(FMath::Max(.002f, FMath::Sin(PI * U)), .70f);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float R = Radius - (Side == 0 ? Width * Taper : 0.f);
				const FVertexID Vertex = Description.CreateVertex();
				Positions[Vertex] = FVector3f(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, FMath::Sin(PI * U) * (bFilament ? 1.8f : 0.f));
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				UVs.Set(Instance, 0, FVector2f(U, float(Side)));
				Normals[Instance] = FVector3f(0, 0, 1);
				Tangents[Instance] = FVector3f(-FMath::Sin(Angle), FMath::Cos(Angle), 0);
				Signs[Instance] = -1.f;
				Colors[Instance] = FVector4f(1, 1, 1, 1);
				(Side == 0 ? Inner : Outer).Add(Instance);
			}
		}
		for (int32 I = 0; I < Steps; ++I)
		{
			const FVertexInstanceID A[] = { Inner[I], Outer[I], Outer[I + 1] };
			const FVertexInstanceID B[] = { Inner[I], Outer[I + 1], Inner[I + 1] };
			Description.CreateTriangle(Group, MakeArrayView(A));
			Description.CreateTriangle(Group, MakeArrayView(B));
		}
		Mesh->GetStaticMaterials().Empty();
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Slash"), TEXT("Slash")));
		UStaticMesh::FBuildMeshDescriptionsParams Options;
		Options.bBuildSimpleCollision = false;
		Options.bUseHashAsGuid = true;
		Options.bAllowCpuAccess = true;
		if (!Mesh->BuildFromMeshDescriptions({ &Description }, Options)) return nullptr;
		return Mesh;
	}

	template<class T> bool SetRapid(FVersionedNiagaraEmitterData* Data, const TCHAR* Suffix, const T& Value, bool bRequired = true)
	{
		TArray<UNiagaraScript*> Scripts;
		Data->GetScripts(Scripts, false);
		int32 Matches = 0;
		for (UNiagaraScript* Script : Scripts)
		{
			if (!Script) continue;
			TArray<FNiagaraVariable> Variables;
			Script->RapidIterationParameters.GetParameters(Variables);
			for (const FNiagaraVariable& Var : Variables)
			{
				if (!Var.GetName().ToString().EndsWith(Suffix)) continue;
				if (Var.GetSizeInBytes() != sizeof(T)) return false;
				if (Script->RapidIterationParameters.SetParameterValue(Value, Var, false)) ++Matches;
			}
		}
		if (!Matches && bRequired) UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Template parameter not found: %s"), Suffix);
		return Matches > 0 || !bRequired;
	}

	bool SetOneShot(FVersionedNiagaraEmitterData* Data)
	{
		UNiagaraScriptSource* Source = Data->EmitterUpdateScriptProps.Script
			? Cast<UNiagaraScriptSource>(Data->EmitterUpdateScriptProps.Script->GetLatestSource()) : nullptr;
		if (!Source || !Source->NodeGraph) return false;
		const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
		for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
		{
			UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node);
			if (!Function || !Function->FunctionScript || Function->FunctionScript->GetName() != TEXT("EmitterState")) continue;
			const TPair<FName, int32> Settings[] = { { TEXT("Life Cycle Mode"), 1 }, { TEXT("Loop Behavior"), 1 }, { TEXT("Inactive Response"), 0 } };
			for (const auto& Setting : Settings)
			{
				UEdGraphPin* Pin = Function->FindPin(Setting.Key);
				if (!Pin || !Pin->LinkedTo.IsEmpty()) return false;
				FNiagaraVariable Variable(Schema->PinToTypeDefinition(Pin), Pin->PinName);
				Variable.SetValue<int32>(Setting.Value);
				FString Default;
				if (!Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable, Default)) return false;
				Pin->DefaultValue = Default;
			}
			Function->MarkNodeRequiresSynchronization(TEXT("Original slash one-shot lifetime"), true);
			return true;
		}
		return false;
	}

	UNiagaraSystem* BuildSystem(const TCHAR* Name, UNiagaraEmitter* Template, UStaticMesh* Mesh, UMaterial* Material,
		bool bEmbers, TArray<UObject*>& Assets)
	{
		UNiagaraSystem* System = GetAsset<UNiagaraSystem>(FString(TEXT("Niagara/")) + Name, Assets);
		TSet<FGuid> Previous;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Previous.Add(Handle.GetId());
		System->RemoveEmitterHandlesById(Previous);
		if (!System->GetSystemSpawnScript()->GetLatestSource()) UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
		if (System->GetEmitterHandles().Num() != 1) return nullptr;
		FNiagaraEmitterHandle& Handle = System->GetEmitterHandles()[0];
		const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data || !Instance.Emitter) return nullptr;
		Data->RemoveParent();
		Data->bLocalSpace = !bEmbers;
		Data->bDeterminism = true;
		Data->RandomSeed = bEmbers ? 527 : 913;
		Data->SimTarget = ENiagaraSimTarget::CPUSim;
		Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		Data->FixedBounds = FBox(FVector(-500), FVector(500));
		if (!SetOneShot(Data) || !SetRapid(Data, TEXT(".SpawnBurst_Instantaneous.Spawn Count"), int32(bEmbers ? 25 : 1))
			|| !SetRapid(Data, TEXT(".EmitterState.Loop Duration"), bEmbers ? .6f : .34f)) return nullptr;
		if (bEmbers)
		{
			if (!SetRapid(Data, TEXT(".InitializeParticle.Lifetime Min"), .25f)
				|| !SetRapid(Data, TEXT(".InitializeParticle.Lifetime Max"), .55f)
				|| !SetRapid(Data, TEXT(".InitializeParticle.Uniform Sprite Size Min"), 2.0f)
				|| !SetRapid(Data, TEXT(".InitializeParticle.Uniform Sprite Size Max"), 5.0f)) return nullptr;
			SetRapid(Data, TEXT(".RandomRangeFloat.Minimum"), 75.f, false);
			SetRapid(Data, TEXT(".RandomRangeFloat.Maximum"), 230.f, false);
			SetRapid(Data, TEXT(".AddVelocity.Velocity Speed Scale"), 1.f, false);
			SetRapid(Data, TEXT(".ShapeLocation.Sphere Radius"), 32.f, false);
			SetRapid(Data, TEXT(".Drag.Drag"), 2.8f, false);
			SetRapid(Data, TEXT(".GravityForce.Gravity"), FVector3f(0, 0, -155), false);
		}
		else if (!SetRapid(Data, TEXT(".InitializeParticle.Lifetime"), Mesh->GetName().Contains(TEXT("Filament")) ? .255f : .32f)) return nullptr;
		const TArray<UNiagaraRendererProperties*> OldRenderers = Data->GetRenderers();
		for (UNiagaraRendererProperties* Renderer : OldRenderers) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
		if (bEmbers)
		{
			auto* Renderer = NewObject<UNiagaraSpriteRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
			Renderer->Material = Material;
			Renderer->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
			Instance.Emitter->AddRenderer(Renderer, Instance.Version);
		}
		else
		{
			auto* Renderer = NewObject<UNiagaraMeshRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
			Renderer->Meshes.SetNum(1); Renderer->Meshes[0].Mesh = Mesh;
			Renderer->bOverrideMaterials = true;
			Renderer->OverrideMaterials.SetNum(1); Renderer->OverrideMaterials[0].ExplicitMat = Material;
			Instance.Emitter->AddRenderer(Renderer, Instance.Version);
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete(false, false);
		if (!System->IsValid())
		{
			UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Niagara compile failed: %s"), *System->GetPathName());
			return nullptr;
		}
		return System;
	}

	void BuildPresets(TArray<FAttack>& Plans, UNiagaraSystem* Slash, UNiagaraSystem* Filament,
		UNiagaraSystem* Embers, TArray<UObject*>& Assets)
	{
		for (FAttack& Attack : Plans)
			for (FBurst& B : Attack.Bursts)
			{
				B.Preset = GetAsset<UWuwaSlashFxPreset>(B.PresetPath.RightChop(FCString::Strlen(AssetRoot)), Assets);
				B.Preset->Layers.Reset();
				B.Preset->MaximumLifetime = .95f;
				auto Add = [&](UNiagaraSystem* System, FRotator Rotation, FVector Location, FVector Scale, float Delay)
				{
					FWuwaSlashFxLayer Layer;
					Layer.System = System; Layer.Transform = FTransform(Rotation, Location, Scale); Layer.DelaySeconds = Delay;
					B.Preset->Layers.Add(Layer);
				};
				Add(Slash, FRotator::ZeroRotator, FVector::ZeroVector, FVector::OneVector, 0);
				Add(Filament, FRotator(2, 7, 3), FVector(0, 0, 1.5), FVector(1.035), .016f);
				Add(Filament, FRotator(-3, -12, -2), FVector(0, 0, -2.0), FVector(.94), .027f);
				Add(Embers, FRotator::ZeroRotator, FVector(95, 15, 0), FVector::OneVector, .018f);
				// Two different planes make the fourth blow read as a crossed rising slash.
				if (B.Attack == 4) Add(Slash, FRotator(12, 20, -50), FVector(15, 0, -12), FVector(.73), .032f);
			}
	}

	void InstallNotifies(TArray<FAttack>& Plans, TArray<UObject*>& Assets)
	{
		for (FAttack& Attack : Plans)
		{
			UAnimMontage* Montage = Attack.Montage;
			Montage->Modify();
			Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& E)
			{
				return E.NotifyName.ToString().StartsWith(NotifyPrefix) && E.Notify && E.Notify->IsA<UWuwaAnimNotify_SlashFx>();
			});
			int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TEXT("Wuwa Slash FX"); });
			if (Track == INDEX_NONE)
			{
				Track = Montage->AnimNotifyTracks.AddDefaulted();
				Montage->AnimNotifyTracks[Track].TrackName = TEXT("Wuwa Slash FX");
				Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(1.f, .25f, .035f);
			}
			for (const FBurst& B : Attack.Bursts)
			{
				auto* Notify = NewObject<UWuwaAnimNotify_SlashFx>(Montage, NAME_None, RF_Transactional);
				Notify->Preset = B.Preset; Notify->SocketName = TEXT("Root");
				Notify->LocationOffset = B.Location; Notify->RotationOffset = B.Rotation; Notify->Scale = B.Scale;
				FAnimNotifyEvent& E = Montage->Notifies.AddDefaulted_GetRef();
				E.Notify = Notify; E.NotifyName = FName(*FString::Printf(TEXT("%sAttack%02d.%02d"), NotifyPrefix, B.Attack, B.Index));
				E.TrackIndex = Track; E.NotifyTriggerChance = 1.f; E.TriggerTimeOffset = 0;
				E.Link(Montage, B.Time);
				E.Guid = FGuid::NewGuid();
			}
			Montage->SortNotifies();
			Montage->RefreshCacheData();
			Assets.AddUnique(Montage);
		}
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args)) return false;
		UE_LOG(LogWuwaSlashFxSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaSlashFxSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaSlashFxSetup;
	FString Reference = FPaths::ProjectDir() / TEXT("Docs/References/SlashFx/AttackFxReference.json");
	FParse::Value(*Params, TEXT("Reference="), Reference);
	TArray<FAttack> Plans;
	if (!ReadAttackPlans(Reference, Plans))
	{
		UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Attack/reference preflight failed. No assets changed: %s"), *Reference);
		return 1;
	}
	UNiagaraEmitter* SlashTemplate = LoadObject<UNiagaraEmitter>(nullptr, SlashTemplatePath);
	UNiagaraEmitter* EmberTemplate = LoadObject<UNiagaraEmitter>(nullptr, EmberTemplatePath);
	if (!SlashTemplate || !EmberTemplate) return 1;
	TArray<FAssetPlan> AssetPlans;
	auto AddPlan = [&](const TCHAR* Relative, UClass* Class) { AssetPlans.Add({ FString(AssetRoot) + Relative, Class, nullptr }); };
	AddPlan(TEXT("Materials/M_ChangliSlash"), UMaterial::StaticClass());
	AddPlan(TEXT("Materials/M_ChangliEmber"), UMaterial::StaticClass());
	AddPlan(TEXT("Mesh/SM_ChangliSlash"), UStaticMesh::StaticClass());
	AddPlan(TEXT("Mesh/SM_ChangliFilament"), UStaticMesh::StaticClass());
	AddPlan(TEXT("Niagara/NS_ChangliSlash"), UNiagaraSystem::StaticClass());
	AddPlan(TEXT("Niagara/NS_ChangliFilament"), UNiagaraSystem::StaticClass());
	AddPlan(TEXT("Niagara/NS_ChangliEmbers"), UNiagaraSystem::StaticClass());
	for (const FAttack& A : Plans) for (const FBurst& B : A.Bursts) AssetPlans.Add({ B.PresetPath, UWuwaSlashFxPreset::StaticClass(), nullptr });
	TArray<UObject*> Backups;
	for (const FAttack& A : Plans) Backups.Add(A.Montage);
	for (FAssetPlan& A : AssetPlans)
	{
		if (!PreflightAsset(A)) return 1;
		if (A.Existing) Backups.Add(A.Existing);
	}
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogWuwaSlashFxSetup, Display, TEXT("Read-only preflight passed: 5 montages, 11 authored bursts, %d original art assets. Use -Apply to generate."), AssetPlans.Num());
		return 0;
	}
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/SlashFx/Backup")
		/ (FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (!Backup(Backups, BackupRoot)) return 1;
	TArray<UObject*> Changed;
	UMaterial* SlashMaterial = BuildMaterial(false, Changed);
	UMaterial* EmberMaterial = BuildMaterial(true, Changed);
	UStaticMesh* SlashMesh = BuildArc(false, SlashMaterial, Changed);
	UStaticMesh* FilamentMesh = BuildArc(true, SlashMaterial, Changed);
	if (!SlashMesh || !FilamentMesh) return 1;
	UNiagaraSystem* Slash = BuildSystem(TEXT("NS_ChangliSlash"), SlashTemplate, SlashMesh, SlashMaterial, false, Changed);
	UNiagaraSystem* Filament = BuildSystem(TEXT("NS_ChangliFilament"), SlashTemplate, FilamentMesh, SlashMaterial, false, Changed);
	UNiagaraSystem* Embers = BuildSystem(TEXT("NS_ChangliEmbers"), EmberTemplate, nullptr, EmberMaterial, true, Changed);
	if (!Slash || !Filament || !Embers)
	{
		UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Niagara generation/compilation failed. No assets saved."));
		return 1;
	}
	BuildPresets(Plans, Slash, Filament, Embers, Changed);
	InstallNotifies(Plans, Changed);
	// Dependencies precede presets and montages in this list. No montage is saved until all art has compiled.
	for (UObject* Asset : Changed)
		if (!Save(Asset))
		{
			UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("Save failed; earlier saves may have succeeded. Restore from %s"), *BackupRoot);
			return 1;
		}
	UE_LOG(LogWuwaSlashFxSetup, Display, TEXT("Original red/gold slash setup complete. 11 FX notifies; skill windows unchanged. Backup: %s"), *BackupRoot);
	return 0;
#else
	UE_LOG(LogWuwaSlashFxSetup, Error, TEXT("WuwaSlashFxSetup requires an editor build."));
	return 1;
#endif
}
