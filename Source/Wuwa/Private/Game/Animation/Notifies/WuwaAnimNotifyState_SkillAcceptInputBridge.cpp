#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillAcceptInputBridge.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaSkillNotifyContext.h"

void UWuwaAnimNotifyState_SkillAcceptInputBridge::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	// Read before UnrealSharp marshals EventReference: its non-UPROPERTY context is native-only.
	DispatchBegin(MeshComp, Animation, EventReference.GetNotify(), WuwaSkillNotifyContext::GetMontageInstanceId(EventReference));
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::NotifyEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	DispatchEnd(MeshComp, Animation, EventReference.GetNotify(), WuwaSkillNotifyContext::GetMontageInstanceId(EventReference));
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload)
{
	// The engine's default branching-point adapter creates an empty EventReference.
	DispatchBegin(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.NotifyEvent, Payload.MontageInstanceID);
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload)
{
	DispatchEnd(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.NotifyEvent, Payload.MontageInstanceID);
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::DispatchBegin(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation, const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const
{
	AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp);
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	const int32 EventId = WuwaSkillNotifyContext::FindNotifyEventId(Montage, NotifyEvent, this);
	int32 SkillHandle = 0;
	if (EventId != INDEX_NONE && WuwaSkillNotifyContext::ResolveActiveSkill(Character, MeshComp,Montage, MontageInstanceId, SkillHandle))
	{
		BeginSkillWindow(Character->SkillComponent, SkillHandle, MontageInstanceId, EventId);
	}
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::DispatchEnd(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const
{
	AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp);
	const int32 EventId = WuwaSkillNotifyContext::FindNotifyEventId(Cast<UAnimMontage>(Animation), NotifyEvent, this);
	if (Character && MontageInstanceId >= 0 && EventId != INDEX_NONE)
	{
		// The old GA or montage may already have ended. Never replace the event's old
		// playback identity with whichever GA happens to be animating the character now.
		EndSkillWindow(Character->SkillComponent, MontageInstanceId, EventId);
	}
}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::BeginSkillWindow_Implementation(
	UWuwaSkillBridgeComponent*, int32, int32, int32) const {}

void UWuwaAnimNotifyState_SkillAcceptInputBridge::EndSkillWindow_Implementation(
	UWuwaSkillBridgeComponent*, int32, int32) const {}
