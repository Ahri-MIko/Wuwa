#include "WuwaSlashFxReferenceNiagara.h"

#if WITH_EDITOR
#include "EdGraphSchema_Niagara.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/UnrealType.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaReferenceNiagara, Log, All);

namespace WuwaSlashFxReferenceNiagara
{
#if WITH_EDITOR
	namespace
	{
		template<class T>
		bool SetRapid(FVersionedNiagaraEmitterData& Data, const TCHAR* Suffix, const T& Value)
		{
			TArray<UNiagaraScript*> Scripts;
			Data.GetScripts(Scripts, false);
			bool bFound = false;
			for (UNiagaraScript* Script : Scripts)
			{
				if (!Script) continue;
				TArray<FNiagaraVariable> Variables;
				Script->RapidIterationParameters.GetParameters(Variables);
				for (const FNiagaraVariable& Variable : Variables)
				{
					if (!Variable.GetName().ToString().EndsWith(Suffix)) continue;
					if (Variable.GetSizeInBytes() != sizeof(T))
					{
						UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Template parameter has an unexpected type: %s"), Suffix);
						return false;
					}
					bFound |= Script->RapidIterationParameters.SetParameterValue(Value, Variable, false);
				}
			}
			if (!bFound) UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Required template parameter missing: %s"), Suffix);
			return bFound;
		}

		bool SetOneShot(FVersionedNiagaraEmitterData& Data)
		{
			UNiagaraScriptSource* Source = Data.EmitterUpdateScriptProps.Script
				? Cast<UNiagaraScriptSource>(Data.EmitterUpdateScriptProps.Script->GetLatestSource()) : nullptr;
			if (!Source || !Source->NodeGraph) return false;
			const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
			for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
			{
				auto* Function = Cast<UNiagaraNodeFunctionCall>(Node);
				if (!Function || !Function->FunctionScript || Function->FunctionScript->GetName() != TEXT("EmitterState")) continue;
				const TPair<FName, int32> Settings[] = {
					{ TEXT("Life Cycle Mode"), 1 }, { TEXT("Loop Behavior"), 1 }, { TEXT("Inactive Response"), 0 }
				};
				for (const auto& Setting : Settings)
				{
					UEdGraphPin* Pin = Function->FindPin(Setting.Key);
					if (!Pin || !Pin->LinkedTo.IsEmpty()) return false;
					FNiagaraVariable Variable(Schema->PinToTypeDefinition(Pin), Pin->PinName);
					Variable.SetValue<int32>(Setting.Value);
					if (!Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable, Pin->DefaultValue)) return false;
				}
				Function->MarkNodeRequiresSynchronization(TEXT("Reference effect: self / once / complete"), true);
				return true;
			}
			return false;
		}

		bool SetCustomInput(UNiagaraNodeAssignment& Assignment, const FNiagaraVariable& Target, const FString& Hlsl)
		{
			const FName ModuleInput(*FString::Printf(TEXT("Module.%s"), *Target.GetName().ToString()));
			const FNiagaraParameterHandle Handle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
				FNiagaraParameterHandle(ModuleInput), &Assignment);
			UEdGraphPin& Override = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
				Assignment, Handle, Target.GetType(), FGuid(), FGuid());
			if (!Override.LinkedTo.IsEmpty()) return false;
			UEdGraphNode* OverrideNode = Override.GetOwningNode();
			UEdGraphPin* MapInput = nullptr;
			for (UEdGraphPin* Pin : OverrideNode->Pins)
				if (Pin->Direction == EGPD_Input
					&& UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef()) MapInput = Pin;
			if (!MapInput || MapInput->LinkedTo.Num() != 1) return false;

			// UE's SetCustomExpressionForFunctionInput and CustomHlsl setters are not DLL-exported.
			// Build the same serialized node using its public signature and reflected HLSL property;
			// the result is a normal editable Custom HLSL dynamic input in the Niagara stack.
			FStrProperty* CodeProperty = FindFProperty<FStrProperty>(UNiagaraNodeCustomHlsl::StaticClass(), TEXT("CustomHlsl"));
			if (!CodeProperty) return false;
			FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*OverrideNode->GetGraph());
			UNiagaraNodeCustomHlsl* Custom = Creator.CreateNode();
			Custom->ScriptUsage = ENiagaraScriptUsage::DynamicInput;
			Custom->Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map")));
			Custom->Signature.Outputs.Add(FNiagaraVariable(Target.GetType(), TEXT("CustomHLSLOutput")));
			CodeProperty->SetPropertyValue_InContainer(Custom, Hlsl);
			Custom->NodeComment = TEXT("Deterministic local XY ring; tangent velocity instead of sphere-outward velocity");
			Creator.Finalize();
			UEdGraphPin* Input = Custom->FindPin(TEXT("Map"), EGPD_Input);
			UEdGraphPin* Output = Custom->FindPin(TEXT("CustomHLSLOutput"), EGPD_Output);
			if (!Input || !Output) return false;
			Input->MakeLinkTo(MapInput->LinkedTo[0]);
			Output->MakeLinkTo(&Override);
			Custom->MarkNodeRequiresSynchronization(TEXT("Reference feather ring expression"), true);
			return true;
		}

		bool SetSpriteRing(FVersionedNiagaraEmitterData& Data, const FReferenceEmitterSpec& Spec)
		{
			UNiagaraScript* Spawn = Data.SpawnScriptProps.Script;
			auto* Source = Spawn ? Cast<UNiagaraScriptSource>(Spawn->GetLatestSource()) : nullptr;
			UNiagaraNodeOutput* Output = Source && Source->NodeGraph
				? Source->NodeGraph->FindEquivalentOutputNode(Spawn->GetUsage(), Spawn->GetUsageId()) : nullptr;
			if (!Output) return false;
			// The final spawn module replaces the template's sphere position and radial velocity.
			// Every output remains editable in Niagara and is shared by the renderer's usual bindings.
			FNiagaraVariable Size(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Particles.SpriteSize"));
			Size.SetValue(FVector2f(Spec.SpriteSize));
			FNiagaraVariable Facing(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteFacing"));
			Facing.SetValue(FVector3f(0, 0, 1));
			FNiagaraVariable Position(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Particles.Position"));
			Position.SetValue(FVector3f::ZeroVector);
			FNiagaraVariable Velocity(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.Velocity"));
			Velocity.SetValue(FVector3f::ZeroVector);
			TArray<FNiagaraVariable> Variables = { Size, Facing, Position, Velocity };
			TArray<FString> Defaults;
			for (const FNiagaraVariable& Variable : Variables)
			{
				FString Default;
				if (!GetDefault<UEdGraphSchema_Niagara>()->TryGetPinDefaultValueFromNiagaraVariable(Variable, Default)) return false;
				Defaults.Add(Default);
			}
			auto* Assignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *Output, INDEX_NONE, Defaults);
			if (!Assignment) return false;
			Assignment->NodeComment = TEXT("Feather ring: local XY position, tangent velocity, plane-facing normal and width / length");
			// Stratify around the authored arc, with deterministic per-particle jitter. Both dynamic
			// inputs use the same angle formula so velocity is orthogonal to the spawn radius.
			// DynamicInput wraps this text as one expression, not a function body.
			const FString Angle = FString::Printf(TEXT(
				"(%.9f + (float(Engine.ExecIndex) + 0.2 + "
				"frac(sin((float(Engine.ExecIndex) + %.9f) * 12.9898) * 43758.5453) * 0.6) / %.9f * %.9f)"),
				FMath::DegreesToRadians(double(Spec.RingStartAngleDegrees)), double(Spec.RandomSeed) + 1.0,
				double(Spec.Count), FMath::DegreesToRadians(double(Spec.RingArcDegrees)));
			const FString PositionHlsl = FString::Printf(TEXT(
				"float3(cos(%s) * %.9f, sin(%s) * %.9f, "
				"(frac(sin((float(Engine.ExecIndex) + %.9f) * 78.233) * 24634.6345) - 0.5) * %.9f)"),
				*Angle, double(Spec.SpawnRadius), *Angle, double(Spec.SpawnRadius),
				double(Spec.RandomSeed) + 7.0, double(Spec.RingHeight));
			const FString VelocityHlsl = FString::Printf(TEXT(
				"(float3(-sin(%s), cos(%s), 0.0) * "
				"lerp(%.9f, %.9f, frac(sin((float(Engine.ExecIndex) + %.9f) * 39.3467) * 47453.5453)) * %.9f)"),
				*Angle, *Angle, double(Spec.SpeedMin), double(Spec.SpeedMax),
				double(Spec.RandomSeed) + 19.0, Spec.bClockwise ? -1.0 : 1.0);
			if (!SetCustomInput(*Assignment, Position, PositionHlsl) || !SetCustomInput(*Assignment, Velocity, VelocityHlsl)) return false;
			Assignment->MarkNodeRequiresSynchronization(TEXT("Reference feather ring spawn"), true);
			return true;
		}

		bool ConfigureEmitter(FNiagaraEmitterHandle& Handle, const FReferenceEmitterSpec& Spec)
		{
			const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
			FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
			if (!Data || !Instance.Emitter) return false;
			Data->RemoveParent();
			Data->bLocalSpace = Spec.bLocalSpace;
			Data->bDeterminism = true;
			Data->RandomSeed = Spec.RandomSeed;
			Data->SimTarget = ENiagaraSimTarget::CPUSim;
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
			Data->FixedBounds = FBox(FVector(-600), FVector(600));
			const bool bSprite = Spec.Mesh == nullptr;
			if (!SetOneShot(*Data)
				|| !SetRapid(*Data, TEXT(".SpawnBurst_Instantaneous.Spawn Count"), bSprite ? Spec.Count : 1)
				|| !SetRapid(*Data, TEXT(".EmitterState.Loop Duration"), Spec.Lifetime + .025f)) return false;
			if (bSprite)
			{
				if (!SetRapid(*Data, TEXT(".InitializeParticle.Lifetime Min"), Spec.Lifetime)
					|| !SetRapid(*Data, TEXT(".InitializeParticle.Lifetime Max"), Spec.Lifetime)
					|| !SetRapid(*Data, TEXT(".RandomRangeFloat.Minimum"), 0.f)
					|| !SetRapid(*Data, TEXT(".RandomRangeFloat.Maximum"), 0.f)
					|| !SetRapid(*Data, TEXT(".AddVelocity.Velocity Speed Scale"), 0.f)
					|| !SetRapid(*Data, TEXT(".ShapeLocation.Sphere Radius"), 0.f)
					|| !SetRapid(*Data, TEXT(".Drag.Drag"), Spec.Drag)
					|| !SetRapid(*Data, TEXT(".GravityForce.Gravity"), FVector3f(Spec.Gravity))
					|| !SetSpriteRing(*Data, Spec)) return false;
			}
			else if (!SetRapid(*Data, TEXT(".InitializeParticle.Lifetime"), Spec.Lifetime)) return false;

			const TArray<UNiagaraRendererProperties*> Previous = Data->GetRenderers();
			for (UNiagaraRendererProperties* Renderer : Previous) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
			if (bSprite)
			{
				auto* Renderer = NewObject<UNiagaraSpriteRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Material = Spec.Material;
				Renderer->SortOrderHint = Spec.SortOrder;
				// Both source feather renderers anchor at the end of the texture.
				Renderer->PivotInUVSpace = FVector2D(0.5, 1.0);
				Renderer->FacingMode = ENiagaraSpriteFacingMode::CustomFacingVector;
				Renderer->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
				Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			}
			else
			{
				auto* Renderer = NewObject<UNiagaraMeshRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Meshes.SetNum(1);
				Renderer->Meshes[0].Mesh = Spec.Mesh;
				Renderer->Meshes[0].PivotOffset = Spec.PivotOffset;
				Renderer->Meshes[0].PivotOffsetSpace = ENiagaraMeshPivotOffsetSpace::Mesh;
				Renderer->SortOrderHint = Spec.SortOrder;
				Renderer->bOverrideMaterials = true;
				Renderer->OverrideMaterials.SetNum(1);
				Renderer->OverrideMaterials[0].ExplicitMat = Spec.Material;
				Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			}
			return true;
		}
	}
#endif

	bool BuildReferenceSystem(UNiagaraSystem* System, const TArray<FReferenceEmitterSpec>& Specs)
	{
#if WITH_EDITOR
		if (!IsValid(System) || Specs.IsEmpty()) return false;
		TSet<FName> Names;
		for (const FReferenceEmitterSpec& Spec : Specs)
		{
			if (Spec.Name.IsNone() || Names.Contains(Spec.Name) || !IsValid(Spec.Material)
				|| !FMath::IsFinite(Spec.Lifetime) || Spec.Lifetime <= 0 || Spec.Count <= 0
				|| Spec.PivotOffset.ContainsNaN() || Spec.Gravity.ContainsNaN()
				|| !FMath::IsFinite(Spec.SpawnRadius) || Spec.SpawnRadius < 0
				|| !FMath::IsFinite(Spec.RingHeight) || Spec.RingHeight < 0
				|| !FMath::IsFinite(Spec.RingArcDegrees) || Spec.RingArcDegrees <= 0 || Spec.RingArcDegrees > 360
				|| !FMath::IsFinite(Spec.RingStartAngleDegrees)
				|| (!Spec.Mesh && !Spec.bLocalSpace)
				|| !FMath::IsFinite(Spec.SpeedMin) || !FMath::IsFinite(Spec.SpeedMax)
				|| Spec.SpeedMin < 0 || Spec.SpeedMax < Spec.SpeedMin
				|| !FMath::IsFinite(Spec.Drag) || Spec.Drag < 0
				// Negative dimensions intentionally mirror the exported feather UV orientation.
				|| Spec.SpriteSize.ContainsNaN() || FMath::IsNearlyZero(Spec.SpriteSize.X) || FMath::IsNearlyZero(Spec.SpriteSize.Y))
			{
				UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Invalid reference emitter specification: %s"), *Spec.Name.ToString());
				return false;
			}
			Names.Add(Spec.Name);
		}
		UNiagaraEmitter* MeshTemplate = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
		UNiagaraEmitter* SpriteTemplate = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst"));
		if (!MeshTemplate || !SpriteTemplate) return false;
		System->Modify();
		TSet<FGuid> Previous;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Previous.Add(Handle.GetId());
		System->RemoveEmitterHandlesById(Previous);
		if (!System->GetSystemSpawnScript()->GetLatestSource()) UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		for (const FReferenceEmitterSpec& Spec : Specs)
		{
			UNiagaraEmitter* Template = Spec.Mesh ? MeshTemplate : SpriteTemplate;
			const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
			FNiagaraEmitterHandle* Handle = System->GetEmitterHandles().FindByPredicate([&](const FNiagaraEmitterHandle& Candidate) { return Candidate.GetId() == Id; });
			if (!Handle) return false;
			Handle->SetName(Spec.Name, *System);
			if (!ConfigureEmitter(*Handle, Spec))
			{
				UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Failed to configure %s / %s. System was not saved."), *System->GetName(), *Spec.Name.ToString());
				return false;
			}
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete(false, false);
		if (!System->IsValid())
		{
			UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Reference Niagara compile failed: %s"), *System->GetPathName());
			return false;
		}
		System->MarkPackageDirty();
		return true;
#else
		return false;
#endif
	}
}
