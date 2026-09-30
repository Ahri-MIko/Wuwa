#include "Editor/WuwaSlashFxImportCommandlet.h"

#if WITH_EDITOR
#include "WuwaSlashFxReferenceNiagara.h"
#include "Animation/AnimMontage.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SlashFx.h"
#include "Game/Render/Effect/WuwaSlashFxPreset.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "RHI.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "StaticMeshAttributes.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSlashFxImport, Log, All);

UWuwaSlashFxImportCommandlet::UWuwaSlashFxImportCommandlet()
{
	IsEditor = true; IsClient = false; IsServer = false; LogToConsole = true;
	HelpDescription = TEXT("Imports first-attack reference meshes, packed textures, reconstructed materials and Niagara layers. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaSlashFxImport [-Apply] [-Recipe=path/to/Attack01.json]");
}

#if WITH_EDITOR
namespace WuwaSlashFxImport
{
	const FString Root = TEXT("/Game/Effects/ChangliSlash/Reference/");
	const TCHAR* OwnerKey = TEXT("WuwaSlashFxGenerator");
	const TCHAR* Owner = TEXT("ChangliReferenceAttack01V2");
	const TCHAR* PresetPath = TEXT("/Game/Effects/ChangliSlash/Presets/DA_Attack01_01.DA_Attack01_01");
	const TCHAR* MontagePath = TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01");

	TSharedPtr<FJsonObject> ReadJson(const FString& File)
	{
		FString Text; TSharedPtr<FJsonObject> Object;
		if (!FFileHelper::LoadFileToString(Text, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object)) return nullptr;
		return Object;
	}
	FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()); }
	FString ObjectPath(const FString& Package) { return Package + TEXT(".") + FPackageName::GetLongPackageAssetName(Package); }

	bool Backup(UObject* Asset, const FString& Directory)
	{
		FString Package = Asset->GetOutermost()->GetName(), Relative = Package;
		if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
		for (const TCHAR* Extension : {TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk")})
		{
			const FString File = FPackageName::LongPackageNameToFilename(Package, Extension);
			if (!IFileManager::Get().FileExists(*File)) continue;
			const FString Destination = Directory / (Relative + Extension);
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
				|| IFileManager::Get().Copy(*Destination, *File, false, true) != COPY_OK) return false;
		}
		return true;
	}

	struct FImport
	{
		TArray<UObject*> Assets;
		FString BackupDirectory;
		bool bFailed = false;
		template<class T> T* Asset(const FString& Relative)
		{
			const FString PackagePath = Root + Relative;
			const bool bExistingPackage = FPackageName::DoesPackageExist(PackagePath);
			T* Result = bExistingPackage ? LoadObject<T>(nullptr, *ObjectPath(PackagePath)) : nullptr;
			if (Result)
			{
				if (Result->GetOutermost()->GetMetaData().GetValue(Result, OwnerKey) != Owner || !Backup(Result, BackupDirectory))
				{ bFailed = true; return nullptr; }
			}
			else
			{
				if (bExistingPackage) { bFailed = true; return nullptr; }
				Result = NewObject<T>(CreatePackage(*PackagePath), *FPackageName::GetLongPackageAssetName(PackagePath), RF_Public | RF_Standalone | RF_Transactional);
				FAssetRegistryModule::AssetCreated(Result);
			}
			Result->Modify(); Result->GetOutermost()->GetMetaData().SetValue(Result, OwnerKey, Owner);
			Assets.AddUnique(Result); return Result;
		}
	};

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *File, Args);
	}

	UTexture2D* ImportTexture(FImport& Import, const TSharedPtr<FJsonObject>& Def, const FString& SourceRoot)
	{
		TArray64<uint8> Compressed, Pixels;
		const FString File = SourceRoot / Def->GetStringField(TEXT("file"));
		if (!FFileHelper::LoadFileToArray(Compressed, *File)) return nullptr;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> PNG = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG->SetCompressed(Compressed.GetData(), Compressed.Num()) || !PNG->GetRaw(ERGBFormat::BGRA, 8, Pixels)) return nullptr;
		UTexture2D* Texture = Import.Asset<UTexture2D>(TEXT("Textures/") + Def->GetStringField(TEXT("name")));
		if (!Texture) return nullptr;
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(PNG->GetWidth(), PNG->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = Def->GetBoolField(TEXT("srgb"));
		// Packed channels must survive compression: many feather silhouettes use RGB, not alpha.
		Texture->CompressionSettings = Texture->SRGB ? TC_EditorIcon : TC_VectorDisplacementmap;
		Texture->MipGenSettings = TMGS_FromTextureGroup;
		Texture->AddressX = TA_Wrap; Texture->AddressY = TA_Wrap;
		Texture->LODGroup = TEXTUREGROUP_Effects;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange(); return Texture;
	}

	UStaticMesh* ImportMesh(FImport& Import, const FString& Name, const FString& SourceRoot)
	{
		auto Json = ReadJson(SourceRoot / TEXT("Geometry") / (Name + TEXT(".json")));
		if (!Json) return nullptr;
		const auto& Points = Json->GetArrayField(TEXT("positions"));
		const auto& Indices = Json->GetArrayField(TEXT("wedgePointIndices"));
		const auto& UVData = Json->GetArrayField(TEXT("uvChannels"))[0]->AsArray();
		const auto& NormalData = Json->GetArrayField(TEXT("normals"));
		const auto& ColorData = Json->GetArrayField(TEXT("colorsRGBA8"));
		if (Indices.Num() != UVData.Num() || Indices.Num() != NormalData.Num() || Indices.Num() != ColorData.Num()) return nullptr;
		UStaticMesh* Mesh = Import.Asset<UStaticMesh>(TEXT("Mesh/") + Name);
		if (!Mesh) return nullptr;
		FMeshDescription Desc; FStaticMeshAttributes A(Desc); A.Register();
		auto Positions = A.GetVertexPositions(); auto Normals = A.GetVertexInstanceNormals();
		auto Colors = A.GetVertexInstanceColors(); auto UVs = A.GetVertexInstanceUVs(); UVs.SetNumChannels(1);
		TArray<FVertexID> Verts; TArray<FVertexInstanceID> Wedges;
		for (const auto& P : Points) { const FVertexID V = Desc.CreateVertex(); Positions[V] = FVector3f(Vec(P->AsArray())); Verts.Add(V); }
		for (int32 I=0; I<Indices.Num(); ++I)
		{
			const int32 Point = int32(Indices[I]->AsNumber()); if (!Verts.IsValidIndex(Point)) return nullptr;
			const auto W = Desc.CreateVertexInstance(Verts[Point]); Wedges.Add(W);
			const auto& UV = UVData[I]->AsArray(); const auto& C = ColorData[I]->AsArray();
			UVs.Set(W, 0, FVector2f(UV[0]->AsNumber(), UV[1]->AsNumber()));
			Normals[W] = FVector3f(Vec(NormalData[I]->AsArray()));
			Colors[W] = FVector4f(C[0]->AsNumber()/255.f,C[1]->AsNumber()/255.f,C[2]->AsNumber()/255.f,C[3]->AsNumber()/255.f);
		}
		const auto Group = Desc.CreatePolygonGroup(); A.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Reference");
		for (const auto& T : Json->GetArrayField(TEXT("triangles")))
		{
			const auto& Tri = T->AsArray(); FVertexInstanceID Face[3];
			for (int32 K=0; K<3; ++K) { int32 W = int32(Tri[K]->AsNumber()); if (!Wedges.IsValidIndex(W)) return nullptr; Face[K] = Wedges[W]; }
			Desc.CreateTriangle(Group, MakeArrayView(Face));
		}
		Mesh->GetStaticMaterials().Reset(); Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Reference")));
		Mesh->SetNumSourceModels(1);
		FMeshBuildSettings& BuildSettings = Mesh->GetSourceModel(0).BuildSettings;
		BuildSettings.bRecomputeNormals = false;
		BuildSettings.bRecomputeTangents = true;
		BuildSettings.bGenerateLightmapUVs = false;
		UStaticMesh::FBuildMeshDescriptionsParams Options; Options.bBuildSimpleCollision=false; Options.bUseHashAsGuid=true; Options.bAllowCpuAccess=true;
		if (!Mesh->BuildFromMeshDescriptions({&Desc}, Options)) return nullptr;
		UE_LOG(LogWuwaSlashFxImport, Display, TEXT("Imported %s: %d points / %d wedges / %d faces"), *Name, Points.Num(), Wedges.Num(), Desc.Triangles().Num());
		return Mesh;
	}

	template<class T> T* Expression(UMaterial* Material)
	{
		T* E = NewObject<T>(Material, NAME_None, RF_Transactional);
		E->MaterialExpressionEditorX = -500; E->MaterialExpressionEditorY = Material->GetExpressions().Num()*80;
		Material->GetExpressionCollection().AddExpression(E); return E;
	}
	void Input(UMaterialExpressionCustom* Code, FName Name, UMaterialExpression* E, int32 Output=0)
	{ FCustomInput I; I.InputName=Name; I.Input.Connect(Output,E); Code->Inputs.Add(I); }

	UMaterial* BuildMaterial(FImport& Import, const TSharedPtr<FJsonObject>& Def, const TMap<FString,UTexture2D*>& Textures)
	{
		UMaterial* M = Import.Asset<UMaterial>(TEXT("Materials/M_") + Def->GetStringField(TEXT("id")));
		if (!M) return nullptr;
		M->PreEditChange(nullptr); M->GetExpressionCollection().Empty();
		M->GetEditorOnlyData()->WorldPositionOffset = FVectorMaterialInput();
		M->BlendMode = BLEND_Translucent; M->SetShadingModel(MSM_Unlit); M->TwoSided=true;
		const bool Sprite = Def->GetBoolField(TEXT("sprite"));
		M->bUsedWithNiagaraSprites=Sprite; M->bUsedWithNiagaraMeshParticles=!Sprite;
		auto* UV=Expression<UMaterialExpressionTextureCoordinate>(M);
		auto* Age=Expression<UMaterialExpressionParticleRelativeTime>(M);
		auto* Color=Expression<UMaterialExpressionVertexColor>(M);
		auto* Art=Expression<UMaterialExpressionCustom>(M); Art->OutputType=CMOT_Float4;
		Art->Description=TEXT("Reconstructed packed-channel material using exported textures and sampled color curve; not original master shader");
		Art->Inputs.Reset(); Input(Art,TEXT("UV"),UV); Input(Art,TEXT("Age"),Age); Input(Art,TEXT("VertexAlpha"),Color,4);
		for (const auto& Pair : Def->GetObjectField(TEXT("textures"))->Values)
		{
			UTexture2D* const* Texture = Textures.Find(Pair.Value->AsString()); if (!Texture) return nullptr;
			auto* T=Expression<UMaterialExpressionTextureObjectParameter>(M); T->ParameterName=FName(*Pair.Key); T->Texture=*Texture;
			T->SamplerType=(*Texture)->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
			Input(Art,FName(*Pair.Key),T);
		}
		auto* Intensity=Expression<UMaterialExpressionScalarParameter>(M); Intensity->ParameterName=TEXT("Intensity"); Intensity->DefaultValue=Def->GetNumberField(TEXT("intensity"));
		Intensity->SliderMin=0; Intensity->SliderMax=12; Input(Art,TEXT("Intensity"),Intensity);
		Art->Code=Def->GetStringField(TEXT("shader"));
		auto* RGB=Expression<UMaterialExpressionComponentMask>(M); RGB->Input.Connect(0,Art); RGB->R=RGB->G=RGB->B=true; RGB->A=false;
		auto* Alpha=Expression<UMaterialExpressionComponentMask>(M); Alpha->Input.Connect(0,Art); Alpha->R=Alpha->G=Alpha->B=false; Alpha->A=true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0,RGB); M->GetEditorOnlyData()->Opacity.Connect(0,Alpha);
		FString Wpo;
		if (!Sprite && Def->TryGetStringField(TEXT("wpo"),Wpo) && !Wpo.IsEmpty())
		{
			auto* P=Expression<UMaterialExpressionPreSkinnedPosition>(M);
			auto* Warp=Expression<UMaterialExpressionCustom>(M); Warp->OutputType=CMOT_Float3; Warp->Inputs.Reset();
			Input(Warp,TEXT("P"),P); Input(Warp,TEXT("Age"),Age); Warp->Code=Wpo;
			auto* ToWorld=Expression<UMaterialExpressionTransform>(M); ToWorld->TransformSourceType=TRANSFORMSOURCE_Instance; ToWorld->TransformType=TRANSFORM_World; ToWorld->Input.Connect(0,Warp);
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0,ToWorld);
		}
		M->PostEditChange(); return M;
	}
}
#endif

int32 UWuwaSlashFxImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaSlashFxImport;
	FString RecipeFile=FPaths::ProjectDir()/TEXT("ArtSource/ChangliSlash/Attack01.json"); FParse::Value(*Params,TEXT("Recipe="),RecipeFile);
	const auto Recipe=ReadJson(RecipeFile); if (!Recipe) { UE_LOG(LogWuwaSlashFxImport,Error,TEXT("Missing recipe %s"),*RecipeFile); return 1; }
	const FString SourceRoot=FPaths::GetPath(RecipeFile);
	for (const auto& V:Recipe->GetArrayField(TEXT("textures"))) if (!IFileManager::Get().FileExists(*(SourceRoot/V->AsObject()->GetStringField(TEXT("file"))))) return 2;
	for (const auto& V:Recipe->GetArrayField(TEXT("meshes"))) if (!ReadJson(SourceRoot/TEXT("Geometry")/(V->AsString()+TEXT(".json")))) return 3;
	UWuwaSlashFxPreset* Preset=LoadObject<UWuwaSlashFxPreset>(nullptr,PresetPath);
	UAnimMontage* Montage=LoadObject<UAnimMontage>(nullptr,MontagePath);
	if (!Preset || !Montage) return 4;
	UWuwaAnimNotify_SlashFx* Notify=nullptr;
	for (const auto& E:Montage->Notifies) if (auto* N=Cast<UWuwaAnimNotify_SlashFx>(E.Notify.Get()); N && N->Preset==Preset)
	{ if (Notify) return 5; Notify=N; }
	if (!Notify) return 6;
	if (!FParse::Param(*Params,TEXT("Apply"))) { UE_LOG(LogWuwaSlashFxImport,Display,TEXT("Source art and Attack01 target verified. Add -Apply to import.")); return 0; }
	FImport Import; Import.BackupDirectory=FPaths::ProjectSavedDir()/TEXT("Diagnostics/SlashFx/ReferenceBackup")/(FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT("-")+FGuid::NewGuid().ToString());
	if (!Backup(Preset,Import.BackupDirectory) || !Backup(Montage,Import.BackupDirectory)) return 7;
	TMap<FString,UTexture2D*> Textures;
	for (const auto& V:Recipe->GetArrayField(TEXT("textures"))) { auto D=V->AsObject(); auto* T=ImportTexture(Import,D,SourceRoot); if (!T) return 8; Textures.Add(D->GetStringField(TEXT("name")),T); }
	TMap<FString,UStaticMesh*> Meshes;
	for (const auto& V:Recipe->GetArrayField(TEXT("meshes"))) { auto* M=ImportMesh(Import,V->AsString(),SourceRoot); if (!M) return 9; Meshes.Add(V->AsString(),M); }
	TMap<FString,UNiagaraSystem*> Systems;
	for (const auto& V:Recipe->GetArrayField(TEXT("systems")))
	{
		auto D=V->AsObject(); TArray<WuwaSlashFxReferenceNiagara::FReferenceEmitterSpec> Specs;
		for (const auto& L:D->GetArrayField(TEXT("layers")))
		{
			auto Def=L->AsObject(); WuwaSlashFxReferenceNiagara::FReferenceEmitterSpec Spec;
			Spec.Name=FName(*Def->GetStringField(TEXT("name"))); Spec.Material=BuildMaterial(Import,Def,Textures); if (!Spec.Material) return 10;
			Spec.Mesh=Meshes.FindRef(Def->GetStringField(TEXT("mesh"))); Spec.Lifetime=Def->GetNumberField(TEXT("lifetime"));
			Spec.Count=Def->GetIntegerField(TEXT("count")); Spec.SortOrder=Def->GetIntegerField(TEXT("sortOrder"));
			Spec.PivotOffset=Vec(Def->GetArrayField(TEXT("pivot")));
			if (!Spec.Mesh) { const auto& Size=Def->GetArrayField(TEXT("spriteSize")); Spec.SpriteSize=FVector2D(Size[0]->AsNumber(),Size[1]->AsNumber()); Spec.SpawnRadius=Def->GetNumberField(TEXT("spawnRadius")); Spec.SpeedMin=Def->GetNumberField(TEXT("speedMin")); Spec.SpeedMax=Def->GetNumberField(TEXT("speedMax")); Spec.Drag=1.5f; }
			Specs.Add(Spec);
		}
		auto* S=Import.Asset<UNiagaraSystem>(TEXT("Niagara/")+D->GetStringField(TEXT("name")));
		if (!S || !WuwaSlashFxReferenceNiagara::BuildReferenceSystem(S,Specs)) return 11;
		Systems.Add(D->GetStringField(TEXT("name")),S);
	}
	FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	if (Import.bFailed) return 12;
	for (UObject* A:Import.Assets)
	{
		if (auto* M=Cast<UMaterial>(A))
			if (const FMaterialResource* Resource=M->GetMaterialResource(GMaxRHIShaderPlatform); Resource && !Resource->GetCompileErrors().IsEmpty())
			{
				for (const FString& Error:Resource->GetCompileErrors()) UE_LOG(LogWuwaSlashFxImport,Error,TEXT("%s: %s"),*M->GetName(),*Error);
				return 16;
			}
	}
	for (UObject* A:Import.Assets) if (!Save(A)) return 13;
	// Switch the existing preset only after every new art asset compiled and saved.
	Preset->Modify(); Preset->Layers.Reset(); Preset->MaximumLifetime=1.5f;
	// The old procedural generator must not silently replace an imported preset.
	Preset->GetOutermost()->GetMetaData().SetValue(Preset,OwnerKey,Owner);
	for (const auto& V:Recipe->GetArrayField(TEXT("placements")))
	{
		auto D=V->AsObject(); FWuwaSlashFxLayer L; L.System=Systems.FindRef(D->GetStringField(TEXT("system"))); if (!L.System) return 14;
		const FVector R=Vec(D->GetArrayField(TEXT("rotation")));
		L.Transform=FTransform(FRotator(R.X,R.Y,R.Z),Vec(D->GetArrayField(TEXT("location"))),Vec(D->GetArrayField(TEXT("scale"))));
		L.DelaySeconds=D->GetNumberField(TEXT("delay"));
		const TArray<TSharedPtr<FJsonValue>>* YawKeys=nullptr;
		if (D->TryGetArrayField(TEXT("localYawKeys"),YawKeys))
		{
			for (const auto& VKey:*YawKeys)
			{
				auto K=VKey->AsObject(); auto* Curve=L.LocalYawDegrees.GetRichCurve();
				const auto H=Curve->AddKey(K->GetNumberField(TEXT("time")),K->GetNumberField(TEXT("value")));
				FRichCurveKey& Key=Curve->GetKey(H); Key.InterpMode=RCIM_Cubic; Key.TangentMode=RCTM_User;
				Key.ArriveTangent=K->GetNumberField(TEXT("arriveTangent")); Key.LeaveTangent=K->GetNumberField(TEXT("leaveTangent"));
			}
		}
		Preset->Layers.Add(L);
	}
	Montage->Modify(); Notify->Modify(); Notify->LocationOffset=FVector::ZeroVector; Notify->RotationOffset=FRotator::ZeroRotator; Notify->Scale=FVector::OneVector;
	if (!Save(Preset) || !Save(Montage)) return 15;
	UE_LOG(LogWuwaSlashFxImport,Display,TEXT("Imported %d reference art assets; Attack01 preset has %d layers. Backup: %s"),Import.Assets.Num(),Preset->Layers.Num(),*Import.BackupDirectory);
	return 0;
#else
	return 1;
#endif
}
