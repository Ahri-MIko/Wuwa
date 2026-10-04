#include "Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"

FString UWuwaAnimNotify_PlayEffect::GetNotifyName_Implementation() const
{
	return Model ? FString::Printf(TEXT("Effect: %s"), *Model->GetName()) : TEXT("Play Effect");
}

void UWuwaAnimNotify_PlayEffect::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
	if (!MeshComp || !Model) return;

	// 首版仅在游戏世界创建系统，动画编辑器预览不发起播放。
	UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(MeshComp);
	if (!Effects) return;

	FWuwaEffectSpawnRequest Request;
	Request.Model = Model;
	const FTransform Offset(RotationOffset, LocationOffset);
	if (bFollow)
	{
		Request.AttachTo = MeshComp;
		Request.SocketName = SocketName;
		Request.Transform = Offset;
	}
	else
	{
		Request.Transform = Offset * MeshComp->GetSocketTransform(SocketName);
	}

	// 此通知用于短表现：自然播完后由系统回收，本通知不持有播放句柄。
	Effects->SpawnEffect(Request);
}
