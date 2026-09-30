# 战斗系统：阶段 1，FightState 本地裁决

这是第一阶段的独立模块说明。第二阶段已接入主技能与 GA 生命周期，
当前完整调用流程见 [Combat-Skill-Lifecycle.md](Combat-Skill-Lifecycle.md)。
第三阶段的动画接招窗口与预输入用法见 [Combat-Input-Windows.md](Combat-Input-Windows.md)。
四个阶段完成后的总入口见 [Combat-Framework.md](Combat-Framework.md)。

本阶段只落地鸣潮 `CharacterFightStateComponent` 的本地状态裁决与句柄机制。
角色会装配一个真实的 C# `WuwaFightStateComponent`；C++ 只提供 UE 反射、只读快照和事件桥。
不需要在角色蓝图里手动添加组件。它不 Tick，也不保存 GA 或输入缓存。

## 阅读顺序

1. `Source/Wuwa/Public/Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateTypes.h`
   定义战斗类别及 `State / SubStatePriority / Handle` 快照。
2. `Script/ManagedWuwa/Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateComponent.cs`
   查看 `CheckSwitchState`、`TrySwitchState`、`ExitState`、`ResetState` 的实际规则。
3. `Source/Wuwa/Public/Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h`
   查看蓝图、C++ 与脚本之间的接口；对应 `.cpp` 只提交快照并广播事件。
4. `AWuwaCharacter::EnsureFightStateSystem()`
   查看角色如何创建托管组件。重复调用不会清除已有战斗状态。

## 本阶段的职责

```text
调用方请求某个战斗类别
    → C# CheckSwitchState 检查类别及子优先级
    → TrySwitchState 成功后生成新句柄
    → C++ 保存 StateData，广播 OnFightStateChanged

调用方退出
    → ExitState(自己取得的句柄)
    → 仅当前句柄匹配时清空状态
```

FightState 只回答“当前战斗占用能否被这个类别替换”。
成功登记 `Skill` 不等于已经激活 GA；成功登记 `Hit` 不会自动造成伤害、取消技能或播放受击动画。
后续技能管理、受击流程负责这些操作，本阶段没有提供假实现。

## 类别与规则

| 值 | 类别 | 同类别、同子优先级是否允许替换 |
| --- | --- | --- |
| 0 | None，空状态 | 只能通过 Exit / Reset 返回 |
| 1 | Skill，普通技能 | 是 |
| 2 | Hit，普通受击 | 是 |
| 3 | SkillOverrideHit，覆盖受击技能 | 否 |
| 4 | ParryHit，被弹反受击 | 否 |
| 5 | SkillOverrideParry，覆盖被弹反技能 | 否 |
| 6 | WeaknessBreak，被破弱 | 是 |
| 7 | SkillOverrideWeaknessBreak，覆盖被破弱技能 | 否 |
| 8 | Captured，抓取类别占位 | 否 |
| 9 | SpecialSkill，特殊技能 | 是 |
| 10 | StateMachine，状态机主状态 | 否 |

类别不同，较高类别胜出；类别相同，较高子优先级胜出；完全相同使用上表。
例如 `Hit(2, 0)` 可以覆盖 `Skill(1, 255)`，类别优先于子优先级。
输入缓存中的指令选择、技能的 `SkillAcceptInput / ReadyEnd` 不属于这里的判断。
因此 `Skill` 同级允许替换，不能解释为任意时刻都能取消旧技能。

这部分与本地导出
`Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/CharacterFightStateComponent.js.txt`
第 50～66 行的本地比较、句柄退出规则对应。

本项目明确增加的接口约束：请求必须是已知的非 None 类别，子优先级必须在 0～255；
不接受 0 或负数退出句柄；句柄耗尽后拒绝新请求，不回绕复用。
原作通用入口并没有完全相同的参数校验，不能将这些防护称为原代码照抄。

## 如何手动验证

在角色蓝图中通过 `FightStateComponent` 调用以下节点，并保存返回值：

1. `Try Switch State(Skill, 100)` → 得到句柄 A。
2. `Try Switch State(Hit, 0)` → 得到句柄 B。
3. `Exit State(A)` → false，当前仍为 Hit。
4. `Exit State(B)` → true，当前回到 None。

`StateData` 可读到当前类别、子优先级和句柄。
在 C# 中，原生的 `GetStateData()` 通过当前 UnrealSharp 绑定暴露为 `StateData` 属性。
不同角色持有各自的组件，句柄只应交还给签发它的组件。

`CheckSwitchState` 不修改状态或预留位置；提交时仍会重新检查。
`ResetState` 用于角色整体重置，不应替代每个技能正常退出时的 `ExitState(handle)`。
Reset 不重置编号计数，旧请求不能误清重置后取得的新占用。

## 第一阶段完成时的范围

当前 Dash、UnifiedState 移动占用、RoleGait、动画通知和输入 Runtime 的行为保持原状。
FightState 尚未接到它们的激活和退出流程中；此阶段在游戏里不会自动出现战斗状态变化。
角色装配与独立接口已经可用，便于先阅读和验证这块规则。

后续阶段再加入轻量 Skill 运行对象与技能管理入口，连接 GA 的实际生命周期和 FightState 句柄。
届时再逐步迁移旧移动占用、接续窗口、预输入消费；不要在本阶段同时维护两份当前主技能。

暂不实现：受击/抓取行为、伤害计算、完整 AI 状态机、远端状态确认、预测或网络复制。
原作的 `IsLocal / WaitConfirm / ConfirmState` 等网络路径没有被伪装成本地空实现。

## 验证

自动化测试位于 `Source/Wuwa/Private/Tests/Combat/WuwaFightStateTests.cpp`，前缀为
`Wuwa.Combat.FightState`，通过真实 UnrealSharp 组件验证装配、角色隔离、裁决和旧句柄保护。
构建顺序沿用项目约定：正常 C++ Editor 构建 → 发布 ManagedWuwa → 无界面 UE 自动化测试。
UBT 不添加 `-NoLiveCoding`；原因见 `Docs/Movement-State-System.md`。

2026-09-27 验证结果：C++ Editor 构建、ManagedWuwa Release 发布均通过；
3 项 FightState 测试及 7 项现有移动/GA/Dash 回归测试全部通过。
其中 1 项带有项目已有的 GameplayCueNotifyPaths 未配置警告，没有失败测试。
报告：`Saved/Automation/FightStateStage1/index.json`。
