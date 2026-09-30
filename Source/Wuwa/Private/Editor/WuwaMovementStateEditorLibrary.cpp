#include "Editor/WuwaMovementStateEditorLibrary.h"

#if WITH_EDITOR
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimationStateMachineGraph.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "Subsystems/EditorAssetSubsystem.h"

namespace WuwaGroundStateMigration
{
constexpr TCHAR BlueprintPath[] = TEXT("/Game/Characters/Role/changli/AnimationBluePrint/ABP_Changli");
constexpr TCHAR DashBlueprintPath[] = TEXT("/Game/Characters/Role/changli/GA/BP_Ability_Dash");
constexpr TCHAR AnimationDirectory[] = TEXT("/Game/Characters/Role/changli/CommonAnim/");
constexpr TCHAR MarkerPrefix[] = TEXT("Wuwa ground state migration: ");

template <typename T>
T* FindByGuid(UEdGraph* Graph, const TCHAR* Text)
{
	FGuid Guid;
	if (!Graph || !FGuid::Parse(Text, Guid))
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && Node->NodeGuid == Guid)
		{
			return Cast<T>(Node);
		}
	}
	return nullptr;
}

template <typename T>
T* FindNode(UEdGraph* Graph)
{
	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (T* Typed = Cast<T>(Node))
			{
				return Typed;
			}
		}
	}
	return nullptr;
}

UAnimStateNode* FindState(UEdGraph* Graph, const TCHAR* StateName)
{
	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimStateNode* State = Cast<UAnimStateNode>(Node);
			if (State && State->GetStateName() == StateName)
			{
				return State;
			}
		}
	}
	return nullptr;
}

UAnimStateTransitionNode* FindTransition(UAnimStateNode* From, UAnimStateNode* To)
{
	if (From && To)
	{
		for (UEdGraphNode* Node : From->GetGraph()->Nodes)
		{
			UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node);
			if (Transition && Transition->GetPreviousState() == From && Transition->GetNextState() == To)
			{
				return Transition;
			}
		}
	}
	return nullptr;
}

struct FDashTaskExit
{
	UEdGraphPin* Callback = nullptr;
	UEdGraphPin* EndAbilityInput = nullptr;
};

/** Validate the two audited montage tasks before modifying either Blueprint. */
bool PreflightDashTaskExits(UBlueprint* Blueprint, TArray<FDashTaskExit>& Exits, FString& Error)
{
	const UFunction* Factory = UAbilityTask_PlayMontageAndWait::StaticClass()->FindFunctionByName(TEXT("CreatePlayMontageAndWaitProxy"));
	const UFunction* EndAbilityFunction = UGameplayAbility::StaticClass()->FindFunctionByName(TEXT("K2_EndAbility"));
	for (const TCHAR* Guid : { TEXT("61A92CB5407741BFA8D540A72844BA86"), TEXT("9FA477314A0A29803FD120AEDAC76512") })
	{
		UEdGraphNode* TaskNode = nullptr;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (UEdGraphNode* Found = FindByGuid<UEdGraphNode>(Graph, Guid))
			{
				if (TaskNode)
				{
					Error = FString::Printf(TEXT("Dash montage task GUID %s is ambiguous."), Guid);
					return false;
				}
				TaskNode = Found;
			}
		}
		UK2Node_BaseAsyncTask* Task = Cast<UK2Node_BaseAsyncTask>(TaskNode);
		UEdGraphPin* Proxy = Task ? Task->FindPin(TEXT("AsyncTaskProxy")) : nullptr;
		if (!Task || Task->GetClass()->GetPathName() != TEXT("/Script/GameplayAbilitiesEditor.K2Node_LatentAbilityCall")
			|| !Factory || Task->GetFactoryFunction() != Factory || !Proxy
			|| Proxy->Direction != EGPD_Output || Proxy->PinType.PinSubCategoryObject != UAbilityTask_PlayMontageAndWait::StaticClass())
		{
			Error = FString::Printf(TEXT("Dash node %s is not the expected PlayMontageAndWait ability task."), Guid);
			return false;
		}
		UEdGraphPin* Completed = Task->FindPin(TEXT("OnCompleted"));
		UEdGraphPin* EndInput = Completed && Completed->LinkedTo.Num() == 1 ? Completed->LinkedTo[0] : nullptr;
		UK2Node_CallFunction* EndCall = EndInput ? Cast<UK2Node_CallFunction>(EndInput->GetOwningNode()) : nullptr;
		UEdGraphPin* EndSelf = EndCall ? EndCall->FindPin(UEdGraphSchema_K2::PN_Self) : nullptr;
		if (!Completed || Completed->Direction != EGPD_Output || Completed->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			|| !EndInput || EndInput->Direction != EGPD_Input || EndInput->PinName != UEdGraphSchema_K2::PN_Execute
			|| EndInput->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec || !EndCall
			|| EndCall->GetGraph() != Task->GetGraph() || !EndAbilityFunction || EndCall->GetTargetFunction() != EndAbilityFunction
			|| !EndCall->FunctionReference.IsSelfContext() || (EndSelf && (!EndSelf->LinkedTo.IsEmpty() || EndSelf->DefaultObject)))
		{
			Error = FString::Printf(TEXT("Dash task %s OnCompleted must directly call this ability's K2_EndAbility before its empty failure callbacks can be migrated."), Guid);
			return false;
		}
		for (const FName Name : { FName(TEXT("OnInterrupted")), FName(TEXT("OnCancelled")) })
		{
			UEdGraphPin* Callback = Task->FindPin(Name);
			if (!Callback || Callback->Direction != EGPD_Output || Callback->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				Error = FString::Printf(TEXT("Dash task %s is missing its %s execution output."), Guid, *Name.ToString());
				return false;
			}
			// Only an additive connection is acceptable: existing completion/user links must survive.
			if (Callback->LinkedTo.IsEmpty()
				&& Task->GetGraph()->GetSchema()->CanCreateConnection(Callback, EndInput).Response != CONNECT_RESPONSE_MAKE)
			{
				Error = FString::Printf(TEXT("Dash task %s %s cannot connect to EndAbility without replacing existing links."), Guid, *Name.ToString());
				return false;
			}
			Exits.Add({ Callback, EndInput });
		}
	}
	return true;
}

/** Only nodes bearing our marker are reused; user nodes are retained, including disconnected drafts. */
struct FMigration
{
	TArray<FString> Errors;
	int32 AddedNodes = 0;
	int32 ChangedConnections = 0;
	int32 AddedDashExits = 0;
	int32 ExistingDashExits = 0;
	int32 PreservedDashExits = 0;

	void ConnectDashTaskExits(UBlueprint* Blueprint, const TArray<FDashTaskExit>& Exits)
	{
		for (const FDashTaskExit& Exit : Exits)
		{
			if (!Exit.Callback->LinkedTo.IsEmpty())
			{
				if (Exit.Callback->LinkedTo.Num() == 1 && Exit.Callback->LinkedTo[0] == Exit.EndAbilityInput) { ++ExistingDashExits; }
				else { ++PreservedDashExits; }
				continue;
			}
			Blueprint->Modify();
			Exit.Callback->GetOwningNode()->GetGraph()->Modify();
			if (Connect(Exit.Callback, Exit.EndAbilityInput)) { ++AddedDashExits; }
		}
		if (AddedDashExits > 0) { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint); }
	}

	template <typename T, typename Configure>
	T* EnsureNode(UEdGraph* Graph, const TCHAR* Role, int32 X, int32 Y, Configure ConfigureNew)
	{
		const FString Marker = FString(MarkerPrefix) + Role;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeComment == Marker)
			{
				T* Typed = Cast<T>(Node);
				if (!Typed)
				{
					Errors.Add(FString::Printf(TEXT("Unexpected node type for %s in %s"), Role, *Graph->GetPathName()));
				}
				return Typed;
			}
		}
		Graph->Modify();
		FGraphNodeCreator<T> Creator(*Graph);
		T* Node = Creator.CreateNode(false);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Node->NodeComment = Marker;
		ConfigureNew(Node);
		Creator.Finalize();
		++AddedNodes;
		return Node;
	}

	bool Connect(UEdGraphPin* Output, UEdGraphPin* Input)
	{
		if (!Output || !Input || Output->Direction != EGPD_Output || Input->Direction != EGPD_Input)
		{
			Errors.Add(TEXT("A required source/output or destination/input pin is missing."));
			return false;
		}
		if (Input->LinkedTo.Num() == 1 && Input->LinkedTo[0] == Output)
		{
			return true;
		}
		const UEdGraphSchema* Schema = Input->GetOwningNode()->GetGraph()->GetSchema();
		const FPinConnectionResponse Response = Schema->CanCreateConnection(Output, Input);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			Errors.Add(FString::Printf(TEXT("Cannot connect %s.%s -> %s.%s: %s"),
				*Output->GetOwningNode()->GetName(), *Output->PinName.ToString(),
				*Input->GetOwningNode()->GetName(), *Input->PinName.ToString(), *Response.Message.ToString()));
			return false;
		}
		Input->GetOwningNode()->Modify();
		Output->GetOwningNode()->Modify();
		if (!Schema->TryCreateConnection(Output, Input))
		{
			Errors.Add(FString::Printf(TEXT("Schema rejected connection to %s.%s"),
				*Input->GetOwningNode()->GetPathName(), *Input->PinName.ToString()));
			return false;
		}
		++ChangedConnections;
		return true;
	}

	UK2Node_BreakStruct* ReadSnapshot(UEdGraph* Graph, int32 X = -640, int32 Y = 0)
	{
		UK2Node_VariableGet* Get = EnsureNode<UK2Node_VariableGet>(Graph, TEXT("Read LocomotionData"), X, Y,
			[](UK2Node_VariableGet* Node) { Node->VariableReference.SetSelfMember(TEXT("LocomotionData")); });
		UK2Node_BreakStruct* Break = EnsureNode<UK2Node_BreakStruct>(Graph, TEXT("Resolved movement snapshot"), X + 220, Y,
			[](UK2Node_BreakStruct* Node)
			{
				Node->StructType = FWuwaLocomotionAnimData::StaticStruct();
				Node->bMadeAfterOverridePinRemoval = true;
			});
		if (!Get || !Break)
		{
			return nullptr;
		}
		bool bNeedsReconstruct = false;
		for (FOptionalPinFromProperty& Pin : Break->ShowPinForProperties)
		{
			const bool bRequired = Pin.PropertyName == TEXT("bGroundMoveActive")
				|| Pin.PropertyName == TEXT("bWantsToStop") || Pin.PropertyName == TEXT("bStateGroundSprint")
				|| Pin.PropertyName == TEXT("StopGait");
			if (bRequired && !Pin.bShowPin)
			{
				Break->Modify();
				Pin.bShowPin = true;
				bNeedsReconstruct = true;
			}
		}
		if (bNeedsReconstruct)
		{
			Break->ReconstructNode();
		}
		UEdGraphPin* StructInput = nullptr;
		for (UEdGraphPin* Pin : Break->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinSubCategoryObject == FWuwaLocomotionAnimData::StaticStruct())
			{
				StructInput = Pin;
				break;
			}
		}
		Connect(Get->FindPin(TEXT("LocomotionData")), StructInput);
		return Break;
	}

	void SetTransition(UAnimStateTransitionNode* Transition, FName Field)
	{
		UEdGraph* Graph = Transition->GetBoundGraph();
		UAnimGraphNode_TransitionResult* Result = FindNode<UAnimGraphNode_TransitionResult>(Graph);
		UK2Node_BreakStruct* Break = ReadSnapshot(Graph);
		Connect(Break ? Break->FindPin(Field) : nullptr, Result ? Result->FindPin(TEXT("bCanEnterTransition")) : nullptr);
	}

	UK2Node_CallFunction* BooleanFunction(UEdGraph* Graph, const TCHAR* Role, const TCHAR* Function, int32 X, int32 Y)
	{
		return EnsureNode<UK2Node_CallFunction>(Graph, Role, X, Y, [Function](UK2Node_CallFunction* Node)
		{
			Node->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Function));
		});
	}

	void SetDirectIdleRule(UAnimStateTransitionNode* Transition)
	{
		UEdGraph* Graph = Transition->GetBoundGraph();
		UK2Node_BreakStruct* Break = ReadSnapshot(Graph, -1000, 0);
		UK2Node_CallFunction* NotMoving = BooleanFunction(Graph, TEXT("Not moving"), TEXT("Not_PreBool"), -350, 0);
		UK2Node_CallFunction* NotStopping = BooleanFunction(Graph, TEXT("Not ordinary stop"), TEXT("Not_PreBool"), -350, 160);
		UK2Node_CallFunction* Both = BooleanFunction(Graph, TEXT("Direct idle rule"), TEXT("BooleanAND"), -100, 0);
		UAnimGraphNode_TransitionResult* Result = FindNode<UAnimGraphNode_TransitionResult>(Graph);
		if (!Break || !NotMoving || !NotStopping || !Both || !Result)
		{
			Errors.Add(TEXT("Failed to create the Move -> Idle rule nodes."));
			return;
		}
		Connect(Break->FindPin(TEXT("bGroundMoveActive")), NotMoving->FindPin(TEXT("A")));
		Connect(Break->FindPin(TEXT("bWantsToStop")), NotStopping->FindPin(TEXT("A")));
		Connect(NotMoving->FindPin(TEXT("ReturnValue")), Both->FindPin(TEXT("A")));
		Connect(NotStopping->FindPin(TEXT("ReturnValue")), Both->FindPin(TEXT("B")));
		Connect(Both->FindPin(TEXT("ReturnValue")), Result->FindPin(TEXT("bCanEnterTransition")));
	}

	void ConfigureStops(UEdGraph* Graph, UAnimGraphNode_SequencePlayer* OriginalRun, UAnimSequence* Walk, UAnimSequence* Sprint)
	{
		UK2Node_BreakStruct* Break = ReadSnapshot(Graph, -1300, 700);
		UAnimGraphNode_StateResult* Result = FindNode<UAnimGraphNode_StateResult>(Graph);
		auto MakePlayer = [this, Graph, OriginalRun](const TCHAR* Role, UAnimSequence* Sequence, int32 Y)
		{
			UAnimGraphNode_SequencePlayer* Player = EnsureNode<UAnimGraphNode_SequencePlayer>(Graph, Role, -500, Y,
				[OriginalRun, Sequence](UAnimGraphNode_SequencePlayer* Node)
				{
					// Keep the user's existing timing/sync settings; replace only the clip.
					Node->Node = OriginalRun->Node;
					Node->SetAnimationAsset(Sequence);
					Node->Node.SetLoopAnimation(false);
				});
			if (!Player)
			{
				return Player;
			}
			// Preserve the existing StepProgress source instead of inventing a new phase calculation.
			UEdGraphPin* OriginalStart = OriginalRun->FindPin(TEXT("StartPosition"));
			UEdGraphPin* NewStart = Player->FindPin(TEXT("StartPosition"));
			if (OriginalStart && OriginalStart->LinkedTo.Num() == 1)
			{
				if (!NewStart)
				{
					for (FOptionalPinFromProperty& Pin : Player->ShowPinForProperties)
					{
						if (Pin.PropertyName == TEXT("StartPosition")) { Pin.bShowPin = true; }
					}
					Player->ReconstructNode();
					NewStart = Player->FindPin(TEXT("StartPosition"));
				}
				Connect(OriginalStart->LinkedTo[0], NewStart);
			}
			return Player;
		};
		UAnimGraphNode_SequencePlayer* WalkPlayer = MakePlayer(TEXT("Walk stop left"), Walk, 500);
		UAnimGraphNode_SequencePlayer* SprintPlayer = MakePlayer(TEXT("Sprint stop left"), Sprint, 900);
		auto MakeBlend = [this, Graph](const TCHAR* Role, int32 X)
		{
			UAnimGraphNode_BlendListByBool* Blend = EnsureNode<UAnimGraphNode_BlendListByBool>(Graph, Role, X, 500,
				[](UAnimGraphNode_BlendListByBool*) {});
			if (Blend)
			{
				for (const FName PinName : { FName(TEXT("BlendTime_0")), FName(TEXT("BlendTime_1")) })
				{
					if (UEdGraphPin* Pin = Blend->FindPin(PinName))
					{
						Graph->GetSchema()->TrySetDefaultValue(*Pin, TEXT("0.0"));
					}
					else { Errors.Add(TEXT("A gait stop blend time pin is missing.")); }
				}
			}
			return Blend;
		};
		UAnimGraphNode_BlendListByBool* WalkOrRun = MakeBlend(TEXT("Walk or run stop"), -150);
		UAnimGraphNode_BlendListByBool* FinalStop = MakeBlend(TEXT("Sprint or ordinary stop"), 150);
		auto MakeEquality = [this, Graph, Break](const TCHAR* Role, const TCHAR* EnumValue, int32 Y)
		{
			UK2Node_EnumEquality* Equal = EnsureNode<UK2Node_EnumEquality>(Graph, Role, -600, Y, [](UK2Node_EnumEquality*) {});
			if (Equal && Break && Connect(Break->FindPin(TEXT("StopGait")), Equal->GetInput1Pin()))
			{
				Graph->GetSchema()->TrySetDefaultValue(*Equal->GetInput2Pin(), EnumValue);
			}
			return Equal;
		};
		UK2Node_EnumEquality* IsWalk = MakeEquality(TEXT("Stopped gait is Walk"), TEXT("Walk"), 700);
		UK2Node_EnumEquality* IsSprint = MakeEquality(TEXT("Stopped gait is Sprint"), TEXT("Sprint"), 1100);
		if (!WalkPlayer || !SprintPlayer || !WalkOrRun || !FinalStop || !IsWalk || !IsSprint || !Result)
		{
			Errors.Add(TEXT("Failed to create the gait-specific stop nodes."));
			return;
		}
		Connect(WalkPlayer->FindPin(TEXT("Pose")), WalkOrRun->FindPin(TEXT("BlendPose_0")));
		Connect(OriginalRun->FindPin(TEXT("Pose")), WalkOrRun->FindPin(TEXT("BlendPose_1")));
		Connect(IsWalk->GetReturnValuePin(), WalkOrRun->FindPin(TEXT("bActiveValue")));
		Connect(SprintPlayer->FindPin(TEXT("Pose")), FinalStop->FindPin(TEXT("BlendPose_0")));
		Connect(WalkOrRun->FindPin(TEXT("Pose")), FinalStop->FindPin(TEXT("BlendPose_1")));
		Connect(IsSprint->GetReturnValuePin(), FinalStop->FindPin(TEXT("bActiveValue")));
		Connect(FinalStop->FindPin(TEXT("Pose")), Result->FindPin(TEXT("Result")));
	}
};
}
#endif

FString UWuwaMovementStateEditorLibrary::MigrateGroundStateAssets(bool bSave)
{
#if WITH_EDITOR
	using namespace WuwaGroundStateMigration;
	if (GEditor && GEditor->PlayWorld)
	{
		return TEXT("ERROR: Stop PIE before migrating animation graphs. No assets saved.");
	}
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, BlueprintPath);
	if (!Blueprint)
	{
		return TEXT("ERROR: ABP_Changli could not be loaded. No assets saved.");
	}
	UBlueprint* DashBlueprint = LoadObject<UBlueprint>(nullptr, DashBlueprintPath);
	UWuwaGameplayAbilityBase* DashDefaults = DashBlueprint && DashBlueprint->GeneratedClass
		? Cast<UWuwaGameplayAbilityBase>(DashBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!DashDefaults)
	{
		return TEXT("ERROR: BP_Ability_Dash must derive from UWuwaGameplayAbilityBase. No assets saved.");
	}
	const EGameplayAbilityInstancingPolicy::Type DashInstancing = DashDefaults->GetInstancingPolicy();
	if (DashInstancing != EGameplayAbilityInstancingPolicy::InstancedPerActor
		&& DashInstancing != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		return TEXT("ERROR: BP_Ability_Dash must use InstancedPerActor or InstancedPerExecution to own a movement-state override. Its existing instancing policy was not changed. No assets saved.");
	}
	const int32 DashPriority = FMath::Max(100, DashDefaults->ActionMoveStatePriority);
	const bool bDashDefaultsChanged = !DashDefaults->bOverridesMoveState
		|| DashDefaults->ActionMoveState != EWuwaMoveState::Dodge
		|| DashDefaults->ActionMoveStatePriority != DashPriority;
	for (const FName Required : { FName(TEXT("bGroundMoveActive")), FName(TEXT("bWantsToStop")),
		FName(TEXT("bStateGroundSprint")), FName(TEXT("StopGait")) })
	{
		if (!FWuwaLocomotionAnimData::StaticStruct()->FindPropertyByName(Required))
		{
			return FString::Printf(TEXT("ERROR: Native snapshot field %s is missing; build the movement state code first. No assets saved."), *Required.ToString());
		}
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UEdGraph* GroundGraph = nullptr;
	UEdGraph* MoveGroundGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetFName() == TEXT("SM_Ground")) { GroundGraph = Graph; }
		if (Graph->GetFName() == TEXT("Stand_Loop_State")) { MoveGroundGraph = Graph; }
	}
	UAnimStateNode* Idle = FindState(GroundGraph, TEXT("Idle"));
	UAnimStateNode* Move = FindState(GroundGraph, TEXT("Move"));
	UAnimStateNode* Stop = FindState(GroundGraph, TEXT("Stop"));
	UAnimStateTransitionNode* IdleToMove = FindTransition(Idle, Move);
	UAnimStateTransitionNode* MoveToStop = FindTransition(Move, Stop);
	UAnimStateTransitionNode* StopToMove = FindTransition(Stop, Move);
	UAnimStateTransitionNode* StopToIdle = FindTransition(Stop, Idle);
	UAnimGraphNode_BlendListByBool* SprintBlend = FindByGuid<UAnimGraphNode_BlendListByBool>(MoveGroundGraph, TEXT("9DAD14E6414B2A13C6F201802E8A7B6B"));
	UEdGraph* StopGraph = Stop ? Stop->GetBoundGraph() : nullptr;
	UAnimGraphNode_SequencePlayer* RunStop = FindByGuid<UAnimGraphNode_SequencePlayer>(StopGraph, TEXT("6D0FB1CD4BB40FA4428B9BB7CD9BDFCB"));
	UAnimSequence* WalkStop = LoadObject<UAnimSequence>(nullptr, *(FString(AnimationDirectory) + TEXT("ChangliR2T1ChangLiMd10011_Stop_Walk_L")));
	UAnimSequence* SprintStop = LoadObject<UAnimSequence>(nullptr, *(FString(AnimationDirectory) + TEXT("ChangliR2T1ChangLiMd10011_Stop_Sprint_L")));
	if (!IdleToMove || !MoveToStop || !StopToMove || !StopToIdle || !SprintBlend || !RunStop || !WalkStop || !SprintStop)
	{
		return TEXT("ERROR: Expected SM_Ground states/transitions, Sprint blend, original left Run stop, or Walk/Sprint stop clips are missing. The graph may have changed since the audit; inspect it before retrying. No assets saved.");
	}
	if (WalkStop->GetSkeleton() != Blueprint->TargetSkeleton || SprintStop->GetSkeleton() != Blueprint->TargetSkeleton)
	{
		return TEXT("ERROR: Stop clips use a different skeleton. No assets saved.");
	}
	TArray<FDashTaskExit> DashTaskExits;
	FString DashTaskError;
	if (!PreflightDashTaskExits(DashBlueprint, DashTaskExits, DashTaskError))
	{
		return TEXT("ERROR: ") + DashTaskError + TEXT(" No assets saved.");
	}
	Blueprint->Modify();
	FMigration Migration;
	UK2Node_BreakStruct* MoveSnapshot = Migration.ReadSnapshot(MoveGroundGraph, -1000, 700);
	Migration.Connect(MoveSnapshot ? MoveSnapshot->FindPin(TEXT("bStateGroundSprint")) : nullptr,
		SprintBlend->FindPin(TEXT("bActiveValue")));
	Migration.SetTransition(IdleToMove, TEXT("bGroundMoveActive"));
	Migration.SetTransition(StopToMove, TEXT("bGroundMoveActive"));
	Migration.SetTransition(MoveToStop, TEXT("bWantsToStop"));
	UAnimStateTransitionNode* MoveToIdle = FindTransition(Move, Idle);
	if (!MoveToIdle)
	{
		MoveToIdle = Migration.EnsureNode<UAnimStateTransitionNode>(GroundGraph, TEXT("Move directly to Idle"),
			(Move->NodePosX + Idle->NodePosX) / 2, Move->NodePosY - 120, [MoveToStop](UAnimStateTransitionNode* Node)
			{
				Node->CrossfadeDuration = 0.1f;
				Node->PriorityOrder = MoveToStop->PriorityOrder + 1;
			});
		if (MoveToIdle) { MoveToIdle->CreateConnections(Move, Idle); }
	}
	if (MoveToIdle) { Migration.SetDirectIdleRule(MoveToIdle); }
	else { Migration.Errors.Add(TEXT("Failed to create Move -> Idle transition.")); }
	Migration.ConfigureStops(StopGraph, RunStop, WalkStop, SprintStop);
	Migration.ConnectDashTaskExits(DashBlueprint, DashTaskExits);
	if (!Migration.Errors.IsEmpty())
	{
		return TEXT("ERROR: Migration incomplete; in-memory edits were not saved.\n") + FString::Join(Migration.Errors, TEXT("\n"));
	}
	if (bDashDefaultsChanged)
	{
		DashBlueprint->Modify();
		DashDefaults->Modify();
		DashDefaults->bOverridesMoveState = true;
		DashDefaults->ActionMoveState = EWuwaMoveState::Dodge;
		DashDefaults->ActionMoveStatePriority = DashPriority;
		FBlueprintEditorUtils::MarkBlueprintAsModified(DashBlueprint);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	// Compile every affected asset first. A failure in either blueprint must never save the other.
	for (UBlueprint* Asset : { static_cast<UBlueprint*>(Blueprint), DashBlueprint })
	{
		FCompilerResultsLog CompileLog;
		FKismetEditorUtilities::CompileBlueprint(Asset, EBlueprintCompileOptions::None, &CompileLog);
		if (CompileLog.NumErrors > 0 || Asset->Status == BS_Error)
		{
			TArray<FString> Messages;
			for (const TSharedRef<FTokenizedMessage>& Message : CompileLog.Messages)
			{
				if (Message->GetSeverity() == EMessageSeverity::Error) { Messages.Add(Message->ToText().ToString()); }
			}
			return FString::Printf(TEXT("ERROR: %s compilation failed. No assets saved.\n%s"),
				*Asset->GetName(), *FString::Join(Messages, TEXT("\n")));
		}
	}
	// Blueprint compilation may replace its generated class and CDO. Inspect the replacement.
	DashDefaults = DashBlueprint->GeneratedClass
		? Cast<UWuwaGameplayAbilityBase>(DashBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!DashDefaults || !DashDefaults->bOverridesMoveState || DashDefaults->ActionMoveState != EWuwaMoveState::Dodge
		|| DashDefaults->ActionMoveStatePriority != DashPriority)
	{
		return TEXT("ERROR: Compiled Dash defaults did not retain Dodge override settings. No assets saved.");
	}
	if (bSave)
	{
		UEditorAssetSubsystem* Assets = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
		if (!Assets)
		{
			return TEXT("ERROR: Both Blueprints compiled, but the editor asset subsystem is unavailable. No assets saved.");
		}
		if (!Assets->SaveLoadedAsset(Blueprint, false))
		{
			return TEXT("ERROR: Both Blueprints compiled, but ABP_Changli could not be saved; Dash was not saved.");
		}
		if (!Assets->SaveLoadedAsset(DashBlueprint, false))
		{
			return TEXT("ERROR: ABP_Changli saved, but BP_Ability_Dash could not be saved. Both compiled successfully; retry saving Dash or restore the caller's asset backups.");
		}
	}
	return FString::Printf(TEXT("OK: ABP_Changli and BP_Ability_Dash compiled; %s. Added %d nodes and changed %d connections. Dash overrides movement with Dodge at priority %d (%s), retaining its instancing policy. Dash interruption/cancellation exits: %d connected to EndAbility, %d already connected, %d existing user connections preserved. Stop selection uses stable StopGait and the existing left-foot/StepProgress convention; user right-foot drafts were retained. Existing idle, IK, physics, root-motion and Slot settings were preserved."),
		bSave ? TEXT("both saved") : TEXT("memory only, not saved"), Migration.AddedNodes, Migration.ChangedConnections,
		DashPriority, bDashDefaultsChanged ? TEXT("updated defaults") : TEXT("defaults already configured"),
		Migration.AddedDashExits, Migration.ExistingDashExits, Migration.PreservedDashExits);
#else
	return TEXT("ERROR: MigrateGroundStateAssets is supported only in editor builds.");
#endif
}
