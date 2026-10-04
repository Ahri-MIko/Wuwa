// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"


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
    InputIntent = CreateDefaultSubobject<UWuwaInputIntentComponent>(TEXT("InputIntent"));

    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

}

void AWuwaCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // 角色是组装者：把输入意图交给需要它的组件，组件之间互不查找，也不回头读角色。
    if (WuwaMovementComponent)
    {
        WuwaMovementComponent->BindInputIntent(InputIntent);
    }
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
        UnifiedStateComponent->OnMoveStateChanged.RemoveAll(WuwaMovementComponent);
        UnifiedStateComponent->OnGaitChanged.RemoveAll(WuwaMovementComponent);
    }
    // 结束阶段：CMC 不再驱动步态，RoleGait 不再读取物理事实和输入（脚本据此不再推进状态）。
    if (WuwaMovementComponent) WuwaMovementComponent->BindMovementState(UnifiedStateComponent, nullptr);
    if (IsValid(RoleGaitComponent))
    {
        RoleGaitComponent->BindDependencies(UnifiedStateComponent, nullptr, nullptr);
        RoleGaitComponent->ResetRuntime();
    }
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
    BindSkillDependencies();
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
    
    // 角色是组装者：依赖都在这里注入，组件之间不互相查找，也不回头读角色。
    // 依赖方向：CMC -> RoleGait -> UnifiedState；RoleGait 只通过引擎基类读物理事实。
    RoleGaitComponent->BindDependencies(UnifiedStateComponent, WuwaMovementComponent, InputIntent);
    WuwaMovementComponent->BindMovementState(UnifiedStateComponent, RoleGaitComponent);
    BindSkillDependencies();

    //初始化各种组件,包括Unified组件的默认值,RoleGait组件的默认值,以及CMC组件运动的默认值
    // 与原作相同：CMC 监听运动状态的变化更新速度配置；RoleGait 不订阅状态事件，由 CMC 每次物理更新前驱动。
    UnifiedStateComponent->OnMoveStateChanged.AddUniqueDynamic(WuwaMovementComponent, &UWuwaMovementComponent::HandleMoveStateChanged);
    UnifiedStateComponent->OnGaitChanged.AddUniqueDynamic(WuwaMovementComponent, &UWuwaMovementComponent::HandleGaitChanged);
    UnifiedStateComponent->InitializeState(WuwaMovementComponent->ReadPositionState(), WuwaMovementComponent->GetInitialDesiredGait());
    RoleGaitComponent->InitializePolicy();
    bMovementStateReady = true;
    RoleGaitComponent->RefreshPolicy();
    WuwaMovementComponent->RefreshMovementSettings();
    return true;
}

// 技能组件与运动状态可能先后装配，任一方就绪时都重新注入一次。
void AWuwaCharacter::BindSkillDependencies()
{
    if (IsValid(SkillComponent))
    {
        SkillComponent->BindDependencies(FightStateComponent, UnifiedStateComponent, RoleGaitComponent);
    }
}

#pragma endregion

#pragma region ProcessInput

FWuwaPlayerInputState AWuwaCharacter::GetPlayerInputState() const
{
    return IsValid(InputIntent) ? InputIntent->GetPlayerInputState() : FWuwaPlayerInputState{};
}

void AWuwaCharacter::ResetPlayerInputState()
{
    ConsumeMovementInputVector();
    if (IsValid(InputIntent))
    {
        InputIntent->ResetInputs();
    }
    // 冲刺需求由 RoleGait 保存；角色结束阶段不再驱动它（与原来经 CMC 转发时一致）。
    if (!bMovementStateEnding && IsValid(RoleGaitComponent))
    {
        RoleGaitComponent->ResetSprintRequest();
    }
}

#pragma endregion


