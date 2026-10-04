# EffectSystem 第一阶段：用脚步声验证统一播放入口

当前链路是 `Model → EffectSystem → 工厂选池 → 池借出 AudioSpec → UE 声音组件`。调用方统一传 `FWuwaEffectSpawnRequest`，每一次播放返回一个新的 `FWuwaEffectHandle`。Spec 和声音组件在播放之间复用。目前只注册 Audio；其他 Model 已有数据定义，执行器留到后续阶段实现。

## 文件与阅读顺序

```text
Public/Game/
├─ EffectModel/                  已有的共享数据资产
│  ├─ Base/
│  ├─ Models/                    Audio 等具体 Model
│  └─ Types/
├─ Effect/
│  ├─ Types/                    请求参数、统一句柄
│  ├─ System/                   WorldSubsystem 统一入口
│  ├─ Factory/                  Model 类型到 Spec 池的注册与查找
│  ├─ Pools/
│  │  ├─ Base/                 公共借还、空闲容量、创建/重置/销毁接口
│  │  └─ Audio/                音频组件的归还与销毁策略
│  └─ Specs/
│     ├─ Base/                  公共执行接口
│     └─ Audio/                 Audio 执行器
└─ Animation/Notifies/          动画入口 WuwaAnimNotify_PlayEffect
```

Private 中保存对应实现。按下面的顺序阅读：

1. [WuwaEffectSpawnRequest.h](../Source/Wuwa/Public/Game/Effect/Types/WuwaEffectSpawnRequest.h)、[WuwaEffectHandle.h](../Source/Wuwa/Public/Game/Effect/Types/WuwaEffectHandle.h)：一次调用传入什么、返回什么。
2. [WuwaEffectSystem.h](../Source/Wuwa/Public/Game/Effect/System/WuwaEffectSystem.h)、[WuwaEffectSystem.cpp](../Source/Wuwa/Private/Game/Effect/System/WuwaEffectSystem.cpp)：统一创建、查询和停止入口。
3. [WuwaEffectSpecFactory.cpp](../Source/Wuwa/Private/Game/Effect/Factory/WuwaEffectSpecFactory.cpp)：通过 Model 的真实类查找池；当前类没有注册时向父类查找。
4. [WuwaEffectSpecPool.h](../Source/Wuwa/Public/Game/Effect/Pools/Base/WuwaEffectSpecPool.h)、[WuwaEffectSpecPool.cpp](../Source/Wuwa/Private/Game/Effect/Pools/Base/WuwaEffectSpecPool.cpp)：通用对象池；[WuwaEffectAudioSpecPool.cpp](../Source/Wuwa/Private/Game/Effect/Pools/Audio/WuwaEffectAudioSpecPool.cpp)：Audio 池具体如何清理资源。
5. [WuwaEffectSpec.h](../Source/Wuwa/Public/Game/Effect/Specs/Base/WuwaEffectSpec.h)、[WuwaEffectAudioSpec.cpp](../Source/Wuwa/Private/Game/Effect/Specs/Audio/WuwaEffectAudioSpec.cpp)：公共 Play/Stop/Finish 接口和实际音频执行。
6. [WuwaAnimNotify_PlayEffect.cpp](../Source/Wuwa/Private/Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.cpp)：动画如何填请求并调用系统。

现有 `WuwaEffectActor` 属于原来的 GAS 拾取物流程，与本系统无关。

## 创建与配置音频 DA

1. 内容浏览器右键 **杂项 → 数据资产**，选择 `WuwaEffectModelAudio`。
2. `Audio Backend` 选择 `Unreal Sound`；将一段短脚步声 SoundWave 或 SoundCue 填入 `Sound`。
3. 首次验证使用默认位置配置，不填拖尾声音。脚步声音不要设置循环；一次性通知不保存句柄来停止循环声音。

项目当前没有现成脚步录音。可以先用 `/Game/Effects/Models/Examples/DA_Example_AudioTestTone` 验证链路；它是测试音，不是脚步素材。之后换掉 DA 的 Sound 即可，不必修改播放系统。

## 在动画中添加通知

1. 打开走路或跑步的 Anim Sequence，在脚接触地面的帧添加通知 **Play Effect**。
2. `Model` 选择上面的 Audio DA。
3. `Socket Name` 填对应脚骨名称。长离导出骨骼使用 `Bip001LFoot` 和 `Bip001RFoot`；若自己修改过骨架，以当前骨架名称为准。
4. 脚步保留 `Follow = false`、位置与旋转偏移为零。这样声音在落脚位置播放，不随角色一起移动。
5. 在另一只脚落地时再加一个相同通知，改成另一只脚的骨骼名。进入 PIE 验证。

本阶段没有自动修改现有动画资产。动画编辑器预览尚未接入，需进入游戏或 PIE 才会播放。声音是否有距离衰减、空间化，按 SoundWave/SoundCue 上的 UE 音频配置执行。

通知只有共享配置，不保存 AudioComponent、播放进度或上一次的句柄。两个角色同时使用同一段动画，会各自借出独立的播放实例；正在播放或淡出的对象不会分给另一位调用者。

## 其他代码如何调用

通过 `UWuwaEffectSystem::GetEffectSystem(WorldContextObject)` 取得当前世界的系统，构造 `FWuwaEffectSpawnRequest` 后调用 `SpawnEffect`。

- `Model`：这次使用哪一份数据资产。
- `Transform`：未指定 `AttachTo` 时是世界变换；指定后是相对插槽的偏移。
- `AttachTo`、`SocketName`：需要跟随哪个场景组件、哪个骨骼或插槽。

`SpawnEffect` 是显式立即播放入口；它不等待 Model 的 AutoPlay 或 StartTime。播放失败返回空句柄。

每次播放的 `FWuwaEffectHandle.Id` 都是独立的 `FGuid`。同一个 DA 连续播放两次也会获得不同的句柄。Id 非空只说明曾获得播放标识，不代表现在仍然在播放：

- `IsEffectActive(Handle)`：查询该实例是否仍由系统管理，包括停止时的淡出阶段。
- `StopEffect(Handle)`：按 Audio Model 的淡出配置停止。`StopEffect(Handle, true)` 立即停止。
- `GetActiveEffectCount()`：查看当前世界系统持有的实例数量，方便验证结束后是否回收。
- `GetPooledEffectCount()`：查看已结束、等待再次使用的 Spec 数量。
- `ClearEffectPools()`：释放空闲缓存，不打断正在播放的效果。正在播放的效果结束后仍可正常归还。

调用方需要主动停止播放时保存返回的句柄；脚步这种短音频可以让其自然结束。工厂根据 Model 的真实类选择 Spec，通知无需判断 Audio、Niagara 等类型。

公共接口当前写在 C++ 并暴露给蓝图。后续新增类型通过 `RegisterEffectSpec(ModelClass, SpecClass, PoolClass)` 注册，工厂查找失败时回退父类。PoolClass 省略时使用通用池，其默认 ResetSpec 会调用 Stop(true)，DestroySpec 无额外资源操作。需要保留组件的 Spec 应注册对应的派生池，比如 Audio 必须配 AudioSpecPool。Spec 的 Play/Stop、池的 CreateSpec/ResetSpec/DestroySpec 都是 BlueprintNativeEvent，预留蓝图或 C# 子类实现入口。

UnrealSharp 的实际绑定名称：C# 使用静态属性 `UWuwaEffectSystem.EffectSystem` 获取当前脚本世界的系统，使用 `effects.ActiveEffectCount`、`effects.PooledEffectCount` 查询数量；`SpawnEffect(request)`、`StopEffect(handle, immediately)`、`IsEffectActive(handle)`、`ClearEffectPools()`、`RegisterEffectSpec(...)` 保持方法。池子类可以重写 `protected override CreateSpec()`、`ResetSpec(spec)`、`DestroySpec(spec)`；实际声明按生成绑定的返回类型填写。原生 Get 方法和 WorldContext 参数经过了绑定转换，不要直接照抄 C++ 的 `GetEffectSystem(...)` 调用。C# 构造请求时应显式填写 Transform；C# 的零初始化结构体不会自动使用 C++ 的 FTransform::Identity 默认值。

## 对象池如何工作

```text
SpawnEffect(Request)
  → Factory.FindPool(Model)
  → Pool.Acquire()
       有空闲 Spec：直接取出
       没有空闲 Spec：CreateSpec() → NewObject
  → Spec.PrepareForPlay()：清除上次完成标记和委托
  → 生成新 Handle，保存 Spec 和借出它的 Pool
  → Spec.Play(Request)

自然结束 / 立即停止 / 淡出完成 / 播放失败
  → System 移除本次 Handle
  → 原 Pool.Release(Spec)
  → ResetSpec(Spec)
       Audio：解绑回调、停止声音、清除 Sound/参数、解除挂接、清除淡出状态
  → 有空闲容量：保留 Spec 和 AudioComponent
       否则：DestroySpec(Spec) 销毁 AudioComponent，Spec 等待 GC
```

基类中的 `IdleSpecs` 是空闲栈，优先复用刚归还的对象；`InUseSpecs` 记录已借出的对象，避免重复归还。两者都有 UPROPERTY 强引用。每个 Model 类注册项拥有一个池，同一类的不同 DA 共用它；派生 Model 没有单独注册时共用父类的池。单个池只创建注册的 Spec 类。

默认每池最多保留 32 个**空闲** Spec，可在派生池的默认值中调整 `MaxIdleSpecs`；这不限制同屏声音数。超过上限的归还对象释放组件后交给 GC。0 表示不缓存。此容量是本项目默认值，并非鸣潮原值。

`AudioComponent` 的 AutoDestroy 关闭；清池、超额淘汰、世界结束时由 Audio 池显式销毁。平时归还会清除 Sound 引用，因此不强制长期持有声音资产。正在淡出的声音仍在 ActiveEffects 和 InUseSpecs 中，只有收到真正的 Stopped 才归还。

立即停止的音频线程回执可能晚于下一次播放。UE 5.7 的 AudioComponent 使用 ActiveCount 区分仍未完成的播放：Play 递增，PlaybackCompleted 递减，未归零时不发送完成广播。这里保留引擎计数，并在归还时解绑自己的旧回调，复用同一个组件不伪造完成事件。外部句柄则每次使用新 GUID，旧句柄无法停止新播放。

运行中重新注册不同 Spec/Pool 时，旧池先停用并清空空闲对象；旧播放结束后归还旧池并释放资源。世界结束时拒绝新播放、停止活动项并清空所有池。池按 World 隔离，没有全局静态实例池。

以后添加例如 Niagara：实现 NiagaraSpec 的 Play/Stop，再派生一个 NiagaraSpecPool，实现 ResetSpec（清除本次参数与挂接）和 DestroySpec（释放组件），最后注册 Model/Spec/Pool 三个类。派生池也可覆盖 CreateSpec，但必须返回注册 Spec 类型的新实例；这些生命周期接口应同步完成，ResetSpec 不启动新播放、不重复归还，DestroySpec 要显式释放需要销毁的引擎资源。

原作导出的 `JavaScript/Game/Effect/CustomObjectPool.js` 确实提供池基类与 EffectActorPool，包含创建、取出、归还、销毁接口；TsEffectSystem 另有 LRU 和玩家特效池。本实现采用其分工方式，当前按 Model 类注册管理 Spec，未照搬原作完整的 LRU、按特效路径缓存与角色专用池。

## 当前边界

| 配置或行为 | 本阶段状态 |
|---|---|
| AudioBackend = UnrealSound、Sound | 已支持，使用 UE AudioComponent 播放主声音。 |
| Request 的世界位置、相对插槽位置与挂接 | 已支持。 |
| FadeOutTime、UnrealFadeOutCurve | 已支持，用于 StopEffect 的正常停止；立即停止忽略淡出。 |
| LocationOffsets、KeepAlive、TrailingSound、EnableOcclusion | 配置非默认值时发出提示，本阶段不应用这些字段。 |
| Wwise 事件路径 | 尚未接入，不会转而播放 UE Sound。 |
| Base 的 StartTime、LoopTime、EndTime 和其他生命周期、平台、可见性策略 | 尚未执行；当前按 Sound 自然播放长度结束。声音自身循环时，需要调用方持有句柄停止。 |
| Group、MultiEffect、Niagara 等其他 Spec | 尚未注册，不能通过本阶段系统播放。 |

一份 Model 有字段不代表当前执行器已实现对应行为。这一步只完成通用入口、工厂和 Audio 的基础实例管理，不是原作完整特效系统还原。

原作的 `TsAnimNotifyFootstepAudio` 会调用角色音频组件的 `ChangeFootstepVariant` 与 `PostFootstepVoice`，并非把脚步全部直接送入通用 EffectSystem。这次通用 Notify 是项目验证 Audio 工厂的入口；地表材质、鞋型、角色音色与脚步变体选择留到后续音频模块。

## 本阶段验证

- WuwaEditor 与 UnrealSharp 绑定编译通过，IDE 工程文件已刷新。
- `Wuwa.Effect.Audio.Lifetime` 真实音频测试通过：同一 Model 并发播放、同组件连续 20 次立即停止/重播、旧句柄和音频回执隔离、重复淡出、换音源后的连续自然结束、挂接与位置清理、显式清池销毁组件。
- `Wuwa.Effect.Pool.Lifecycle` 通过：同 Spec 复用、GC 强引用、并发实例隔离、空闲容量、失败和同步完成的唯一归还、清池保留活动项、运行时替换注册、世界结束关闭。最终报告为 2 成功、0 警告、0 失败。
- 测试使用临时声音副本和独立设备；为避免无 PIE 编辑器暂停测试世界音频，仅测试副本使用 UI SoundClass。测试检查解码错误与自然播放时长，没有模拟完成事件，也没有改生产声音为 UI 音频。
- 对象池阶段记录在 `Saved/Diagnostics/EffectPool/`，最终报告为 `Tests/index.json`。脚步落点、实际音色和游戏内空间化仍按上面的 PIE 步骤检查。
