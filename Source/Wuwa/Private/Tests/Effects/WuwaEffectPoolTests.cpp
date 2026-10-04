#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Tests/Effects/WuwaEffectPoolTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectPoolLifecycleTest,
	"Wuwa.Effect.Pool.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectPoolLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();
	TStrongObjectPtr<UWuwaEffectPoolTestModel> Model(NewObject<UWuwaEffectPoolTestModel>());
	System->RegisterEffectSpec(UWuwaEffectPoolTestModel::StaticClass(), UWuwaEffectPoolTestSpec::StaticClass(),
		UWuwaEffectPoolTestPool::StaticClass());
	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();

	const auto First = System->SpawnEffect(Request);
	const TWeakObjectPtr<UWuwaEffectSpec> FirstSpec = Model->LastPlayed;
	auto* Pool = CastChecked<UWuwaEffectPoolTestPool>(FirstSpec->GetOuter());
	TestTrue(TEXT("First play starts"), System->IsEffectActive(First));
	System->StopEffect(First, true);
	TestEqual(TEXT("Completion returns the instance"), System->GetPooledEffectCount(), 1);
	Pool->Release(FirstSpec.Get());
	TestEqual(TEXT("Duplicate release cannot add the instance twice"), Pool->GetIdleCount(), 1);
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("The idle pool keeps its Spec alive across GC"), FirstSpec.IsValid());

	const auto Second = System->SpawnEffect(Request);
	TestTrue(TEXT("A later play reuses the exact same Spec"), Model->LastPlayed == FirstSpec);
	TestTrue(TEXT("Reused Spec gets a fresh handle"), Second.Id.IsValid() && Second.Id != First.Id);
	TestFalse(TEXT("Old handle cannot stop the new play"), System->StopEffect(First, true));
	TestTrue(TEXT("Reused Spec has reset completion state"), System->IsEffectActive(Second) && !FirstSpec->IsFinished());
	const auto Third = System->SpawnEffect(Request);
	TestTrue(TEXT("Overlapping plays use different Specs"), Model->LastPlayed != FirstSpec);
	const auto Fourth = System->SpawnEffect(Request);
	TestEqual(TEXT("Idle capacity does not cap concurrent plays"), System->GetActiveEffectCount(), 3);
	System->StopEffect(Second, true);
	System->StopEffect(Third, true);
	System->StopEffect(Fourth, true);
	TestEqual(TEXT("Idle capacity is enforced"), Pool->GetIdleCount(), 2);
	TestEqual(TEXT("Capacity overflow is discarded"), Pool->DiscardedCount, 1);

	Model->bFailToPlay = true;
	TestFalse(TEXT("Failed playback returns no handle"), System->SpawnEffect(Request).Id.IsValid());
	TestEqual(TEXT("Failure returns the borrowed Spec exactly once"), Pool->GetIdleCount(), 2);
	Model->bFinishInsidePlay = true;
	TestFalse(TEXT("Synchronous finish plus failure returns no handle"), System->SpawnEffect(Request).Id.IsValid());
	Model->bFailToPlay = false;
	TestFalse(TEXT("Synchronous finish does not return an already finished handle"), System->SpawnEffect(Request).Id.IsValid());
	TestEqual(TEXT("Synchronous completion does not duplicate idle entries"), Pool->GetIdleCount(), 2);
	TestEqual(TEXT("Failures and synchronous finishes did not allocate new Specs"), Pool->CreatedCount, 3);
	Model->bFinishInsidePlay = false;

	const auto LiveDuringClear = System->SpawnEffect(Request);
	System->ClearEffectPools();
	TestEqual(TEXT("Clear removes only idle instances"), System->GetPooledEffectCount(), 0);
	TestTrue(TEXT("Clear preserves active playback"), System->IsEffectActive(LiveDuringClear));
	System->StopEffect(LiveDuringClear, true);
	TestEqual(TEXT("A cleared pool can be used again"), Pool->GetIdleCount(), 1);

	const auto OldRegistrationPlay = System->SpawnEffect(Request);
	System->RegisterEffectSpec(UWuwaEffectPoolTestModel::StaticClass(), UWuwaEffectPoolTestSpec::StaticClass(),
		UWuwaEffectSpecPool::StaticClass());
	TestTrue(TEXT("Changing registration does not stop existing plays"), System->IsEffectActive(OldRegistrationPlay));
	const auto NewRegistrationPlay = System->SpawnEffect(Request);
	TestTrue(TEXT("New playback belongs to the new pool"), Model->LastPlayed->GetOuter() != Pool);
	System->StopEffect(OldRegistrationPlay, true);
	TestEqual(TEXT("Old registration never returns into the replacement pool"), System->GetPooledEffectCount(), 0);
	TestEqual(TEXT("Retired pool does not retain returning Specs"), Pool->GetIdleCount(), 0);
	System->StopEffect(NewRegistrationPlay, true);
	System->SpawnEffect(Request);
	System->OnWorldEndPlay(*World);
	TestEqual(TEXT("World end stops active effects"), System->GetActiveEffectCount(), 0);
	TestEqual(TEXT("World end clears idle effects"), System->GetPooledEffectCount(), 0);
	TestFalse(TEXT("World end rejects new playback"), System->SpawnEffect(Request).Id.IsValid());
	World->DestroyWorld(false);
	return true;
}

#endif
