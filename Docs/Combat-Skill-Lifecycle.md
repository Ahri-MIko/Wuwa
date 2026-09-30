# 战斗系统：阶段 2，主技能与 GA 生命周期

这是阶段 2 的实现记录。阶段 3 已接通动画窗口和预输入，使用方式见
[Combat-Input-Windows.md](Combat-Input-Windows.md)。
阶段 4 的同步断点、清缓存和最终接入说明见 [Combat-Framework.md](Combat-Framework.md)。

本阶段接通了实际 GA。继承 `UWuwaGameplayAbilityBase` 的主动技能默认加入主技能管理，
角色自动创建 C# `WuwaSkillComponent`，无需手动添加组件。

## 先读这两个脚本

1. `Script/ManagedWuwa/Game/NewWorld/Character/Common/Component/Skill/WuwaSkill.cs`
   一次执行的普通 C# 对象，记录 GA 弱引用、打断等级、FightState 句柄。没有 Tick。
2. 同目录 `WuwaSkillComponent.cs`
   保存唯一的 `_currentSkill`，决定技能能否让位，并维护开始/结束流程。

然后阅读 `WuwaGameplayAbilityBase.cpp` 的 `CanActivateAbility / PreActivate / EndAbility`，
查看 UE 的 GA 生命周期如何接入脚本。C++ `WuwaSkillBridgeComponent` 仅提供反射接口；
`GetCurrentSkillData()` 每次向脚本查询，不额外保存第二份当前技能。

## 三个模块分别做什么

| 模块 | 保存/处理的内容 |
| --- | --- |
| SkillComponent | 当前主技能、是否允许同级接招、是否已进入让位阶段 |
| FightStateComponent | 当前战斗类别及覆盖优先级，例如普通技能、受击、覆盖受击技能 |
| GameplayAbility | 蒙太奇、AbilityTask、技能执行；把真实开始/结束交给 SkillComponent |

当前主技能通过 SkillComponent 查询，不从移动占用 Source 或 ASC 当前蒙太奇反推。
没有移动、没有蒙太奇的主技能，也能被正确登记。

## 一次激活的完整链路

```text
现有输入 Runtime → ASC.RequestAbilityActivation / TryActivateAbility
  → GA.CanActivateAbility
      → Skill.CanBeginSkill：纯查询打断权限和战斗类别
      → 现有移动合法性检查
      → GAS 自身的标签、冷却、消耗等激活检查
  → GA.PreActivate 的 Super：建立本次 GAS 执行上下文
  → Skill.TryBeginSkill：重新检查并开始交接
      → 有旧主技能：请求旧 GA.EndAbility，等待同步清理实际完成
      → 旧 GA 清理任务、蒙太奇、移动占用，回报 Skill.EndSkill(旧句柄)
      → FightState.TrySwitchState：申请新战斗状态
      → 创建 WuwaSkill，记录新 GA 和新句柄，关闭上一轮窗口
  → GA 取得本次移动占用（仅 bOverridesMoveState 为 true 时）
  → GA.ActivateAbility：确认登记成功，再进入蓝图执行

GA 自然结束 / 被取消 / 启动中止
  → 原有任务与蒙太奇清理
  → 释放自己取得的移动占用
  → Skill.EndSkill(自己的句柄)
  → FightState.ExitState(自己的句柄)
```

如果开始阶段未取得技能或移动占用，GA 在进入蓝图前结束并归还已取得的资源。
外部受击等流程已换成新 FightState 句柄时，旧技能的清理不会把它清掉。

## 可配置的 GA 字段

在 GA 蓝图 Class Defaults 的 `Wuwa | Combat | Skill` 中：

| 字段 | 默认值 | 用途 |
| --- | --- | --- |
| Is Main Skill | true | 是否进入主技能管理；被动/辅助 GA 关闭 |
| Interrupt Level | 100 | 技能打断等级，当前范围 0～255 |
| Skill Override Type | None | 申请普通技能或可覆盖受击等战斗类别 |

OverrideType 映射：None→Skill，Hit→SkillOverrideHit，Parry→SkillOverrideParry，
WeaknessBreak→SkillOverrideWeaknessBreak，Special→SpecialSkill。
它只配置战斗裁决类别，不会自动补齐受击、霸体或伤害行为。

`ActionMoveStatePriority` 继续属于旧移动占用，不与 `InterruptLevel` 共用。
新技能允许接替旧主技能时，移动预检查只忽略那个即将退出的旧主技能来源；
位置合法性和其他来源的占用仍需通过。正式取得移动占用时照常重新检查。

## 技能让位规则与手动使用

有当前主技能时，以下任意一个条件成立才能接招：

```text
新 InterruptLevel > 旧 InterruptLevel
或者：等级相同，并且 SkillAcceptInput == true
或者：MainSkillReadyEnd == true
```

以两个等级都为 100 的技能 A、B 为例：

1. A 执行期间窗口默认关闭，B 被拒绝，A 继续执行。
2. 在 A 的 GA 蓝图中调用 `Set Skill Accept Input(true)`，B 就可以结束 A 并开始。
3. `Set Skill Accept Input(false)` 撤销同级接招权限。
4. A 调用 `Set Skill Ready End(true)` 后，等级为 20 的 B 也可以接上。
5. 新技能开始时两个标记都重新置为 false，不继承旧技能窗口。

本阶段只提供上述接口，尚未添加对应的动画 NotifyState，也不自动重放预输入。
原先 Dash 的移动取消通知继续工作；它调用 EndAbility 时现在还会清理主技能记录。

阶段 3 的正式通知使用原生播放实例编号和通知事件编号标记窗口归属，
由 SkillComponent 保存。不要在迟到的 NotifyEnd 中重新查询“当前技能”然后关闭它的窗口。

## 与导出的鸣潮代码对应及本项目适配

参考导出目录 `Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/`：

- `Skill/BaseSkillComponent.js.txt:217`：高等级、同级 AcceptInput、ReadyEnd 的打断规则。
- `Skill/BaseSkillComponent.js.txt:294`：先结束旧技能，再申请新 FightState。
- `Skill/BaseSkillComponent.js.txt:333`：GA 与 Skill 的开始/结束关联。
- `Skill/Skill.js.txt:157`：Skill 结束时释放自己的 FightState 句柄。

这里保留这些职责与规则，但没有复制原作资源加载、PendingSkill、多技能组和网络路径。
原作由 Skill.Begin 请求 ASC；本阶段沿用项目现有 ASC 输入入口，
通过 GA 的生命周期进入脚本管理，确保直接 TryActivateAbility 也接受同样检查。
配置暂放在 GA 默认值中，运行数据仍属于独立 WuwaSkill 对象。

原作 GA 模式用 K2_EndAbility 请求旧技能结束。本项目也走正常 EndAbility，
不依赖 CancelAbilitiesWithTag 或 CanBeCanceled。旧 GA 处于 ScopeLock 时暂时拒绝交接，
不在它还未真正结束时覆盖当前技能。

交接过程中不允许事件回调重入再启动第三个主技能。
也不要在旧 GA 的同步 K2_OnEndAbility / ASC AbilityEnded 回调中立即再启动主技能：
此时旧技能清理尚未完成。下一阶段的缓存消费应安排在最终 EndSkill 之后或下一帧。

ReadyEnd 允许低等级接招的原因是：旧技能先结束，旧 FightState 也随之释放，
新技能面对的是释放后的状态，而不是用低等级强行覆盖仍在执行的高等级技能。

## 阶段范围

已接入：实际主 GA 的登记、打断、退出、战斗状态句柄、既有 Dash 移动占用。

阶段 3 已实现：动画接招窗口、输入列表中选择一次后清批，以及运行时修改技能打断等级。
受击行为、伤害、连段选招、被动技能组、多组并行、网络同步仍留待后续。

## 验证

集成测试文件 `Source/Wuwa/Private/Tests/Combat/WuwaSkillLifecycleTests.cpp`，前缀
`Wuwa.Combat.Skill`。测试使用真实 GAS 执行实例和真实生成的 C# 组件。
还需运行已有 FightState、移动状态、GA 状态预检查和真实 Dash 生命周期回归。

2026-09-27：C++ Editor 构建、ManagedWuwa Release 发布通过。
5 项技能集成测试与 10 项 FightState/移动/GA/Dash 回归测试全部通过；
其中 1 项保留项目已有的 GameplayCueNotifyPaths 未配置警告，没有失败测试。
报告：`Saved/Automation/SkillStage2/index.json`。未进行画面手感验收。

测试中的临时角色不会自动执行正常的 BeginPlay，因此显式装配移动与技能组件。
`WuwaSkillTestAbility` 仅把测试期间调整的原生 CDO 配置复制到实例，
让不同测试请求能表达不同等级；打断策略、GAS 生命周期和失败清理仍执行生产代码。
