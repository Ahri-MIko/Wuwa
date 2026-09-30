# 一个攻击 Action，按条件选择不同 GA

本轮把输入意图和技能身份分开。五个攻击 GA 继续用各自的身份 Tag，左键只产生一种攻击输入。

2026-09-27 已完成下面三处资产配置，并在新 UE 进程中重新加载验证。
日常扩展直接编辑 `DA_Changli_InputCommands`，不需要重跑初始化命令。

```text
IA_Attack
  → Input.Combat.Attack / Input.Route.Ability
  → 当前角色的 InputCommandConfig
  → 按规则顺序选中一个 TargetAbilityTag
  → ASC 找到唯一的已授予 Spec
  → 技能接招权限检查 / 原始输入缓存
  → GAS 激活检查 → SkillComponent 交接
```

这一层只处理 `Pressed`。物理按键仍由 `IMC_Character` 配置，规则不用知道鼠标左键或手柄按键。

## 先配置这三处

1. `DA_InputActionTagAsset` 中，`IA_Attack` 只有一行：
   `InputTag = Input.Combat.Attack`，`RouteTag = Input.Route.Ability`。
2. 角色蓝图的 `CharacterAbilities` 授予 `GA_Attack1` 到 `GA_Attack5`，各一次。
   它们的 `OriginalTag` 分别是 `Abilities.Skill.Attack01` 到 `Abilities.Skill.Attack05`。
3. 创建 `WuwaInputCommandConfig` 类型的 DataAsset，并赋给角色蓝图的 `InputCommandConfig`。
   本项目长离的配置路径是 `/Game/Characters/Role/changli/Input/DA_Changli_InputCommands`。

`Rules` 按下表顺序填写；所有行的 `InputTag` 都是 `Input.Combat.Attack`。

| RuleName | RequiredCurrentSkillTag | TargetAbilityTag |
| --- | --- | --- |
| Attack01To02 | Abilities.Skill.Attack01 | Abilities.Skill.Attack02 |
| Attack02To03 | Abilities.Skill.Attack02 | Abilities.Skill.Attack03 |
| Attack03To04 | Abilities.Skill.Attack03 | Abilities.Skill.Attack04 |
| Attack04To05 | Abilities.Skill.Attack04 | Abilities.Skill.Attack05 |
| AttackDefault01 | 留空 | Abilities.Skill.Attack01 |

空 `RequiredCurrentSkillTag` 表示不限制当前技能，**不是只匹配 Idle**。所以最后一行也处理 Dash 中按攻击、Attack05 后继续按攻击等情况。
默认行必须在这些具体规则后面，否则会先选中 Attack01。

`RequiredCurrentSkillTag` 精确匹配当前正在执行的主技能身份。
Attack01 还在执行时按攻击会选择 Attack02；如果 Attack01 已经结束，当前主技能为空，就选择默认 Attack01。
这份配置没有额外保存“技能结束后仍可续连”的连段进度或计时器。

## 不要把选招条件和接招时机混在一起

规则只决定“这次攻击想执行哪个 GA”，不会提前开放打断权限。
五个攻击如果同为 `InterruptLevel = 100`，仍要在各自蒙太奇上配置
`AnimNotifyState_SkillAcceptInput`，到窗口才能让下段接上。
每个 GA 需要通过 ASC 管理的播放链路播放蒙太奇，例如 `PlayMontageAndWait`，并在完成、取消和打断时正确结束。
通知和播放配置见 [Combat-Input-Windows.md](Combat-Input-Windows.md)。

例如正在 Attack01 的前半段：

```text
按攻击 → 首条规则命中，目标为 Attack02
        → 同级接招窗口尚未开放
        → 缓存 Input.Combat.Attack，而不是缓存 Attack02 的 Spec

窗口 Begin → 权限开放 → 检查缓存
           → 再按现在的技能、Tag、属性解析规则
           → 选择 Attack02 → 提交 GAS → 交接
```

缓存到期后不会自动续招。缓存期间角色状态变化，下一次解析可能选择另一个派生。
状态查询用最新值；当前实现仍在新输入或动画断点等明确机会检查，不因任意属性变化每帧重试。
需要在 GA 自定义阶段重查时，先更新状态，再调用已有 `CallAnimBreakPoint()`；它本身不会开放接招权限。

## 加一个特殊派生

在普通规则前面新增更具体的一行，可以同时使用三组限制，它们都要通过：

- `RequiredCurrentSkillTag`：当前必须正在执行哪个主技能，空表示不限。
- `OwnerTagQuery`：查询 ASC **拥有的角色状态 Tag**；可以用查询编辑器配置 All/Any/No。
- `Conditions`：附加条件对象数组，全部返回 true；空数组不限，空对象是配置错误。

比如“Attack02 中，角色具有强化状态时，下一次攻击走另一个 GA”，配置：
`RequiredCurrentSkillTag = Abilities.Skill.Attack02`，`OwnerTagQuery` 要求强化状态 Tag，
`TargetAbilityTag` 指向强化 GA，并将该行放在 `Attack02To03` 前面。
强化状态 Tag 必须确实通过 GameplayEffect、ActivationOwnedTags 或项目已有状态逻辑加入 ASC。

**技能的 `OriginalTag` 和角色拥有的状态 Tag 不相同。**
授予 GA 时，`OriginalTag` 写入 Spec 的身份标签，供目标查找；它不会自动加入 ASC 的 Owned Tags。
“当前在 Attack02”已经有 `RequiredCurrentSkillTag`，不用再复制一份状态 Tag。

需要判断属性、距离或自定义规则时，派生 `UWuwaInputCondition`，实现
`Evaluate(FWuwaInputCommandContext Context)`，并将实例添加到这一行的 `Conditions`。
上下文提供 `ASC`、`Avatar`、`CurrentSkill` 和原始 `InputTag`。
从上下文读取当前属性再返回 bool；对象只保存阈值等配置，不能保存某个角色的计时和运行状态。
配置资产会被多个角色共享。`Evaluate` 是查询，不应结束技能、改 Tag 或触发新的输入。

本轮已提供 C# 条件 `InputCondition_AttributeMinimum`：在 `Conditions` 里添加这个类型，
选择 `Attribute` 并设置 `Minimum`，当角色当前属性值 `>= Minimum` 时通过。
类路径为 `/Script/UnrealSharp.InputCondition_AttributeMinimum_C`。
它每次通过 ASC 的 `TryGetAttributeValue` 读取当前值，所以预输入下次重新解析时会使用最新属性；
条件属性没有配置或无法读取时不通过。

当前没有实现完整的条件公式解析器。数组使用 AND；需要 OR 可以用 `OwnerTagQuery`，
或配置多条指向同一目标的规则。

## 首条命中之后发生什么

| 情况 | 行为 |
| --- | --- |
| 输入 Tag 没有配置任何规则 | 沿用直接按 Tag 找 GA，保留现有 Dash |
| 有规则，但当前没有一条条件成立 | 按有效期缓存原始输入，下次机会重查 |
| 首条命中，但当前不能接招 | 缓存原始输入，不尝试下面的规则 |
| 命中的目标未授予，或对应多个 Spec | 配置失败，不偷偷选择下面的技能 |
| 已向 GAS 提交，但消耗、冷却等检查拒绝 | 本批输入已消费，不依次试其他规则 |

规则数组的顺序决定一个输入选哪个技能。已有的 `InterruptLevel` 决定技能能否打断，
并在多个不同输入都能执行时选择哪个输入。这两种顺序不要混用。
`ClearBufferedInput(Input.Combat.Attack)` 清的是通用攻击预输入，不应填某段技能的身份 Tag。
缓存时长的覆盖配置也应填 `Input.Combat.Attack`。

## 通过命令配置当前资产

`WuwaCombatInputSetup` 是项目专用的编辑器命令let。默认只读审计；加 `-Apply` 才保存资产。
先关闭同一项目的编辑器，确保已完成本轮原生 Editor 和 Managed 构建，再运行：

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\UEProjects\Wuwa\Wuwa.uproject' -run=WuwaCombatInputSetup -unattended -nop4 -nullrhi -nosound -UTF8Output
```

确认审计后执行配置：

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\UEProjects\Wuwa\Wuwa.uproject' -run=WuwaCombatInputSetup -Apply -unattended -nop4 -nullrhi -nosound -UTF8Output
```

`-Apply` 的范围固定为：创建上述五行配置、规范 `IA_Attack` 的一行语义绑定、将配置赋给
`BP_WuwaCharacterBase`、确保五个攻击 GA 在初始授予列表中各出现一次。原列表中的其他 GA 和其他输入行保留。
命令不改 GA 默认值、蓝图图表、蒙太奇通知、物理 IMC 或地图。

已有配置必须与上述五行一致才能重复执行；若规则已有自定义内容、目标路径是其他类型资产，
或角色已指向另一份配置，命令拒绝覆盖。后续维护自己编辑 DataAsset 即可，不必重新运行初始化命令。
若某个 GA 的 `OriginalTag` 不符、没有设为主技能，或其他 GA 占用了同一攻击身份，
命令同样停止并报告原因。不同 Spec 可以使用 `Instanced Per Actor`；命令记录实例策略，不强制改成 `Instanced Per Execution`。

保存前备份现有文件到 `Saved/Backups/CombatInputSetup/<时间-唯一编号>/`，保留 Content 下的相对目录。
新建配置原先没有文件，因此没有旧文件备份。保存失败可能已有部分文件写入，日志会指出并保留备份。
角色蓝图在保存任何资产前编译并校验配置/授予列表没有丢失。

审计 JSON 在 `Saved/Diagnostics/CombatInputCommands/`：

- `SetupAudit.json`：默认只读运行结果。
- `BeforeApply.json`、`AfterApply.json`：保存前后的静态配置。

审计只证明资产配置，不代表已经播放动画或验证了五段攻击的画面。
“输入已绑定、技能已授予”与“动画接招窗口在预期时间触发”需要分别验证：
在 PIE 中先测空闲按攻击是否执行 Attack01，再测窗口前、窗口中、缓存过期后的攻击输入。
通过资产审计不能代替这些实际播放检查。

## 与鸣潮导出的对应

本地导出的 `InputFunctionAttack.js` 将通用攻击送到 `createInputCommandFromDataTable`。
`InputModel.js` 按 Action/State 从 `DT_InputCommandTransform` 分组；
`InputFunctionCommon.js` 顺序检查角色 Tag 和 BehaviorCondition，首条满足后生成具体 SkillId 命令。
`SkillBehaviorCondition.js` 支持角色 Tag、属性和其他条件，无公式时是全部 AND。

本项目复用这一分工：语义输入 → 条件配置 → 具体技能 → 执行检查；
`TargetAbilityTag` 对应本项目的技能身份，`UWuwaInputCondition` 是轻量的扩展点。
上面的五段规则是针对当前五个 GA 配置的示例，没有声称它就是鸣潮长离原表的完整普攻链。
导出中没有足够的真实表行来证明长离所有强化、空中和多段派生的具体条件。

## 本轮验证和仍需配置的动作时机

UE 5.7 Editor 原生编译、ManagedWuwa Release 发布通过。
39 项自动化全部通过：新增 9 项条件选招，原有 30 项战斗/预输入/通知/Dash/移动回归。
其中 1 项有项目原有 GameplayCueNotifyPaths 配置警告，0 失败。
测试报告：`Saved/Automation/CombatInputCommands/index.json`。
已验证真实 C# 属性条件、AND 条件、连段身份、缓存条件变化后重选、首命中失败不回退、旧 Dash 输入兼容。

保存后重新加载的配置检查：`Saved/Diagnostics/CombatInputCommands/SetupAudit.json`。
五个攻击身份与授予均有效；左键仍绑定 IA_Attack，该 Action 现在路由到通用攻击输入；角色引用五行规则资产。

本轮只配置选招，没有代选动画接招帧。实际加载的 `AM_Attack01` 蒙太奇通知轨道目前为空，
没有本框架的 `SkillAcceptInput` / `SkillReadyEnd` 通知。
如果 GA 内没有手动开放权限，同为 100 级的 Attack02 即使被规则选中，也会等待接招机会或缓存过期。
要在动画尚未结束时接第二段，需在你选择的时点放 `AnimNotifyState_SkillAcceptInput`；后续段按同样方式配置。
这不是配置表自动决定的时间，也不能通过增加另一条同键绑定来代替。
当前验证是逻辑和资产配置验证，没有宣称已验证五段实机动画手感。
