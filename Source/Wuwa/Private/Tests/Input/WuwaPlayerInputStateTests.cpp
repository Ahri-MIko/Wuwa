#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"

namespace WuwaPlayerInputStateTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		TArray<AWuwaPlayerController*> Controllers;

		~FFixture()
		{
			for (AWuwaPlayerController* Controller : Controllers)
			{
				if (IsValid(Controller))
				{
					Controller->SetPawn(nullptr);
				}
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		static FActorSpawnParameters SpawnParameters()
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Parameters.ObjectFlags |= RF_Transient;
			return Parameters;
		}

		AWuwaCharacter* SpawnCharacter() const
		{
			return World ? World->SpawnActor<AWuwaCharacter>(SpawnParameters()) : nullptr;
		}

		// Ownership only: input is read from the character's own intent component, never from the controller.
		// Only the control link is established; Possess would start unrelated GAS/PlayerState initialization.
		AWuwaPlayerController* Control(AWuwaCharacter* Character)
		{
			AWuwaPlayerController* Controller = World ? World->SpawnActor<AWuwaPlayerController>(SpawnParameters()) : nullptr;
			if (Controller && Character)
			{
				Controller->SetPawn(Character);
				Character->SetController(Controller);
				Controllers.Add(Controller);
			}
			return Controller;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaPlayerInputSnapshotTest,
	"Wuwa.Input.PlayerState.MovementIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaPlayerInputSnapshotTest::RunTest(const FString& Parameters)
{
	WuwaPlayerInputStateTests::FFixture Fixture;
	AWuwaCharacter* Character = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("Character exists"), Character))
	{
		return false;
	}

	TestFalse(TEXT("No input initially"), Character->GetPlayerInputState().bHasMoveInput);
	// 只建立输入读取与方向查询所需的控制关系，避免触发此单元测试无关的 GAS 初始化。
	AWuwaPlayerController* Controller = Fixture.Control(Character);
	if (!TestNotNull(TEXT("Movement has a view rotation provider"), Controller))
	{
		return false;
	}
	Controller->SetControlRotation(FRotator(-35.f, 90.f, 0.f));
	// 不依赖实际移动：CMC 被禁用且没有速度时，仍能获得玩家意图。
	Character->GetWuwaMovementComponent()->DisableMovement();
	WuwaTestInput::SetMoveAxis(Character, FVector2D(0.f, 0.5f));
	const FWuwaPlayerInputState Pressed = Character->GetPlayerInputState();
	TestTrue(TEXT("Input survives disabled movement and zero velocity"), Pressed.bHasMoveInput);
	TestEqual(TEXT("Analog magnitude is preserved"), Pressed.MoveAxis, FVector2D(0.f, 0.5f));
	TestTrue(TEXT("World direction follows camera yaw, ignores pitch, and is normalized"),
		Pressed.MoveWorldDirection.Equals(FVector::RightVector, 0.001));

	// 方向在读取时计算，转镜头后不沿用旧的 MoveInputDir。
	Controller->SetControlRotation(FRotator(0.f, 0.f, 0.f));
	TestTrue(TEXT("Read uses current camera orientation"),
		Character->GetPlayerInputState().MoveWorldDirection.Equals(FVector::ForwardVector, 0.001));

	// Released/Canceled 在现有 Controller 中都转为零轴；残余速度不应算成输入。
	Character->GetWuwaMovementComponent()->Velocity = FVector(500.f, 0.f, 0.f);
	WuwaTestInput::SetMoveAxis(Character, FVector2D::ZeroVector);
	const FWuwaPlayerInputState Released = Character->GetPlayerInputState();
	TestFalse(TEXT("Release clears intent despite remaining velocity"), Released.bHasMoveInput);
	TestTrue(TEXT("Release clears world direction"), Released.MoveWorldDirection.IsZero());
	TestTrue(TEXT("Previously returned snapshot remains a value copy"), Pressed.bHasMoveInput);

	WuwaTestInput::SetMoveAxis(Character, FVector2D(0.001f, 0.f));
	TestFalse(TEXT("Small stick noise does not select forward dash"), Character->GetPlayerInputState().bHasMoveInput);
	TestTrue(TEXT("Below-threshold input has no dash direction"), Character->GetPlayerInputState().MoveWorldDirection.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAbilityPlayerInputTest,
	"Wuwa.Input.PlayerState.AbilityAvatar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAbilityPlayerInputTest::RunTest(const FString& Parameters)
{
	WuwaPlayerInputStateTests::FFixture Fixture;
	AWuwaCharacter* First = Fixture.SpawnCharacter();
	AWuwaCharacter* Second = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("First character exists"), First)
		|| !TestNotNull(TEXT("Second character exists"), Second))
	{
		return false;
	}

	if (!TestNotNull(TEXT("First character has a player controller"), Fixture.Control(First))
		|| !TestNotNull(TEXT("Second character has a player controller"), Fixture.Control(Second)))
	{
		return false;
	}

	UWuwaGameplayAbilityBase* Ability = NewObject<UWuwaGameplayAbilityBase>();
	TestFalse(TEXT("An ability without ActorInfo safely reports no input"), Ability->GetPlayerInputState().bHasMoveInput);
	WuwaTestInput::SetMoveAxis(First, FVector2D(0.f, 1.f));
	WuwaTestInput::SetMoveAxis(Second, FVector2D(-1.f, 0.f));

	FGameplayAbilityActorInfo ActorInfo;
	FGameplayAbilitySpec Spec;
	ActorInfo.AvatarActor = First;
	Ability->OnGiveAbility(&ActorInfo, Spec);
	TestEqual(TEXT("Ability reads its own Avatar"), Ability->GetPlayerInputState().MoveAxis, FVector2D(0.f, 1.f));
	ActorInfo.AvatarActor = Second;
	TestEqual(TEXT("Changed Avatar is queried without a cached character pointer"),
		Ability->GetPlayerInputState().MoveAxis, FVector2D(-1.f, 0.f));
	TestEqual(TEXT("Other character keeps its own input"), First->GetPlayerInputState().MoveAxis, FVector2D(0.f, 1.f));

	ActorInfo.AvatarActor = Fixture.World->SpawnActor<AActor>();
	TestFalse(TEXT("Non-character Avatar returns no input"), Ability->GetPlayerInputState().bHasMoveInput);
	ActorInfo.AvatarActor.Reset();
	TestTrue(TEXT("Missing Avatar returns a zero direction"), Ability->GetPlayerInputState().MoveWorldDirection.IsZero());
	Ability->OnGiveAbility(nullptr, Spec);
	return true;
}

#endif
