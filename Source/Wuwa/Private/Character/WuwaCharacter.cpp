// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/WuwaCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include"WuwaASC/WuwaAbilitySystemComponent.h"
#include"WuwaASC/WuwaAttributeSet.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Character/CharacterMovementComponent/WuwaMovementComponent.h"
#include "Input/WuwaInputComponent.h"          // ← 新增：需要 UWuwaInputComponent 完整定义
#include "InputActionValue.h"            // ← 新增：需要 FInputActionValue
#include "Character/WuwaMoveInputHandler.h"
#include "GameFramework/Controller.h"    // ← 新增：需要 Controller
#include "Cores/WuwaPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h" 
#include "PlayerController/WuwaPlayerController.h"
#include "Tools/DebugHelper.h"
#include "UI/WuwaHUD.h"
#include "UI/WuwaWidgetController.h"
#include "WuwaASC/WuwaGameplayAbilityBase.h"


class UWuwaWidgetController;

#pragma region Initializer

// Sets default values
AWuwaCharacter::AWuwaCharacter(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer.SetDefaultSubobjectClass<UWuwaMovementComponent>(
        ACharacter::CharacterMovementComponentName))
{

    WuwaMovementComponent = Cast<UWuwaMovementComponent>(GetCharacterMovement());//重置MovementComponent组件

    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;


    //实例化输入组件
    WuwaInputComponent = CreateDefaultSubobject<UWuwaInputComponent>(TEXT("WuwaInputComponent"));

    //Camera And Spring Settings
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = CameraArmLength;          // 初始距离
    CameraBoom->bUsePawnControlRotation = true;   // 弹簧臂跟随控制器旋转

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false; // 摄像机自身不再额外旋转,交给弹簧臂

}

#pragma endregion

#pragma region BeginPlay

// Called when the game starts or when spawned
void AWuwaCharacter::BeginPlay()
{
    Super::BeginPlay();
    const AWuwaPlayerController* PC =Cast<AWuwaPlayerController>( GetController());
    UWuwaMoveInputHandler* MovementInputHandler = PC ? PC->MoveInputHandler.Get() : nullptr;
    if (MovementInputHandler)
    {
        MovementInputHandler->OnMove.AddDynamic(this, &AWuwaCharacter::HandleMoveInput);
        MovementInputHandler->OnLook.AddDynamic(this, &AWuwaCharacter::HandleLook);
    }
}
#pragma endregion

#pragma region Update


// Called every frame
void AWuwaCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}


#pragma endregion

#pragma region Logical Functions


UAbilitySystemComponent* AWuwaCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}



#pragma endregion

#pragma region ProcessInput

void AWuwaCharacter::HandleMoveInput(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();
    MoveInputDir = Vector2ToCameraDirNormalized(MoveInput, FollowCamera);
    Move(Value);
}


void AWuwaCharacter::HandleLook(const FInputActionValue& Value)
{
    MouseMoveVec = Value.Get<FVector2D>();
    AddControllerYawInput(MouseMoveVec.X);
    AddControllerPitchInput(MouseMoveVec.Y);
}

void AWuwaCharacter::Move(const FInputActionValue& Value)
{
    if (!WuwaMovementComponent) return;

    if (WuwaMovementComponent->IsClimbing())
    {
        HabdleClimbInput(Value);
    }
    else
    {
        MoveInputDir = Vector2ToCameraDirNormalized(MoveInput, FollowCamera);
        AddMovementInput(FVector(MoveInputDir.X, MoveInputDir.Y, 0.f), 1.f);
    }

}


void AWuwaCharacter::HabdleClimbInput(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    const FVector ForwardDirection = FVector::CrossProduct(
        -WuwaMovementComponent->ProcessedSurfaceNomal,
        GetActorRightVector()
    );
    const FVector RightDirection = FVector::CrossProduct(
        -WuwaMovementComponent->ProcessedSurfaceNomal,
        -GetActorUpVector()
    );

    AddMovementInput(ForwardDirection, MovementVector.Y);
    AddMovementInput(RightDirection, MovementVector.X);
}


FVector2D AWuwaCharacter::Vector2ToCameraDirNormalized(const FVector2D InSource2D, const UCameraComponent* InCameraComp) const
{
    if (!InCameraComp)
    {
        return FVector2D::ZeroVector;
    }

    const FRotator CameraRot = InCameraComp->GetComponentRotation();
    const FRotator YawOnly(0.f, CameraRot.Yaw, 0.f);

    const FVector Forward = UKismetMathLibrary::GetForwardVector(YawOnly);
    const FVector Right = UKismetMathLibrary::GetRightVector(YawOnly);

    const FVector Dir = Forward * InSource2D.Y + Right * InSource2D.X;

    return FVector2D(Dir.X, Dir.Y).GetSafeNormal();
}

#pragma endregion

#pragma region Replicate
//初始化ASCInfo用的
void AWuwaCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitGasInfoandHUD();
    InitInitialAbilities();
}

void AWuwaCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitGasInfoandHUD();
}

void AWuwaCharacter::InitGasInfoandHUD()
{
    AWuwaPlayerState* WuwaPlayerState = GetPlayerState<AWuwaPlayerState>();
    check(WuwaPlayerState);
    WuwaPlayerState->GetAbilitySystemComponent()->InitAbilityActorInfo(WuwaPlayerState, this);
    AbilitySystemComponent = Cast<UWuwaAbilitySystemComponent>(WuwaPlayerState->GetAbilitySystemComponent());
    AttributeSet = WuwaPlayerState->GetAttributeSet();
    AbilitySystemComponent->InitAbilitySystemCompoent();
    // 只有本地玩家才有 HUD，服务端上的远程玩家和客户端上的别人的角色都没有
    if (AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetController()))
    {
        if (AWuwaHUD* HUD = Cast<AWuwaHUD>(PC->GetHUD()))
        {
            HUD->InitCtrAndWidget(AbilitySystemComponent, AttributeSet, PC, WuwaPlayerState);
            HUD->WuwaWidgetController->BroadInitialValues();
            HUD->WuwaWidgetController->BindCallBackDependencies();
        }
    }
}


#pragma endregion

#pragma region ASC

//在服务端初始化
void AWuwaCharacter::InitInitialAbilities()
{
    if (!HasAuthority()) return;
    
    UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

    if (!ASC)
    {
        return;
    }
    
    for (const TSubclassOf<UGameplayAbility> Ability : CharacterAbilities)
    {
        check(Ability);
        FGameplayAbilitySpec AbilitySpec(
        Ability,
        1,
        INDEX_NONE,
        this
        );
        if (UWuwaGameplayAbilityBase* WuwaAbility = Cast<UWuwaGameplayAbilityBase>(AbilitySpec.Ability))
        {
            AbilitySpec.GetDynamicSpecSourceTags().AddTag(WuwaAbility->OriginalTag);
            ASC->GiveAbility(AbilitySpec);
        }
        
    }
}


#pragma endregion



