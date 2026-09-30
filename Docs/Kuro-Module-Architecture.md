# 鸣潮导出脚本：模块协作与可借鉴的设计

检查日期：2026-09-26。来源为本机 `C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript`。
本次扫描到 19,569 个 `.js` 文件；重点阅读了框架、实体、输入、移动、技能、相机以及背包的代表性实现，并非逐个审计全部业务模块。
结论针对这份导出，不推定其对应的游戏版本。原生 UE/Kuro 函数体、蓝图图表及实际运行时序不能单靠这些 JS 完整还原。

为方便阅读，选定脚本的文本副本位于 `C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926`。
副本只插入阅读换行，文件后缀为 `.js.txt`，没有执行导出的脚本；这些副本也不是重新生成的 TS 源码。
本文的源码定位优先链接这些阅读副本，并标明函数名。原始文件保留在导出目录。

## 1. 总体结构：公共系统与每实体组件协作

| 范围 | 已观察到的实现 | 实际责任 |
| --- | --- | --- |
| 框架基础 | `Core/Framework`、`Core/Entity`、`Core/Event`、`Core/Tick` | 初始化、结束、组件查找、更新调度、事件派发 |
| 公共系统 | `InputController`、`CameraController`、各业务 Controller | 接收请求，协调对应系统 |
| 公共运行数据 | `InputModel`、`CameraModel`、`InventoryModel` 等 | 保存该系统的运行数据、索引、查询和相关维护逻辑 |
| 每个角色的数据与行为 | `CharacterInputComponent`、`RoleGaitComponent`、`CharacterUnifiedStateComponent`、技能/移动等组件 | 保存该实体的状态，并处理角色行为 |
| 表现与引擎连接 | 动画通知、角色输入蓝图、CameraActor、UE Movement、UI View | 提供编辑入口，执行动画、物理、镜头与界面表现 |

`ControllerHolder` 和 `ModelManager` 提供全局访问入口。这里的 Controller 经常是静态脚本类，并不等于 UE 的 `APlayerController`。
这是可以描述为“服务定位器”的访问方式；这个术语是对代码的分析，不是库洛在源码里给出的架构声明。
Model 也并非严格的纯数据结构，不能把整套游戏概括为教科书式 MVC。

`ControllerManagerBase` 统一初始化、清理 Controller，只更新已登记到 Tick 列表中的项。
`Entity` 则统一维护每实体组件的生命周期、依赖检查与 Tick/AfterTick 列表；组件按优先级等规则排序，销毁和结束遍历为逆序。
这是有行为、有生命周期的组件系统，不能仅凭 Entity/Component 命名把它视为数据导向的纯 ECS。

源码：
[ControllerManagerBase](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Core/Framework/ControllerManagerBase.js.txt:10)、
[Entity.AddComponent](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Core/Entity/Entity.js.txt:87)、
[组件更新列表](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Core/Entity/Entity.js.txt:12)。

**适配本项目：** 输入设备状态属于本地玩家；冲刺需求、当前技能、动作窗口属于角色；相机跟随对象属于相机系统。
先确定谁拥有数据，再决定文件放在哪个目录。无需为了模仿脚本框架重新实现 UE 已有的 ActorComponent 生命周期。

## 2. 输入：动作语义 → 分层解释 → 具体指令

已确认的链路：

```text
输入分发 / InputController
  ├─ 保存 Action 的按下时刻、按住时长、轴值
  └─ 根据输入过滤器分发 Press / Release / Hold
        ↓
CharacterInputComponent
        ↓
当前角色的 InputLayer 列表
  ├─ 按 LayerType 降序排列
  ├─ 调用 HandlePress / HandleRelease / HandleHold
  └─ 第一个返回非空且 CommandType != 0 的指令被采用
        ↓
SInputCommand(CommandType, IntValue, TagValue)
        ↓
角色输入组件按 CommandType 分发
  ├─ 技能 ID → 技能执行链
  ├─ 跳跃 → 移动组件
  ├─ 攀爬 → 攀爬组件
  └─ 冲刺 / 走跑切换等 → 对应状态入口
```

`CharacterInputLayer` 的闪避分支调用角色输入蓝图的 `闪避按下` 等函数。
因此导出 JS 显示的是“统一入口 + 可替换解释层”，角色特殊输入规则并不全部写在这个 JS 文件里。
基础层之外还存在载具、交互、瞄准等输入层；是否拦截由对应实现决定，不能只看目录名推断每层全部规则。

`createSkillCommand` 会先参考优先级和技能接受输入的时机。
指令包含具体技能 ID，因此“动作种类只表示 Skill，如何找到具体 GA”并不是问题：种类负责路由，参数负责定位。
`BaseSkillComponent` 再根据技能配置找到 `AbilityClass` 或蒙太奇执行方式。

源码：
[InputController.InputAction](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Input/InputController.js.txt:58)、
[InputLayerUnit.Sort](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Input/InputLayer.js.txt:49)、
[角色输入层](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Input/InputLayer/CharacterInputLayer.js.txt:20)、
[指令分发](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/CharacterInputComponent.js.txt:400)、
[创建技能指令](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Input/InputLayerFunction/InputFunctionCommon.js.txt:7)。

**适配本项目：** 现有 `InputRouter` 已经保存语义输入，并按 RouteTag 找 Handler，但还不是鸣潮这种多层输入解释结构。
当前 Dash 不需要为此重写。以后载具、瞄准、交互开始争用同一个 Action 时，再增加有序处理层，定义 Pass/Handled/Blocked 的明确结果。
设备按键到语义 Action 的映射继续放在输入配置，通知和 GA 读取语义输入。

## 3. 输入缓存：集中重试、选指令，不由每个 GA 自己抢队列

角色输入组件分别保存当帧输入事件和缓存输入。
缓存记录包括 Action、输入阶段、时刻以及累计时间；按 Action/阶段配置缓存寿命，超时删除。

在 `k8r()` 缓存查询中，组件会重新让输入层解释缓存事件，把此刻能形成的指令收集起来，再通过 `U8r()` 按优先级选择一个。
`CharAnimBreakPoint` 回调 `s8r()` 会触发缓存查询；该路径查询执行后清空缓存。
`PostProcessInput` 也处理当帧指令和缓存合并/维护，因此不能把系统简化成“只在一个通知点处理 FIFO 队头”。

源码：
[缓存超时、重新解释与优先级选择](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/CharacterInputComponent.js.txt:355)。

**适配本项目：** 后续如果增加预输入，建议由输入/动作请求模块集中维护候选项，明确缓存失效与执行结果。
消费条件需要结合本项目 GAS 激活结果设计；不能照抄看到的某一段清空逻辑就声称完整复现。

## 4. 移动：每帧决策，状态变化时通知执行者

```text
移动输入 + 位置状态 + 冲刺 Tag + 步态限制
  → RoleGaitComponent.OnTick
  → UpdateMovePressing / UpdateMoveReleasing
  → UnifiedState.SetMoveState
  → 检查控制权、是否变化、位置/移动状态组合是否合法
  → 更新 CachedMoveState、对应 Tag 和原生状态桥
  → EmitWithTarget(角色, CharOnUnifiedMoveStateChanged, old, new)
  → 移动组件重新应用移动配置、速度上限等
```

`BaseMoveComponent.OnMoveStateChange` 调用 `ResetMovementSetting`、`ResetMaxSpeed`、`ResetCharacterMovementInfo`。
`CharacterMoveComponent` 负责注册这个角色的状态监听。
`RoleGait` 也有水中、攀爬等基础移动选择，但并没有包办攀爬检测、技能流程、物理和动画图。

这里需要区分：普通步态策略每帧可以重新计算；提交同一状态时不重复广播状态变更。
整个 setter 仍可能有额外工作，例如导出中的音频调用位于内部“是否变化”判断之前，不能声称相同请求完全零成本。

源码：
[RoleGait.OnTick](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Role/Component/RoleGaitComponent.js.txt:36)、
[UnifiedState.SetMoveState / Kkr](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Abilities/CharacterUnifiedStateComponent.js.txt:114)、
[移动组件订阅](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/CharacterMoveComponent.js.txt:139)、
[BaseMoveComponent 原始文件](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/Game/NewWorld/Character/Common/Component/BaseMoveComponent.js)。

**适配本项目：** 保留 RoleGait 决策、UnifiedState 提交、CMC 执行、AnimBP 表现的分工。
当前 C# RoleGait 仍有多处 `RefreshPolicy()` 入口；本次只做研究，没有把这些入口合并成每帧一次。
未来若收敛调度，应选定物理更新前的一处正常刷新点，保留确有时序需求的动作退出等即时入口。

## 5. 技能：输入解释不是最终执行判定

`BaseSkillComponent.BeginSkill` 的核心流程包括：

1. 查找/加载技能配置和运行实例，检查能否开始。
2. 判断中断关系，结束需要让出的技能。
3. 尝试切换战斗状态，取得对应状态句柄。
4. 选择目标。
5. 根据 SkillMode 调用 `TryActivateAbilityByClass`，或走技能蒙太奇执行路径。
6. 发出技能相关事件，触发相关 Buff 事件。

实际主技能打断判断不只有等级：还包括同等级时 `SkillAcceptInput`、`IsMainSkillReadyEnd` 等条件。
`Skill.InterruptLevel` 会从 `SkillInfo.InterruptLevel` 初始化，技能组件还可以修改活动技能的打断等级。
这说明技能运行实例、配置、战斗状态、GAS 存在分工，不能说所有规则都塞在某个 GA 基类里。

这也不是完整事务：已中断旧技能后，后续战斗状态切换或 GA 激活仍可能失败。
GA 激活失败分支会释放已取得的战斗状态句柄，但本段没有展示“把旧技能完整回滚”的机制。

源码：
[打断条件](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Skill/BaseSkillComponent.js.txt:217)、
[BeginSkill 的执行路径](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Skill/BaseSkillComponent.js.txt:284)、
[技能运行实例](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/Skill/Skill.js.txt:159)。

**适配本项目：** 能力的消耗、冷却、Tags 等继续使用 GAS；通用动作中断规则与请求入口保持集中。
输入层只提供请求与必要的预判，最终激活入口复检。不要让输入函数直接先取消任意当前 GA，再假设新 GA 一定可以启动。

## 6. 相机：独立生命周期、重新绑定目标、按顺序计算

这次补齐后可以确认相机内部链路：

```text
CameraModelInstance
  → EntitySystem 创建 FightCamera 等相机实体
  → FightCamera 创建 LogicComponent 与 DisplayComponent
  → DisplayComponent 在 WorldDone 等节点创建/管理 UE CameraActor

TsCharacterController.ReceivePossess(Pawn)
  → CameraController.OnPossess(Pawn)
  → FightCamera.LogicComponent.SetPawn(Pawn)
  → SetCharacter
      ├─ 移除旧角色的状态与 Tag 监听
      ├─ 保存新角色及所需组件
      ├─ 重新绑定碰撞、旋转区域等目标信息
      ├─ 注册新角色的监听
      └─ 发出 CameraCharacterChanged
```

角色作为相机观察目标传入，相机实体由相机系统管理。换角色无须为每个角色重建一整套相机算法。
同时相机仍直接依赖 `TsBaseCharacter`、输入组件、Tag 组件等，所以这是生命周期和职责分离，并不是完全依赖反转。

`FightCameraLogicComponent.OnAfterTick` 组织每帧主流程：

- 先运行配置控制器等前置步骤并更新角色相关数据。
- 按明确列表依次更新重力、镜头修改、输入、锁定、对话、自动调整、攀爬等控制器。
- 执行限幅、平滑、位置/旋转/FOV 等处理。
- 在 `VPr()` 中计算期望位置，经过 `CameraCollision.CheckCollision`，再设置 CameraActor 的位置和旋转。

源码确实使用 AfterTick；其与所有 UE 原生物理/动画 Tick 的精确先后还需要引擎侧与运行时验证。
不能仅凭函数名承诺已证明所有 CMC 更新都在它前面。

源码：
[相机实体创建](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/CameraModelInstance.js.txt:218)、
[FightCamera 组件装配](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCamera.js.txt:16)、
[PlayerController 的 Possess 入口](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Controller/TsCharacterController.js.txt:16)、
[SetPawn / SetCharacter](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraLogicComponent.js.txt:236)、
[相机更新](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraLogicComponent.js.txt:268)、
[碰撞与最终 Actor 变换](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraLogicComponent.js.txt:362)。

## 7. 相机里特别值得学习的三种机制

### 7.1 模式、配置、临时效果是不同层级

`CameraModelInstance` 保存各镜头模式的启用状态和优先关系，退出当前模式后可选择仍启用的模式。
`CameraConfigController` 根据角色/目标的 Tag、平台开关和配置优先级，组织生效配置与淡入淡出。
`CameraModifyController` 则处理具体的临时镜头修改。

这些不是一个通用“把全部效果加起来”的栈。当前临时 Modify 路径拒绝低于当前优先级的请求；接受新请求时结束/接替之前的修改，并保留过渡所需信息。
不能把这条路径解释为“所有技能镜头都可同时叠加”。

源码：
[相机模式选择](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/CameraModelInstance.js.txt:159)、
[配置按 Tag 与优先级组织](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraController/CameraConfigController.js.txt:170)、
[临时 Modify 优先级](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraController/CameraModifyController.js.txt:107)。

### 7.2 请求实例编号防止旧回调误关新效果

成功申请镜头修改时增加 `ModifyInstance`；结束接口要求蒙太奇和编号都匹配：

```javascript
StopCameraModify(t, i) {
    this.j1_ === t && this.ModifyInstance === i && this.EndModify(true, true);
}
```

例如 A 技能的镜头被 B 技能替换，A 稍后才收到 NotifyEnd，A 携带的旧编号不能结束 B 的镜头。
这是你之前“只释放本次动作持有的状态”的一个可以直接验证的实例。
它仅证明这条接口有实例校验，不代表所有通知对象都天然解决了共享实例问题。

源码：[StopCameraModify](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraController/CameraModifyController.js.txt:306)。

### 7.3 禁用权限按来源保存

`CameraControllerBase.Lock(source)` 把来源放进 Set，`Unlock(source)` 只删除这个来源。
启用条件要求锁集合为空，并满足自身激活条件。
两个系统同时禁用自动调整时，其中一个结束，不会把另一个的限制顺带解除。

源码：[Lock / Unlock / IsActivate](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Camera/FightCameraController/CameraControllerBase.js.txt:69)。

**适配本项目：** 新建相机系统时可先有 Follow、LookInput、SkillModifier 三个职责，加上目标绑定、请求句柄和最终统一更新。
之后再根据实际玩法拆 LockOn、Climb、Collision。可考虑放在本地玩家相机服务或 PlayerCameraManager 周边；具体 UE 容器选择属于本项目设计，不是已经验证的库洛 C++ 类结构。

## 8. 事件：通知事实与发起请求分开

`EventSystem` 同时支持全局事件和带 Target 的事件。
移动状态的例子使用 `EmitWithTarget(Entity, ...)`，消费者只订阅某个角色；切换相机目标时显式解绑旧角色、绑定新角色。

“请执行某事，并告诉我是否成功”在已检查链路中使用直接函数调用，例如创建技能指令、开始技能、申请镜头修改。
“某件事已发生”可以通过事件通知多个消费者，例如移动状态变更、相机观察角色变化、物品变化。
这是一种值得借鉴的划分，并不表示所有库洛代码都严格遵循同一条规范。

事件也不等于下一帧处理：这里 `Emit` 直接调用底层事件系统。设计自己的组件时仍需处理回调中的重入与生命周期变化。

源码：[EventSystem](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Common/Event/EventSystem.js.txt:64)。

## 9. 传统状态机确实存在，但用途不同

`Core/Utils/StateMachine/StateMachine.js` 具有 `AddState`、`Start`、`Switch`、`Update`。
切换时检查目标状态 `CanChangeFrom`，调用旧状态 `Exit` 和新状态 `Enter`；相同状态另有 `CanReEnter` 分支。
`StateBase` 提供 `OnEnter / OnUpdate / OnExit` 等回调。
NPC 的 `CommonNpcPerformComponent` 引用了这套状态机。

另外，`CharacterStateMachineNewComponent` 持有 `AiStateMachineGroup`，有自己的 Tick、控制权、网络状态切换等入口。
它不是仅凭名字就能当成玩家 Walk/Run/Sprint 的总状态机。

因此，可以同时保留：

- 普通步态：输入和条件驱动的轻量决策。
- 爬墙、受击等有明确进入/执行/退出行为的流程：按需要采用专属组件和状态机。
- 动画状态机：处理姿势和动画混合。
- 技能执行：使用 GAS 及动作协调规则。

源码：
[StateMachine.Switch](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Core/Utils/StateMachine/StateMachine.js.txt:16)、
[StateBase](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Core/Utils/StateMachine/StateBase.js.txt:11)、
[NPC 使用者](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/Game/NewWorld/Character/Npc/Component/CommonNpcPerformComponent.js)、
[AI 状态机组件](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/NewWorld/Character/Common/Component/CharacterStateMachineNewComponent.js.txt:15)。

## 10. 背包示例：界面不拥有业务数据的寿命

`InventoryController` 注册网络消息，提供锁定、使用等请求入口，访问 `InventoryModel` 并发出物品相关事件。
`InventoryModel` 保存物品及界面选择等运行数据，提供查询和维护函数。
`InventoryView` 在生命周期中注册/移除事件监听，从 Model 读取内容并更新界面。

因此背包窗口可以关闭、重开，而物品数据仍由业务模块管理。
不过 Controller 也直接调用 UI，View 也直接访问 Model；这不是所有依赖都只通过接口隔离的纯架构。

源码：
[InventoryController](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Module/Inventory/InventoryController.js.txt:8)、
[InventoryModel](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Module/Inventory/InventoryModel.js.txt:7)、
[InventoryView 事件生命周期](C:/UEProjects/Wuwa/Saved/Diagnostics/KuroArchitecture20260926/Game/Module/Inventory/Views/InventoryView.js.txt:211)。

## 本项目建议的学习与实施顺序

1. 保持现有输入快照、RoleGait、UnifiedState、CMC、AnimBP 的责任边界，先收敛普通步态的更新入口。
2. 需要多种模式解释同一个输入时，再扩展现有 InputRouter 为有序处理层。
3. 新相机先做好目标绑定、统一更新、配置与运行实例分离，以及带句柄的临时修改；不要一开始照搬所有相机 Controller。
4. 随着爬墙/游泳等行为增长，将过程逻辑放进专属组件；按流程复杂度选择状态机，而不是不断扩充 RoleGait。
5. Buff、动作限制、相机锁等多来源效果统一采用按来源或句柄释放的规则。

不建议直接照搬庞大的全局 Holder、长串 `GetComponent(数字)`、跨模块深层成员访问以及高度集中的巨型类。
部分难读变量名来自导出构建产物的混淆，不能据此推断库洛原始 TS 也这样命名。

本次新增研究笔记和阅读副本，没有修改 C++、C# 或 UE 资产，也没有执行游戏运行测试。
