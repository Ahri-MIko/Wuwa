#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_SkillAcceptInputBridge.generated.h"

class UWuwaSkillBridgeComponent;

/** 保留原生通知的播放实例身份，再交给 C# 管理技能窗口。共享通知对象不保存运行状态。 */
UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaAnimNotifyState_SkillAcceptInputBridge : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload) override;

	/** 已验证本次播放属于当前主技能；编号来自原始通知上下文，不由脚本反推。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill", meta = (BlueprintProtected))
	void BeginSkillWindow(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle,
		int32 MontageInstanceId, int32 NotifyEventId) const;

	/** 不要求旧 GA 仍在播放；脚本按播放实例与通知编号关闭先前登记的窗口。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill", meta = (BlueprintProtected))
	void EndSkillWindow(UWuwaSkillBridgeComponent* Skills, int32 MontageInstanceId, int32 NotifyEventId) const;

private:
	void DispatchBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const;
	void DispatchEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const;
};
