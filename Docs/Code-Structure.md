# 代码目录与职责

本项目仍使用一个原生模块 `Wuwa` 和一个手写 C# 工程 `ManagedWuwa`。
下列目录是源码归类，不是新建的 UE 模块。

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
   │  ├─ WuwaInputRouteHandler
   │  ├─ WuwaInputTypes
   │  ├─ WuwaInputComponent
   │  └─ DataAsset/                WuwaInputDataAsset、WuwaInputConfig
   ├─ NewWorld/Character/
   │  ├─ Common/
   │  │  ├─ WuwaCharactorBase
   │  │  └─ Component/
   │  │     ├─ Input/              输入快照、角色指令及其处理器
   │  │     ├─ Move/               CMC、步态与冲刺需求
   │  │     ├─ Anim/               动画实例、采样数据与运动计算
   │  │     └─ Abilities/          ASC、属性集、GA 与 AbilityTask 基类
   │  └─ Role/                    WuwaCharacter
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
| 步态、移动速度、冲刺窗口运行状态 | `Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h`、`WuwaMovementTypes.h` |
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
└─ Game/AnimNotifyState/
   ├─ AnimNotifyState_MovementCancelWindow.cs
   └─ AnimNotifyState_DesireToKeepSprint.cs
```

`MovementCancelWindow` 检查移动意图并调用 GA 的蒙太奇交接函数。
`DesireToKeepSprint` 读取语义冲刺输入，更新角色 CMC 持有的窗口状态。
通知对象只保存配置，角色各自的运行数据保存在角色组件中。

连续时间窗口使用 `Game/AnimNotifyState`；以后新增单次通知时使用同级
`Game/AnimNotify`。目录只在有实际源码时创建。

这两个已有 C# 类的命名空间保持原值：

| 类 | 保留的命名空间 |
| --- | --- |
| `UAnimNotifyState_MovementCancelWindow` | `ManagedWuwa.Animation.Notifies` |
| `UAnimNotifyState_DesireToKeepSprint` | `Notifies` |

UnrealSharp 使用命名空间参与生成 UE 类型路径。移动源码目录不要求更改命名空间，
本次保持它们不变，以保留现有动画资产引用。将来若统一命名空间，应单独处理类型迁移和资产引用。

## 当前冲刺需求的数据流

```text
输入映射 / InputAction
    → Controller.HandleRoutedInput
    → InputRouter：记录 Pressed / Released / Canceled，分发输入
    → Controller.GetSprintInputState
    → Character.GetPlayerInputState
    → C# AnimNotifyState_DesireToKeepSprint
    → CMC.UpdateSprintDesireWindow
    → 动画数据采样 → AnimBP
```

C# 的 `character.PlayerInputState` 是 UnrealSharp 为原生
`GetPlayerInputState()` 生成的只读属性，每次读取都会调用原生函数。
具体物理键由输入映射处理；通知读取 `SprintHeld` 和 `SprintHeldSeconds`。

## 测试、生成文件与新增代码

- 自动化测试位于 `Source/Wuwa/Private/Tests/{Input,Movement,Animation,Ability}`，测试名称保持不变。
- 项目原生头文件使用相对 `Public` 的完整 include 路径，例如 `Game/Input/WuwaInputTypes.h`。
- 手写游戏逻辑放在 `Source/Wuwa` 或 `Script/ManagedWuwa`；插件源码保持在 `Plugins`。
- `Intermediate/UnrealSharp` 是自动生成绑定，`Script/Wuwa.RuntimeGlue` 是运行时生成的项目绑定；不要在其中维护手写游戏逻辑。
- `bin`、`obj`、`Binaries`、`Intermediate` 是生成输出，不加入版本管理。
- 本次只调整目录、include 和文档引用；UCLASS/USTRUCT 名称、模块名、C# 类型标识与内容资产路径保持不变。
