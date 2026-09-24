# Movement → 动画：这次实现怎么读

这次只搭建 Common 移动的数据读取层，不重新设计长离的动画状态机，也不生成猜测的步幅、播放速率或取消窗口。Alt 的输入链路见 [Alt-WalkRun.md](Alt-WalkRun.md)。

## 与鸣潮导出资源的对应关系

原资源根目录：`C:/GamePakExtractor/Output/Exports/Client/Content/Aki`。

| 导出中可确认的结构 | 本 Demo 的对应实现 |
| --- | --- |
| `Character/BaseCharacter/BP_ABPLogicParams.cpp`：独立 UObject，含 `InputDirectRef`、`AccelerationRef`、`SpeedRef`、`HasMoveInputRef` 与角色状态 | `UWuwaAnimLogicParams`：保存运动事实 `MoveData` 和游戏状态 `StateData` |
| `TypeScript/Game/NewWorld/Character/Common/Blueprint/Utils/TsMoveBlueprintFunctionLibrary.cpp`：`UpdateAnimInfoMove(entityId, animLogicParams, WorldContext)` | `UWuwaAnimDataLibrary::UpdateAnimInfoMove`：从 Demo 的 Movement 采集运动数据 |
| 同目录 `TsGameplayBlueprintFunctionLibrary.cpp`：`UpdateAnimInfoUnifiedState(...)` | `UpdateAnimInfoUnifiedState`：采集 MovementMode、步态、落地、攀爬与蹲伏状态 |
| `Character/Role/Common/ABP_BaseRole.json`：继承 `KuroAnimInstanceRole`，通过 Property Access 使用运动/步态变量 | `UWuwaAnimInstance`：提供供 AnimBP 使用的值类型 `LocomotionData` |

**证据边界：**导出的上述函数体是空壳，无法证明原作如何计算这些字段，也无法恢复参数对象到原生 AnimInstance 的完整调用链。因此借鉴的是“参数对象、分项更新接口、动画消费变量”的职责划分；从 `UWuwaMovementComponent` 取值、对象归属、快照更新时机和起停公式是本 Demo 的 C++ 实现。没有声称这是鸣潮原代码，也没有把所有移动数据包装成输入命令。

## 阅读顺序

1. `Source/Wuwa/Public/Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataTypes.h`：采样需要哪些运动事实、游戏状态。
2. `Source/Wuwa/Private/Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.cpp`：每个字段具体从哪里取得。
3. `Source/Wuwa/Private/Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.cpp`：何时采样，谁持有参数对象，何时清空。
4. `Source/Wuwa/Public/Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionMath.h`：原始采样如何变为动画需要的局部方向、起停信号。
5. `Source/Wuwa/Public/Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h`：最终暴露给 AnimBP 的只读字段。

数据依次经过 `Movement → AnimDataLibrary → AnimLogicParams → BuildAnimationData → AnimInstance.LocomotionData`。动画不会反向写入 Movement，也不读取 Alt 按键。

## 为什么同时有参数对象和结构体

- `AnimLogicParams` 是游戏线程上的采样对象，每个 AnimInstance 单独拥有一个，不是共享 DataAsset；`MoveData` 和 `StateData` 分开维护。
- `LocomotionData` 是一次动画更新的值快照。新增/迁移的 AnimGraph 节点应只读它，不跨线程读取角色、Movement 或参数 UObject；现有图还没有全部迁移。
- 以后增加受击、攀爬细节、瞄准等数据时，可以添加独立采样分组及对应更新函数；不必让输入回调重写整个动画结构体。
- `EWuwaGait` 移到游戏侧 `WuwaMovementTypes.h`，避免 Movement 反过来依赖动画类型。反射枚举名称保持不变。

## 不能混在一起的四个概念

| 字段 | 含义 |
| --- | --- |
| `LocalMoveIntent` / `bHasMoveInput` | 最近一次 CMC 消费的移动输入，转换到 Actor 局部坐标；不是原始按键，也不是加速度 |
| `GroundSpeed` / `LocalVelocity` | 实际运动速度；松手后仍可能有惯性 |
| `DesiredGait` / `AllowedGait` | 期望步态 / 当前规则允许的步态；站着不动时也能选择 Run |
| `MaxSpeed` | `Movement->GetMaxSpeed()` 返回的有效上限；不是此刻实际速度 |

Alt 改变 Movement 持有的步态。CMC 的 `GetMaxSpeed()` 在 Walk 时返回 `min(max(WalkSpeed, 0), 原上限)`。默认 WalkSpeed 是 200 cm/s。这里**没有反复覆盖 MaxWalkSpeed 配置，也没有直接修改 Velocity**。减速由 CMC 处理；因此切换时 `MaxSpeed` 可以先变，`GroundSpeed` 随后才下降。

本轮工作期间代码中另新增了 `RunSpeed = 500`，现有 Run 分支是 `max(RunSpeed, Super::GetMaxSpeed())`；本次动画工作保留了这项改动。因此地面站立跑步时，如果 MaxWalkSpeed 是 600，RunSpeed 配成 500 也仍然返回 600，不是强制使用 500。动画采样调用虚函数，不硬编码任一种速度规则。

当前 `bStateGroundWalk/Run/Sprint` 表达“地面 + 允许步态”，不表示相应动画已经播放。起停信号也只是候选条件，还需要动画状态机和动作许可；不能用它们绕过攻击、闪避等锁定。

## AnimBP 怎么用

现有 `ABP_Changli` 的父类接为 `WuwaAnimInstance`，角色 Mesh 继续使用原 AnimBP。原事件图、状态机、动画资产与转场节点不重写；本次只让它获得自动更新的数据入口。**旧节点不会因为更换父类就自动改成使用新字段。**

1. 在动画蓝图中启用“显示继承的变量”，拖出 `LocomotionData`，拆分结构体或使用 `Break Wuwa Locomotion Anim Data`。
2. 先看 `Has Valid Movement Data`；编辑器预览无角色时为 false 是正常的。
3. 原来自己从 Movement 读取的速度/空中状态，后续逐个替换为 `GroundSpeed` / `State Air`。不要另写一套 Alt 布尔值。
4. 状态机转场、BlendSpace 数据入口可以用 Property Access 读取 `LocomotionData` 的字段；不要在线程安全更新里穿过 `AnimLogicParams` 去查询 UObject。
5. PIE 时选择真正运行的动画实例作为调试对象，观察 `AllowedGait`、`MaxSpeed` 和 `GroundSpeed`：按 Alt 后应看到步态和上限先改变。

`AnimLogicParams` 只用于游戏线程采样/调试，不需要你在 EventGraph 手动创建或调用更新函数。UE 在 `NativeUpdateAnimation` 之后调用普通 `BlueprintUpdateAnimation`，已有事件图可以读到本次快照。

资产修改前的备份：`Saved/Backups/MovementAnimation-20260917-111232/ABP_Changli.uasset`。`Scripts/ConnectMovementAnimation.py` 默认只读检查，只有显式传 `-WuwaConnectAnimation` 才改父类；再次执行写操作前请先备份。

## 生命周期、线程和当前边界

- 更新 UObject 数据的代码只在游戏线程运行。纯计算函数没有 UObject 访问。
- 初始化、反初始化、换 Pawn/Movement 或数据源失效会清空旧快照；不会把上一角色的速度/起停状态带给新角色。
- CMC 通常先更新移动、Mesh 再更新动画，但根运动可在移动过程中触发动画更新。因此快照表示“采样时可用的数据”，不承诺永远是当前帧最终速度。
- `GetLastInputVector()` 是最近消费输入。停止 Tick、禁用移动、远端代理等情况还需要专门策略，不能把它当作永远实时的原始轴或网络同步输入。
- 当前验证单机；未实现联网 gait 复制、CMC SavedMove/预测、Root Motion 动作、完整起停状态机或步幅标定。
- `WalkRunMix`、`StepSizeMix`、播放速率需要结合真实动画素材另行实现，本次不填假公式。

## 验证

完整编译目标：`WuwaEditor Win64 Development`。自动化运行 `Wuwa.`，包含原先 Alt/路由回归，以及本次的采集、输入与惯性区分、Alt→动画上限同步、无效源复位、实例隔离、生命周期和真实资产父类/引用检查。

这些测试不等于手动 PIE 视觉验收；动画过渡是否自然仍需在接好状态机后观察。

本轮结果：完整编译成功，15 项自动化测试通过、0 失败、0 测试警告。报告在 `Saved/Automation/MovementAnimation/index.json`；实际 AnimBP 重设父类后的编译也为 0 错误、0 警告。
