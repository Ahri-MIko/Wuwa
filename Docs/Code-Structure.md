# 代码目录与职责

本项目仍使用一个原生模块 `Wuwa` 和一个手写 C# 工程 `ManagedWuwa`。
下列目录是源码归类，不是新建的 UE 模块。

战斗模块的完整调用链、接口和分阶段阅读入口见 [Combat-Framework.md](Combat-Framework.md)。

本次参考的是鸣潮已导出的 `JavaScript/Game/AnimNotifyState`、
`Game/NewWorld/Character/Common/Component` 和 `Character/Role` 等目录。
`Common/Component` 下的 `Input`、`Move`、`Anim`、`Abilities` 均能在导出中找到。
UE 的 `Public/Private` 边界、GAS 类及 `Core/Asset` 等具体安排属于本项目的适配，
不表示已还原鸣潮未导出的 C++ 工程结构。

## C++

头文件放在 `Source/Wuwa/Public`，实现文件放在 `Source/Wuwa/Private` 的对应位置。
以 `Public` 为例：

```text
Public/
├─ Core/
│  ├─ Asset/                       WuwaAssetManager
│  └─ Utilities/                   DebugHelper
└─ Game/
   ├─ Common/                      WuwaGameTags
   ├─ Controller/                  WuwaPlayerController
   ├─ Framework/                   WuwaGameMode、WuwaPlayerState
   ├─ Input/
   │  ├─ WuwaEnhancedInputComponent
   │  ├─ WuwaInputRouterComponent
   │  ├─ IWuwaInputRouteHandler
   │  ├─ WuwaInputTypes
   │  ├─ WuwaInputComponent
   │  └─ DataAsset/                WuwaInputDataAsset、WuwaInputConfig
   ├─ NewWorld/Character/
   │  ├─ Common/
   │  │  ├─ WuwaCharactorBase
   │  │  └─ Component/
   │  │     ├─ Input/              输入快照、角色指令及其处理器
   │  │     ├─ Move/               CMC、运动参数配置和步态枚举
   │  │     ├─ Anim/               动画实例、采样数据与运动计算
   │  │     └─ Abilities/          ASC、GA、统一状态快照与脚本桥
   │  └─ Role/                    WuwaCharacter、Component/RoleGait 脚本桥
   ├─ Effect/                     WuwaEffectActor
   └─ UI/                         WuwaHUD、WuwaUserWidget、WuwaWidgetController
```

### 输入与冲刺代码从哪里找

| 职责 | 文件位置（相对 Public；实现位于对应 Private） |
| --- | --- |
| 把 Enhanced Input 事件发给路由器 | `Game/Controller/WuwaPlayerController.h` |
| 保存语义指令的按住状态、时长，分发给 Handler | `Game/Input/WuwaInputRouterComponent.h` |
| InputAction → 输入 Tag / Route Tag 配置 | `Game/Input/DataAsset/WuwaInputDataAsset.h` |
| 输入阶段、事件和连续按住状态 | `Game/Input/WuwaInputTypes.h` |
| 移动输入处理器与走跑指令 | `Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h`、`WuwaInputCommand.h` |
| 技能输入处理器 | `Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h` |
| 给 GA 和通知读取的输入快照 | `Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h` |
| 组装当前角色的输入快照 | `Game/NewWorld/Character/Role/WuwaCharacter.h` 中的 `GetPlayerInputState()` |
| 应用运动参数、兼容旧蓝图入口 | `Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h`、`WuwaMovementSettings.h` |
| 统一状态快照、状态变更事件 | `Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h`、`WuwaUnifiedStateTypes.h` |
| 脚本步态组件的反射接口 | `Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h` |
| GA 和蒙太奇移动交接 | `Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h` |
| 采集并提供 AnimBP 数据 | `Game/NewWorld/Character/Common/Component/Anim/` |

`Game/Input` 负责输入基础设施；角色的指令含义和处理规则放在
`Character/Common/Component/Input`。当前主入口是 Controller 的
`HandleRoutedInput → InputRouter.DispatchInput`。
原有 `WuwaInputComponent` 的委托/长按接口保留，本次文件整理没有改写输入行为。

## C#

```text
Script/ManagedWuwa/
├─ ManagedWuwa.csproj
├─ ManagedWuwa.cs                 模块入口
└─ Game/
   ├─ AnimNotifyState/
   │  ├─ AnimNotifyState_MovementCancelWindow.cs
   │  └─ AnimNotifyState_DesireToKeepSprint.cs
   └─ NewWorld/Character/
      ├─ Common/Component/Abilities/WuwaUnifiedStateComponent.cs
      └─ Role/Component/WuwaRoleGaitComponent.cs
```

`MovementCancelWindow` 检查移动意图并调用 GA 的蒙太奇交接函数。
`DesireToKeepSprint` 读取语义冲刺输入，通过兼容的 CMC 入口更新 C# RoleGait 的窗口状态。
通知对象只保存配置，角色各自的运行数据保存在角色组件中。

连续时间窗口使用 `Game/AnimNotifyState`；以后新增单次通知时使用同级
`Game/AnimNotify`。目录只在有实际源码时创建。

这两个已有 C# 类的命名空间保持原值：

| 类 | 保留的命名空间 |
| --- | --- |
| `UAnimNotifyState_MovementCancelWindow` | `ManagedWuwa.Animation.Notifies` |
| `UAnimNotifyState_DesireToKeepSprint` | `Notifies` |

本项目当前 UnrealSharp 配置生成的实际类位于 `/Script/UnrealSharp`，类名带 `_C`；
namespace 对应的蓝图包路径不能直接当作 UClass 路径。已有命名空间保持不变。
Character 的软类引用默认加载 `WuwaUnifiedStateComponent_C` 和 `WuwaRoleGaitComponent_C`，
游戏世界中自动装配这两个组件；编辑器动画预览不创建游戏状态组件。

## 当前冲刺需求的数据流

```text
输入映射 / InputAction
    → Controller.HandleRoutedInput
    → InputRouter：记录 Pressed / Released / Canceled，分发输入
    → Controller.GetSprintInputState
    → Character.GetPlayerInputState
    → C# AnimNotifyState_DesireToKeepSprint
    → CMC.UpdateSprintDesireWindow（兼容转发）
    → C# RoleGait：窗口／持久需求 + Walk/Run 偏好 + 步态限制
    → C# UnifiedState：合法组合校验、动作占用、提交状态
    → OnStateChanged
        → CMC：按已提交步态应用速度、加速度、摩擦、制动参数
        → 动画数据采样 → AnimBP
```

C# 的 `character.PlayerInputState` 是 UnrealSharp 为原生
`GetPlayerInputState()` 生成的只读属性，每次读取都会调用原生函数。
具体物理键由输入映射处理；通知读取 `SprintHeld` 和 `SprintHeldSeconds`。

冲刺窗口与窗口结束后的需求分开保存在每个角色的 C# RoleGait 组件：

- `Begin/UpdateSprintDesireWindow` 在窗口内采集连续按住时长；超过 0.2 秒为 `Sustained`，否则为 `Temporary`。窗口内松开冲刺输入仍会回到 `Temporary`。
- `EndSprintDesireWindow` 提交最后一次采样，再删除窗口。C# 的 NotifyEnd 会先补采样最后一帧。
- `Temporary` 从窗口结束起保留 `TemporarySprintDuration` 游戏秒，默认 **1 秒**；CMC 每次物理更新前调度脚本处理过期，不依赖 AnimBP 持续更新。重复 End 不会续期。
- 已提交的 `Sustained` 没有时限，松开冲刺键不会降级；松开移动方向时清除。判断使用语义移动输入，而非仍可能有 RootMotion 残余的 Velocity。窗口结束时已无移动输入也不会留下长期需求。
- 不同来源的重叠窗口或后续 Dash 的临时需求不会覆盖已保留的长期需求；各角色分别保存结果。
- 离开地面移动、蹲伏、控制切换、清空玩家输入和 EndPlay 会清理需求。动作系统强制退出普通移动（如未来的受击/死亡逻辑）可调用 `ClearSprintDesire()`，同时作废未完成窗口，迟到的 Tick/End 不会恢复需求。

`GetSprintDesire()` 汇总“角色保留的需求 + 当前开放窗口”。它仍然是意图查询，
原有 Walk/Run 选择保留。RoleGait 结合输入、地面状态、动作占用及步态限制，决定是否进入 Sprint。
这里的三态需求与一秒时限是本项目规则，不代表已获取鸣潮原生动画类中的完整实现。

动画蓝图继承 `WuwaAnimInstance` 后，可在 **Event Graph** 直接调用
`Reset Sprint Desire`（Target 为 Self）。例如给 Idle 设置 Entered State Event，
在对应事件上调用一次。节点通过 CMC 转发，清除 RoleGait 的持久需求和全部开放窗口，
同时刷新本实例的整个 `LocomotionData`，保证冲刺需求和实际状态一致。它是游戏线程执行节点，不用于 Transition Rule
或 Blueprint Thread Safe Update Animation。其他蓝图也可通过 CMC 调用
`Clear Sprint Desire` 完成同样的角色状态清理。

## 测试、生成文件与新增代码

- 自动化测试位于 `Source/Wuwa/Private/Tests/{Input,Movement,Animation,Ability}`，测试名称保持不变。
- 项目原生头文件使用相对 `Public` 的完整 include 路径，例如 `Game/Input/WuwaInputTypes.h`。
- 手写游戏逻辑放在 `Source/Wuwa` 或 `Script/ManagedWuwa`；插件源码保持在 `Plugins`。
- `Intermediate/UnrealSharp` 是自动生成绑定，`Script/Wuwa.RuntimeGlue` 是运行时生成的项目绑定；不要在其中维护手写游戏逻辑。
- `bin`、`obj`、`Binaries`、`Intermediate` 是生成输出，不加入版本管理。
- 状态框架与使用方法见 [Movement-State-System.md](Movement-State-System.md)。
- 战斗选招规则见 [Combat-Input-Commands.md](Combat-Input-Commands.md)：C# `Game/NewWorld/Input/WuwaInputCommandResolver.cs` 解释角色 DataAsset，`Input/Conditions` 存放无运行状态的条件类。
