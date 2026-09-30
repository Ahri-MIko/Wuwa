#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_SlashFx.generated.h"

class UWuwaSlashFxPreset;

/** Places a one-shot effect group at an authored animation moment. */
UCLASS(meta=(DisplayName="Wuwa Slash FX"))
class WUWA_API UWuwaAnimNotify_SlashFx : public UAnimNotify
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") TObjectPtr<UWuwaSlashFxPreset> Preset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX", meta=(AnimNotifyBoneName="true")) FName SocketName = TEXT("Root");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") FVector LocationOffset = FVector(0,0,90);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") FRotator RotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slash FX") FVector Scale = FVector::OneVector;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("刀光 / Slash FX"); }
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;
private:
	void Play(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Animation, int32 MontageInstanceId) const;
};
