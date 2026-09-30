#include "Game/Animation/Notifies/WuwaAnimNotify_SlashFx.h"

#include "WuwaSkillNotifyContext.h"
#include "Game/Render/Effect/WuwaSlashFxComponent.h"
#include "Game/Render/Effect/WuwaSlashFxPreset.h"

void UWuwaAnimNotify_SlashFx::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Play(MeshComp, Animation, WuwaSkillNotifyContext::GetMontageInstanceId(EventReference));
}

void UWuwaAnimNotify_SlashFx::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
    Play(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.MontageInstanceID);
}

void UWuwaAnimNotify_SlashFx::Play(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 MontageInstanceId) const
{
    if (!IsValid(Mesh) || !Mesh->GetWorld() || !IsValid(Mesh->GetOwner()) || !IsValid(Preset)) return;
    UWuwaGameplayAbilityBase* Ability = nullptr;
    int32 SkillHandle = 0;
    if (Mesh->GetWorld()->IsGameWorld())
    {
        AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(Mesh);
        if (!WuwaSkillNotifyContext::ResolveActiveSkill(Character, Mesh, Cast<UAnimMontage>(Animation), MontageInstanceId, SkillHandle)) return;
        Ability = Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get();
    }

    UWuwaSlashFxComponent* Effects = Mesh->GetOwner()->FindComponentByClass<UWuwaSlashFxComponent>();
    if (!Effects)
    {
        Effects = NewObject<UWuwaSlashFxComponent>(Mesh->GetOwner());
        Mesh->GetOwner()->AddInstanceComponent(Effects);
        Effects->RegisterComponent();
    }
    const FTransform SocketTransform = Mesh->GetSocketTransform(SocketName);
    const FTransform Placement = FTransform(RotationOffset, LocationOffset, Scale) * SocketTransform;
    Effects->PlayEffect(Preset, Placement, Ability, SkillHandle);
}
