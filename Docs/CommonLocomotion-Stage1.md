# Common 移动：第一阶段

> 以下保留为第一阶段的历史说明。Alt 输入现已接入统一路由，见 [Alt-WalkRun.md](Alt-WalkRun.md)；最新动画采集结构和 AnimBP 接入见 [Movement-Animation.md](Movement-Animation.md)。不再需要用蓝图手动调用 ToggleWalkRun 接按键。

本阶段只建立游戏侧步态策略和动画数据入口，不修改现有动画蓝图、输入绑定或 GAS 动作。

## 与导出资源的对应关系

- 已确认：`ABP_BaseRole` 读取 `KuroAnimInstanceRole` 的运动/步态变量；起步、步行、跑、跑停有独立组织。
- 已确认：原资源有分方向混合结构 `VeloctiyBlend`、`WalkRunMix`、`StepSizeMix`、`GroundMovePlayRate`。
- 本项目实现：`UWuwaAnimInstance` 提供 `LocomotionData`；`FWuwaVelocityBlend` 表达方向权重。
- 尚未还原：原作变量的计算、滤波、动画步幅标定和取消窗口公式。当前方向权重算法、阈值、200/900 cm/s 的走/冲刺速度都是可调 Demo 初值。
- 本阶段不生成 `WalkRunMix`、`StepSizeMix` 或播放速率：需先核对自己的动画/BlendSpace 参数，不能用猜测值冒充原作结果。

## 数据所有权

1. `UWuwaMovementComponent` 持有 `DesiredGait` 和 Sprint 许可。`AllowedGait` 是当前允许的速度策略，不是按速度反推出的动画状态。
2. Run 沿用已有 `MaxWalkSpeed`。Walk/Sprint 通过 `GetMaxSpeed` 生效，保留原有攀爬、蹲伏、游泳等速度规则。
3. `UWuwaAnimInstance::NativeUpdateAnimation` 在游戏线程读取移动结果，生成一次动画更新的快照；AnimBP 只读快照。
4. 第一阶段曾通过 CMC 控制加速度估算移动意图；当前已改成独立采集最近消费的输入向量，不再从加速度反推。输入方向、加速度、实际速度分开保存，见最新动画文档。
5. `bIsGoingToMove`、`bWantsToStop` 只是简化的起停候选，结合上一帧速度状态保留一帧内起步/停稳的信号。它们不是完整的动作状态，不代表可以取消闪避/攻击；传送、受击和根运动等还需要动作上下文。
6. `Local` 均为 Actor 局部空间，不是带模型朝向修正的 Mesh 空间。角色切换/预览无有效 Pawn 时快照复位。

## 编辑器接入（尚未自动执行）

1. 编译成功后，新建测试用动画蓝图，以 `WuwaAnimInstance` 为父类、使用自己的角色骨骼。不要直接覆盖现有自定义 C++ 动画父类。
2. 在 AnimBP 中读取 `LocomotionData` 并拆分结构体；PIE 选中实际运行实例作为调试对象。
3. 检查：静止 GroundSpeed 约 0；移动时 LocalVelocity/LocalAccel 变化；跳跃时 bStateAir 为真；攀爬时 bStateClimb 为真。
4. 验证步态接口时，对角色的 WuwaMovementComponent 调用 `ToggleWalkRun`。正式按键绑定留给下一阶段的统一路由，动画蓝图不要查询 Alt。
5. Sprint 需要同时设置期望 Sprint 并开放许可；本阶段不开放任何真实闪避后冲刺窗口。
6. 下一阶段保留 Walk/Run 的独立表现，建立起步、跑停子状态机，并标定动画混合参数。

## 范围限制

- 不新增输入处理器，不重复注册 IA，不自动变更任何 `.uasset`。
- 未实现联网 gait 的复制、CMC saved move/预测和远端输入意图重建。当前仅验证单机数据通路。
- AnimBP 工作线程只能使用安全采集后的数据；不要直接在 Thread Safe Update 内访问角色或 ASC。

## 自动化验证

在 UE 的 Session Frontend / Automation 中运行 `Wuwa.Locomotion`：

- `VelocityBlend`：零速、四方向、对角线与垂直速度隔离。
- `MovingThreshold`：速度进入/退出阈值和低速滞回。
- `GaitPolicy`：默认速度保持、走跑切换、Sprint 许可、攀爬/禁用移动回归。
- `TransitionSignals`：一帧内起步/停稳不丢信号，持续移动不重复起步，空中不触发地面起停。
