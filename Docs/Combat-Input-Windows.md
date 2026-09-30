# 战斗系统：阶段 3，接招通知与预输入

阶段 4 已把动画断点改为同步事件，并补齐清缓存接口；本文使用说明已更新。
最终接口和阅读顺序见 [Combat-Framework.md](Combat-Framework.md)。

本阶段接通 `InputTag → 缓存 → 动画接招机会 → 选一个技能 → GAS → SkillComponent`。
技能交接仍走上一阶段的真实 EndAbility / PreActivate；没有另建一个主技能状态机。
原有 Dash 移动取消、冲刺需求、步态和相机逻辑保留。

## 先看这三个文件

1. `Script/ManagedWuwa/Game/AnimNotifyState/AnimNotifyState_SkillAcceptInput.cs`
   Begin 开放同级接招，End 撤销本窗口。
2. `Script/ManagedWuwa/Game/AnimNotify/AnimNotify_SkillReadyEnd.cs`
   标记当前技能进入让位阶段，并将它的运行时 InterruptLevel 设为 0。
3. `Script/ManagedWuwa/Game/NewWorld/Input/WuwaCombatInputRuntime.cs`
   `ProcessInput` 处理新输入，`ProcessPendingInput` 在接招机会出现后重新选取缓存。

`CombatInputBuffer.cs` 只保存原始输入和过期时间，不保存 GA 实例或决定能否打断。
`WuwaSkillComponent.cs` 继续负责权限判断、当前 Skill、旧技能结束和新技能登记。

## 在蒙太奇里怎样使用

以下是需要你按动作手感配置的动画资源步骤。本阶段没有替你修改真实蒙太奇的轨道或时间。

1. 打开需要接招的 **AnimMontage**，在它自身的 Notifies 轨道添加
   `AnimNotifyState_SkillAcceptInput`（显示名“技能同级接招窗口”）。
2. 把状态通知的左边界拖到允许同等级技能接替的时刻，右边界拖到撤销此权限的时刻。
   例如技能 A、B 的 InterruptLevel 都是 100：A 进入此窗口后，B 才能接替 A。
3. 如果动作后段要允许更低等级技能也接上，另加单点通知
   `AnimNotify_SkillReadyEnd`（“技能进入让位阶段”）。它没有持续长度。
   到达此点后，当前 GA 与蒙太奇继续执行；有合法新技能时才交接。
4. GA 继承 `WuwaGameplayAbilityBase`，默认 Is Main Skill 为 true。
   蒙太奇通过 ASC 管理的 GAS 播放路径播放，例如 `PlayMontageAndWait`。
   直接只对 AnimInstance 调 MontagePlay、没有 ASC 关联的动画不会取得技能窗口权限。
5. 此版把通知放在 **Montage 自身轨道**，不要放在底层 Sequence。
   当前桥通过 Montage 内的事件索引确定唯一窗口；Sequence 通知暂不支持。

原先 Dash 的“有移动输入就 StopMontageForMovement + EndAbility”通知保留。
它解决回到移动；上述通知解决技能之间接招。不要用 ReadyEnd 替换移动取消通知。
若需要同一 GA 在尚未结束时再次接上自身，此版要求 Instancing Policy 为
`Instanced Per Execution`；活动中的 `Instanced Per Actor` 此版等结束后才能再次激活。

## 一次输入如何走完

```text
Enhanced Input → PlayerController → InputRouter → AbilityInputHandler
  → C# ProcessInput(InputTag, Pressed, 缓存有效期)
      → ASC 查询这个 Tag 对应的已授予 GA
      → 当前允许：提交激活，清除上一批缓存
      → 主技能暂不能接：保存原始 Tag/Phase/时间

动画通知 Begin / ReadyEnd
  → SkillComponent 先更新权限
  → CallAnimBreakPoint：递增序号并广播 OnAnimBreakPoint
  → AbilityInputHandler 立即请求 ProcessPendingInput
      → 清理超时输入
      → 序号没有变化：返回
      → 有新机会：重新解释缓存的 Tag，重新检查当前权限
      → 选 InterruptLevel 最高的一个；同级保留最早输入
      → 清空这一批，向 ASC 提交一次
      → GAS 检查标签、冷却、消耗、移动合法性等
      → 接上一阶段主技能交接链路

旧技能真实 EndAbility 完成 / GAS ScopeLock 中的断点
  → 只更新 InputOpportunitySerial
  → AbilityInputHandler 的 PostUpdateWork Tick 再处理
```

`InputOpportunitySerial` 只是“需要再检查一次缓存”的编号，不是权限快照。
实际执行时必须重新检查 SkillComponent 当前规则。
仅修改运行时等级不会自动制造断点；ReadyEnd 和窗口 Begin 会。
EndAbility 的机会在其清理完成后才消费，避免在结束调用栈里再次启动技能。
`SkillStartSerial` 另行记录主技能开始次数；另一技能即便在两次输入检查之间完整开始并结束，
上一批缓存也会被丢弃。正常结束原技能不会增加此编号，因此仍能接上它收到的预输入。

例如 A 后段 0.6 秒开始接招，B 的缓存时间为 0.3 秒：

| 操作 | 结果 |
| --- | --- |
| A 的 0.4 秒按 B，0.6 秒打开窗口 | B 尚未过期，尝试接上 |
| A 的 0.1 秒按 B，0.6 秒打开窗口 | B 已过期，不自动执行 |
| 窗口前连续按两次同一个 B，两次均未过期 | 只提交一次，随后清空两条输入 |
| 同时缓存 B(100) 和 C(150)，当前两者都允许 | 选择 C |
| 选中 C，但 GAS 拒绝激活 | 本批仍已消费，不再依次尝试 B |
| 没有有效新输入时到达 ReadyEnd | 保持当前动画，等待自身结束 |

高等级技能原本就可接替低等级技能，不必等同级窗口。
ReadyEnd 修改的是当前 `WuwaSkill` 的运行时等级；GA 默认配置和已经取得的
FightState 子优先级不修改。旧技能结束后再归还其 FightState 句柄。

## 配置和重置

PlayerController 蓝图的 `AbilityInputHandler` 组件默认值中：

- `Default Buffer Lifetime Seconds`：默认 0.3 秒。
- `Buffer Lifetime Overrides`：按语义 InputTag 配置特例，0 表示不缓存。

不读取 LeftShift 或其他物理键，键鼠/手柄仍由现有输入映射产生相同 Tag。
当前只处理 Pressed。输入 Tag 经 [输入转换规则](Combat-Input-Commands.md) 选出技能身份 Tag；
同一输入可以按条件选不同 GA，但一个目标技能身份必须唯一对应一个已授予 GA。
目标匹配多个 Spec 返回 Ambiguous，不会绕过首条已命中的规则。

切 Pawn、FlushPressedKeys、重建输入绑定和组件 EndPlay 都清缓存。
Runtime 也检查 ASC/Avatar 更换、时间倒退、新主技能开始，以及外部 FightState 接管。
主动切换游戏输入上下文时，应走这些重置入口；此版没有实现完整 UI 输入层。
`GetBufferedInputCount()` 可用于调试，返回上次处理后的剩余数量。

## 为什么有几个 C++ 通知桥

当前 UnrealSharp 生成的 `FAnimNotifyEventReference` 绑定没有保留原生 Notify 指针、
ContextData 和 MontageInstanceID。直接在 C# 的 ReceivedNotifyBegin/End 中找“当前 GA”，
会把旧播放迟到的 End 错当作新播放的 End。

`Source/Wuwa/Private/Game/Animation/Notifies/WuwaSkillNotifyContext.h` 及两个原生通知桥
只负责在上下文丢失前读取身份、验证实际播放，再调用 C#：

```text
BeginSkillWindow(Skills, SkillHandle, MontageInstanceId, NotifyEventId)
EndSkillWindow(Skills, MontageInstanceId, NotifyEventId)
ReachSkillReadyEnd(Skills, SkillHandle)
```

C# 组件用 `(MontageInstanceId, NotifyEventId)` 集合保存窗口。
两个窗口重叠时，结束其中一个不会关闭另一个；旧播放的 End 也无法关闭新播放的窗口。
通知对象本身不保存每个角色的运行数据。Queued 与 Branching Point 都有原生入口。

## 与本地鸣潮导出的对应和差异

格式化导出根目录：`Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/`。

- `CharacterInputComponent.js.txt:220`：未被选中的原始输入按配置加入缓存数组。
- `CharacterInputComponent.js.txt:364`：动画断点重新把缓存转换成当前可执行命令。
- `CharacterInputComponent.js.txt:382`：严格 `>` 选择最高优先级，同级保留较早项。
- `CharacterInputComponent.js.txt:448`：提交技能命令；调用方清批，不等待 GA 激活成功。
- `Input/InputLayerFunction/InputFunctionCommon.js.txt:10`：高等级、同级 AcceptInput、ReadyEnd 预检查。
- `Skill/BaseSkillComponent.js.txt:419`：CallAnimBreakPoint 通知输入层检查。

原始通知导出在 `C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/Game/`：

- `AnimNotifyState/TsAnimNotifyStateNextAtt.js`：Begin 开放 AcceptInput 并触发断点；End 关闭。
- `AnimNotify/TsAnimNotifyEndSkill.js`：设置 ReadyEnd、当前 Priority=0、触发断点，未直接结束 GA。

因此不是“从 GA 开始到取消窗口才允许缓存”，也不是“只有一个输入槽”。
缓存有效期和技能接招窗口各自独立。本项目缓存主技能按下输入，以及已配置但当前条件未命中的输入，
原作还处理长按、松开以及更多输入层转换。

长离导出 `BP_Input_ChangLi.json` 中攻击/闪避的按下缓存配置为 0.3 秒；
本项目选用这个值作为可改默认值，不声称所有鸣潮角色都使用同一常量。
本项目按世界游戏时间过期，暂未接入原作对角色自定义时间缩放的计时处理。

阶段 4 的普通动画断点在 Begin 内立即检查，解决上一阶段依赖帧末造成的短窗口漏消费。
EndAbility 清理中和 GAS ScopeLock 中仍延后处理，不强行破坏生命周期。
通知应放在蒙太奇开始混出前，并保证它仍属于当前 GA；停止/被替换的播放不会再开放权限。
本版提供 `GA.CallAnimBreakPoint()` 与 `GA.ClearBufferedInput(InputTag)`，没有额外添加
独立 BreakPoint/ClearInputCache 通知类；需要自定义阶段时可从当前 GA 调用这些接口。

## 验证

自动化测试位于 `Source/Wuwa/Private/Tests/Combat/`：
`WuwaCombatInputBufferTests.cpp` 和 `WuwaSkillNotifyTests.cpp`。
使用真实 C# 类、GAS 和技能组件，覆盖缓存选择、真实结束后消费、通知身份及交接隔离。
2026-09-27：UE 5.7 Editor 原生构建、ManagedWuwa Release 发布均通过。
7 项预输入测试、2 项通知测试与 15 项已有技能/FightState/移动/Dash 回归，共 24 项全部通过。
其中 1 项有已有的 GameplayCueNotifyPaths 配置警告，失败数为 0。
报告：`Saved/Automation/CombatStage3/index.json`。

输入路由测试实际经过 Controller → Router → Handler → Runtime，并验证 Handler Tick 消费及
FlushPressedKeys 重置。通知测试使用真实播放实例并从原生入口注入 Queued/Branching 上下文，
没有推进实际动画时间轴验证通知触发时点。轨道位置和画面手感仍需在你配置的动画上验收。
