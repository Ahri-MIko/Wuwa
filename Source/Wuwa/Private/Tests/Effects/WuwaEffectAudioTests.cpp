#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Effect/Specs/Audio/WuwaEffectAudioSpec.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaEffectModelAudio.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace WuwaEffectAudioTests
{
	/** 使用真实解码和播放回调，不手动发送 Stopped/Finished。 */
	class FVerifyAudioLifetime final : public IAutomationLatentCommand
	{
	public:
		explicit FVerifyAudioLifetime(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual ~FVerifyAudioLifetime() override
		{
			if (World) World->DestroyWorld(false);
		}

		virtual bool Update() override
		{
			if (!World)
			{
				if (!Initialize()) return true;
				StartLoopChecks();
				return false;
			}

			// 独立测试 World 没有 PIE 游戏循环；驱动真实设备，让非 UI 声音继续播放。
			World->GetAudioDeviceRaw()->Update(true);
			if (System->IsEffectActive(PendingHandle))
			{
				if (FPlatformTime::Seconds() - PhaseStartedAt < 10.0) return false;
				Test->AddError(bWaitingForNaturalEnd
					? TEXT("The real short sound did not finish within 10 seconds.")
					: TEXT("The real looping sound did not finish fading within 10 seconds."));
				return true;
			}

			const double ElapsedSeconds = FPlatformTime::Seconds() - PhaseStartedAt;
			USoundWave* FinishedSound = bWaitingForNaturalEnd ? PingSound.Get() : LoopSound.Get();
			const double ExpectedSeconds = bWaitingForNaturalEnd ? PingSound->GetDuration() : Model->FadeOutTime;
			// 播放失败也会释放句柄；只有解码无错误、且真实时间经过足够长，才算播放/淡出完成。
			Test->TestFalse(TEXT("Completion did not come from a SoundWave decode error"), FinishedSound->HasError());
			if (bWaitingForNaturalEnd)
			{
				Test->TestTrue(TEXT("The short sound lived for at least 70 percent of its real duration"),
					ElapsedSeconds >= ExpectedSeconds * 0.7);
			}
			// 淡出按音频设备时间推进，独立设备的手动 Update 不与墙钟一一对应。
			// StartLoopChecks 已验证 Stop 后仍持有句柄，此处验证真实 Stopped 回调完成回收。
			Test->AddInfo(FString::Printf(TEXT("%s completed: elapsed=%.3fs expected=%.3fs decodeError=%s"),
				bWaitingForNaturalEnd ? TEXT("Natural playback") : TEXT("Fade-out"),
				ElapsedSeconds, ExpectedSeconds, FinishedSound->HasError() ? TEXT("true") : TEXT("false")));
			if (!bWaitingForNaturalEnd)
			{
				Test->TestTrue(TEXT("Old audio callbacks do not stop the immediately reused component"),
					System->IsEffectActive(ReusedHandle) && ReusedComponent->IsPlaying());
				if (!bRepeatedFade)
				{
					// 刚淡出结束的对象必须能再次淡出，不能遗留 bStopping。
					FWuwaEffectSpawnRequest RepeatRequest;
					RepeatRequest.Model = Model.Get();
					PendingHandle = System->SpawnEffect(RepeatRequest);
					System->StopEffect(PendingHandle);
					Test->TestTrue(TEXT("A reused fading Spec stays active during its second fade"), System->IsEffectActive(PendingHandle));
					PhaseStartedAt = FPlatformTime::Seconds();
					bRepeatedFade = true;
					return false;
				}
				System->StopEffect(ReusedHandle, true);
			}
			Test->TestEqual(TEXT("Audio completion releases the active effect"), System->GetActiveEffectCount(), 0);
			Test->TestEqual(TEXT("Both audio instances return to the pool"), System->GetPooledEffectCount(), 2);
			if (bWaitingForNaturalEnd && ++NaturalPlayCount == 2)
			{
				Test->TestNull(TEXT("Idle audio no longer holds a Sound asset"), ReusedComponent->Sound.Get());
				System->ClearEffectPools();
				Test->TestEqual(TEXT("Audio pool clears its cached Specs"), System->GetPooledEffectCount(), 0);
				Test->TestTrue(TEXT("Clearing the pool destroys cached components"),
					!ReusedComponent.IsValid() || !ReusedComponent->IsRegistered());
				return true;
			}

			// 换成非循环临时短音，验证自然播放完成也走相同清理链路。
			Model->Sound = PingSound.Get();
			Model->FadeOutTime = 0.f;
			FWuwaEffectSpawnRequest Request;
			Request.Model = Model.Get();
			PhaseStartedAt = FPlatformTime::Seconds();
			PendingHandle = System->SpawnEffect(Request);
			Test->TestTrue(TEXT("The real short sound starts"), PendingHandle.Id.IsValid());
			Test->TestTrue(TEXT("Natural replay uses the existing component with a different Sound"),
				ReusedComponent->Sound == PingSound.Get() && ReusedComponent->IsPlaying());
			Test->TestEqual(TEXT("Natural replay did not allocate another Spec"), CountAudioSpecs(), 2);
			bWaitingForNaturalEnd = true;
			return false;
		}

	private:
		bool Initialize()
		{
			if (!GEngine || !GEngine->UseSound() || !FAudioDeviceManager::Get())
			{
				Test->AddError(TEXT("Audio must be enabled: remove -nosound; commandlets also need -AllowCommandletAudio."));
				return false;
			}
			USoundWave* EngineTone = LoadObject<USoundWave>(nullptr,
				TEXT("/Engine/EngineSounds/1kSineTonePing.1kSineTonePing"));
			if (!Test->TestNotNull(TEXT("Engine test tone loads"), EngineTone)) return false;
			PingSound.Reset(DuplicateObject<USoundWave>(EngineTone, GetTransientPackage()));
			if (!Test->TestTrue(TEXT("The test tone has a finite short duration"),
				PingSound->GetDuration() > 0.f && PingSound->GetDuration() < 10.f)) return false;
			if (!Test->TestFalse(TEXT("The test tone has no existing decode error"), PingSound->HasError())) return false;

			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(true).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			if (!Test->TestNotNull(TEXT("Audio test world exists"), World)) return false;

			// 独立设备避免改变用户当前世界的音频状态。静音输出仍会真实解码和推进播放。
			FAudioDeviceParams AudioParams;
			AudioParams.AssociatedWorld = World;
			AudioParams.Scope = EAudioDeviceScope::Unique;
			FAudioDeviceHandle AudioDevice = FAudioDeviceManager::Get()->RequestAudioDevice(AudioParams);
			if (!Test->TestTrue(TEXT("A real audio device is available"), AudioDevice.IsValid())) return false;
			World->SetAudioDevice(AudioDevice);
			AudioDevice.GetAudioDevice()->SetDeviceMuted(true);
			System = World->GetSubsystem<UWuwaEffectSystem>();
			if (!Test->TestNotNull(TEXT("Game world creates the effect subsystem"), System)) return false;

			// Headless Editor 没有 PIE，会全局暂停普通游戏声音。只给测试声音使用 UI 声音类，
			// 防止该外部暂停冻结真实采样进度；生产 AudioSpec 保持普通游戏声音，不做此处理。
			// 两阶段都使用临时副本，Engine 声音资产及用户 DA 都不保存、不修改。
			USoundClass* TestSoundClass = NewObject<USoundClass>();
			TestSoundClass->Properties.bIsUISound = true;
			AudioDevice.GetAudioDevice()->RegisterSoundClass(TestSoundClass);
			PingSound->SoundClassObject = TestSoundClass;
			LoopSound.Reset(DuplicateObject<USoundWave>(PingSound.Get(), GetTransientPackage()));
			LoopSound->bLooping = true;
			Model.Reset(NewObject<UWuwaEffectModelAudio>());
			Model->Sound = LoopSound.Get();
			Model->FadeOutTime = 0.15f;
			return true;
		}

		void StartLoopChecks()
		{
			FWuwaEffectSpawnRequest Request;
			Request.Model = Model.Get();
			TStrongObjectPtr<USceneComponent> Attachment(NewObject<USceneComponent>(World));
			Attachment->RegisterComponentWithWorld(World);
			Request.AttachTo = Attachment.Get();
			Request.Transform.SetLocation(FVector(10, 20, 30));
			const FWuwaEffectHandle First = System->SpawnEffect(Request);
			for (TObjectIterator<UAudioComponent> It; It; ++It)
			{
				if (It->GetWorld() == World && Cast<UWuwaEffectAudioSpec>(It->GetOuter())) ReusedComponent = *It;
			}
			if (!Test->TestTrue(TEXT("AudioSpec owns the actual playback component"), ReusedComponent.IsValid())) return;
			Test->TestTrue(TEXT("Initial playback follows the requested attachment"), ReusedComponent->GetAttachParent() == Attachment.Get());
			const FWuwaEffectHandle Second = System->SpawnEffect(Request);
			Test->TestTrue(TEXT("Both plays start with separate handles"),
				First.Id.IsValid() && Second.Id.IsValid() && First.Id != Second.Id);
			Test->TestEqual(TEXT("The same Model supports two live plays"), System->GetActiveEffectCount(), 2);
			Test->TestTrue(TEXT("First play can be stopped independently"), System->StopEffect(First, true));
			Test->TestFalse(TEXT("First handle has finished"), System->IsEffectActive(First));
			Test->TestTrue(TEXT("Second play survives stopping the first"), System->IsEffectActive(Second));
			Test->TestEqual(TEXT("Exactly one play remains"), System->GetActiveEffectCount(), 1);
			Test->TestNull(TEXT("Returning audio releases the previous attachment"), ReusedComponent->GetAttachParent());
			Test->TestNull(TEXT("Returning audio releases its previous Sound"), ReusedComponent->Sound.Get());
			Request.AttachTo.Reset();
			Request.Transform.SetLocation(FVector(100, 200, 300));
			FWuwaEffectHandle OldHandle = First;
			for (int32 Index = 0; Index < 20; ++Index)
			{
				ReusedHandle = System->SpawnEffect(Request);
				Test->TestTrue(TEXT("Rapid reuse starts a new playback"), System->IsEffectActive(ReusedHandle));
				Test->TestTrue(TEXT("Rapid reuse keeps the same AudioComponent"), ReusedComponent->IsPlaying());
				Test->TestFalse(TEXT("A stale handle cannot cancel reused audio"), System->StopEffect(OldHandle, true));
				OldHandle = ReusedHandle;
				if (Index < 19) System->StopEffect(ReusedHandle, true);
			}
			Test->TestNull(TEXT("World-space replay has no leftover parent"), ReusedComponent->GetAttachParent());
			Test->TestTrue(TEXT("World-space replay uses the new position"), ReusedComponent->GetComponentLocation().Equals(FVector(100, 200, 300)));
			Test->TestEqual(TEXT("Repeated playback only allocated two Specs"), CountAudioSpecs(), 2);
			System->ClearEffectPools();
			Test->TestEqual(TEXT("Clearing idle audio does not stop live audio"), System->GetActiveEffectCount(), 2);
			PhaseStartedAt = FPlatformTime::Seconds();
			Test->TestTrue(TEXT("Second play accepts fade-out"), System->StopEffect(Second));
			Test->TestTrue(TEXT("A fading play stays active until the engine stops it"), System->IsEffectActive(Second));
			PendingHandle = Second;
		}

		int32 CountAudioSpecs() const
		{
			int32 Count = 0;
			for (TObjectIterator<UWuwaEffectAudioSpec> It; It; ++It)
			{
				if (It->GetWorld() == World) ++Count;
			}
			return Count;
		}

		FAutomationTestBase* Test;
		UWorld* World = nullptr;
		UWuwaEffectSystem* System = nullptr;
		TStrongObjectPtr<USoundWave> PingSound;
		TStrongObjectPtr<USoundWave> LoopSound;
		TStrongObjectPtr<UWuwaEffectModelAudio> Model;
		FWuwaEffectHandle PendingHandle;
		FWuwaEffectHandle ReusedHandle;
		TWeakObjectPtr<UAudioComponent> ReusedComponent;
		int32 NaturalPlayCount = 0;
		double PhaseStartedAt = 0.0;
		bool bWaitingForNaturalEnd = false;
		bool bRepeatedFade = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectAudioLifetimeTest,
	"Wuwa.Effect.Audio.Lifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectAudioLifetimeTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(WuwaEffectAudioTests::FVerifyAudioLifetime(this));
	return true;
}

#endif
