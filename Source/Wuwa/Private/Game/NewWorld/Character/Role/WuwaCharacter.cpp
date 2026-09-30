// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "Camera/CameraComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "InputActionValue.h"            // ← 新增：需要 FInputActionValue
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "GameFramework/Controller.h"    // ← 新增：需要 Controller
#include "GameFramework/CharacterMovementComponent.h" 
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"


class UWuwaWidgetController;

namespace
{
    FVector2D CameraRelativeDirection(const FVector2D& InputAxis, const FRotator& ViewRotation)
    {
        const FRotator YawOnly(0.f, ViewRotation.Yaw, 0.f);
        const FVector Direction = UKismetMathLibrary::GetForwardVector(YawOnly) * InputAxis.Y
            + UKismetMathLibrary::GetRightVector(YawOnly) * InputAxis.X;
        return FVector2D(Direction.X, Direction.Y).GetSafeNormal();
    }
}

#pragma region LifeCycle

// Sets default values
AWuwaCharacter::AWuwaCharacter(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer.SetDefaultSubobjectClass<UWuwaMovementComponent>(
        ACharacter::CharacterMovementComponentName))
{
    // UnrealSharp 当前生成类的实际包是 /Script/UnrealSharp，不是 C# namespace Blueprint 包。
    UnifiedStateClass = TSoftClassPtr<UWuwaUnifiedStateBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaUnifiedStateComponent_C")));
    RoleGaitClass = TSoftClassPtr<UWuwaRoleGaitBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaRoleGaitComponent_C")));
    FightStateClass = TSoftClassPtr<UWuwaFightStateBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaFightStateComponent_C")));
    SkillClass = TSoftClassPtr<UWuwaSkillBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaSkillComponent_C")));

    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

}

void AWuwaCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        EnsureMovementStateSystem();
        EnsureSkillSystem();
    }
}



// Called when the game starts or when spawned
void AWuwaCharacter::BeginPlay()
{
    Super::BeginPlay();
    EnsureMovementStateSystem();
    EnsureSkillSystem();
    const AWuwaPlayerController* PC =Cast<AWuwaPlayerController>( GetController());
    UWuwaMoveInputHandler* MovementInputHandler = PC ? PC->MoveInputHandler.Get() : nullptr;
    if (MovementInputHandler)
    {
        MovementInputHandler->OnMove.AddDynamic(this, &AWuwaCharacter::HandleMoveInput);
    }
}


// Called every frame
void AWuwaCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void AWuwaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bFightStateEnding = true;
    // FightState 在自身 EndPlay 中先拒绝新申请，再清理状态，避免提前广播重置。
    bMovementStateEnding = true;
    bMovementStateReady = false;
    if (IsValid(UnifiedStateComponent))
    {
        UnifiedStateComponent->OnStateChanged.RemoveAll(WuwaMovementComponent);
        UnifiedStateComponent->OnStateChanged.RemoveAll(RoleGaitComponent);
    }
    if (IsValid(RoleGaitComponent)) RoleGaitComponent->ResetRuntime();
    Super::EndPlay(EndPlayReason);
}



#pragma endregion

#pragma region ensure each component
//保证当前的角色身上有这个组件
bool AWuwaCharacter::EnsureFightStateSystem()
{
    if (bFightStateEnding || bInitializingFightState || HasAnyFlags(RF_ClassDefaultObject)|| !GetWorld() || !GetWorld()->IsGameWorld()) return false;
    if (IsValid(FightStateComponent) && FightStateComponent->IsRegistered()) return true;

    TGuardValue<bool> InitializingGuard(bInitializingFightState, true);
    FightStateComponent = FindComponentByClass<UWuwaFightStateBridgeComponent>();
    if (!IsValid(FightStateComponent))
    {
        UClass* ManagedClass = FightStateClass.LoadSynchronous();
        if (!ManagedClass || ManagedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            UE_LOG(LogTemp, Error, TEXT("[Wuwa.Combat] Managed FightState class unavailable. Build/publish ManagedWuwa before playing. Class=%s"),
                *FightStateClass.ToString());
            return false;
        }

        FightStateComponent = NewObject<UWuwaFightStateBridgeComponent>(this, ManagedClass,
            MakeUniqueObjectName(this, ManagedClass, TEXT("FightState")), RF_Transient);
        AddInstanceComponent(FightStateComponent);
    }
    //这里的Register是让组件正式生效
    if (!FightStateComponent->IsRegistered()) FightStateComponent->RegisterComponent();
    return IsValid(FightStateComponent) && FightStateComponent->IsRegistered();
}

//确保一定有SkillSystem
bool AWuwaCharacter::EnsureSkillSystem()
{
    if (bFightStateEnding || bInitializingSkill || !EnsureFightStateSystem()) return false;
    if (IsValid(SkillComponent) && SkillComponent->IsRegistered()) return true;

    TGuardValue<bool> InitializingGuard(bInitializingSkill, true);
    SkillComponent = FindComponentByClass<UWuwaSkillBridgeComponent>();
    if (!IsValid(SkillComponent))
    {
        UClass* ManagedClass = SkillClass.LoadSynchronous();
        if (!ManagedClass || ManagedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            UE_LOG(LogTemp, Error, TEXT("[Wuwa.Combat] Managed Skill class unavailable. Build/publish ManagedWuwa before playing. Class=%s"),
                *SkillClass.ToString());
            return false;
        }
        SkillComponent = NewObject<UWuwaSkillBridgeComponent>(this, ManagedClass,
            MakeUniqueObjectName(this, ManagedClass, TEXT("Skill")), RF_Transient);
        AddInstanceComponent(SkillComponent);
    }
    if (!SkillComponent->IsRegistered()) SkillComponent->RegisterComponent();
    return IsValid(SkillComponent) && SkillComponent->IsRegistered();
}


//
bool AWuwaCharacter::EnsureMovementStateSystem()
{
    if (bMovementStateReady && IsValid(UnifiedStateComponent) && IsValid(RoleGaitComponent)) return true;
    bMovementStateReady = false;
    if (bMovementStateEnding || bInitializingMovementState || HasAnyFlags(RF_ClassDefaultObject) || !GetWorld()
        || !GetWorld()->IsGameWorld() || !WuwaMovementComponent) return false;
    
    
    TGuardValue<bool> InitializingGuard(bInitializingMovementState, true);
    UClass* StateClass = UnifiedStateClass.LoadSynchronous();
    UClass* GaitClass = RoleGaitClass.LoadSynchronous();
    if (!StateClass || !GaitClass || StateClass->HasAnyClassFlags(CLASS_Abstract) || GaitClass->HasAnyClassFlags(CLASS_Abstract))
    {
        UE_LOG(LogTemp, Error, TEXT("[Wuwa.State] Managed state classes unavailable. Build/publish ManagedWuwa before playing. State=%s Gait=%s"),
            *UnifiedStateClass.ToString(), *RoleGaitClass.ToString());
        return false;
    }
    UnifiedStateComponent = FindComponentByClass<UWuwaUnifiedStateBridgeComponent>();
    RoleGaitComponent = FindComponentByClass<UWuwaRoleGaitBridgeComponent>();
    if (!IsValid(UnifiedStateComponent))
    {
        UnifiedStateComponent = NewObject<UWuwaUnifiedStateBridgeComponent>(this, StateClass,
            MakeUniqueObjectName(this, StateClass, TEXT("UnifiedState")), RF_Transient);
        AddInstanceComponent(UnifiedStateComponent);
    }
    if (!IsValid(RoleGaitComponent))
    {
        RoleGaitComponent = NewObject<UWuwaRoleGaitBridgeComponent>(this, GaitClass,
            MakeUniqueObjectName(this, GaitClass, TEXT("RoleGait")), RF_Transient);
        AddInstanceComponent(RoleGaitComponent);
    }
    // 两个引用先就绪再注册，避免动态组件 BeginPlay 时只看到半套依赖。
    if (!UnifiedStateComponent->IsRegistered()) UnifiedStateComponent->RegisterComponent();
    if (!RoleGaitComponent->IsRegistered()) RoleGaitComponent->RegisterComponent();
    
    //初始化各种组件,包括Unified组件的默认值,RoleGait组件的默认值,以及CMC组件运动的默认值
    UnifiedStateComponent->OnStateChanged.AddUniqueDynamic(WuwaMovementComponent, &UWuwaMovementComponent::HandleUnifiedStateChanged);
    UnifiedStateComponent->InitializeState();
    RoleGaitComponent->InitializePolicy();
    UnifiedStateComponent->OnStateChanged.AddUniqueDynamic(RoleGaitComponent, &UWuwaRoleGaitBridgeComponent::HandleUnifiedStateChanged);
    bMovementStateReady = true;
    RoleGaitComponent->RefreshPolicy();
    WuwaMovementComponent->RefreshMovementSettings();
    return true;
}

#pragma endregion

#pragma region ProcessInput

FWuwaPlayerInputState AWuwaCharacter::GetPlayerInputState() const
{
    FWuwaPlayerInputState State;
    const AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetController());
    if (!PC || !(PC->GetPawn() == this)) return State;//如果PC为空或者当前的Pawn不是这个直接返回空
    
    
    const FWuwaInputActionState SprintInput = PC->GetInputRouter()->GetInputActionState(FWuwaGameTags::Get().Abilities_Movement_Dash);
    State.bSprintHeld = SprintInput.bHeld;
    State.SprintHeldSeconds = SprintInput.HeldSeconds;
    State.MoveAxis = MoveInput;
    const float Threshold = FMath::Clamp(MoveInputThreshold, 0.f, 1.f);
    State.bHasMoveInput = State.MoveAxis.SizeSquared() > FMath::Square(Threshold);
    if (State.bHasMoveInput)
    {
        const FVector2D Direction = GetCameraRelativeMoveDirection(State.MoveAxis);
        State.MoveWorldDirection = FVector(Direction.X, Direction.Y, 0.f);
    }
    return State;
}

void AWuwaCharacter::ResetPlayerInputState()
{
    MoveInput = FVector2D::ZeroVector;
    MoveInputDir = FVector2D::ZeroVector;
    ConsumeMovementInputVector();
    if (WuwaMovementComponent)
    {
        WuwaMovementComponent->ClearSprintDesire();
    }
}

void AWuwaCharacter::HandleMoveInput(const FInputActionValue& Value)
{
    // 先记录意图，再判断能否移动：Dash 限制移动时，GA 仍能查询玩家是否按着方向。
    MoveInput = Value.Get<FVector2D>();
    MoveInputDir = GetCameraRelativeMoveDirection(MoveInput);
    if (WuwaMovementComponent)
    {
        WuwaMovementComponent->NotifyMoveInputChanged(GetPlayerInputState().bHasMoveInput);
    }
    if (CanApplyMove())
    {
        Move(Value);
    }
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
        MoveInputDir = GetCameraRelativeMoveDirection(MoveInput);
        AddMovementInput(FVector(MoveInputDir.X, MoveInputDir.Y, 0.f), 1.f);
    }

}

bool AWuwaCharacter::CanApplyMove()
{
    //TODO:需要根据当前的情况判断是否能够接收输入
    return  true;
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


FVector2D AWuwaCharacter::GetCameraRelativeMoveDirection(FVector2D InputAxis) const
{
    // ControlRotation 是游戏操作朝向，镜头震动和演出偏移不会改变移动意图。
    return CameraRelativeDirection(InputAxis, GetControlRotation());
}

FVector2D AWuwaCharacter::Vector2ToCameraDirNormalized(const FVector2D InSource2D, const UCameraComponent* InCameraComp) const
{
    return InCameraComp
        ? CameraRelativeDirection(InSource2D, InCameraComp->GetComponentRotation())
        : GetCameraRelativeMoveDirection(InSource2D);
}

#pragma endregion


