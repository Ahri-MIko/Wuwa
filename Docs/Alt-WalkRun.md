# Alt 走跑切换：先看这三个函数

本次只打通走跑切换。参考原作“判断输入上下文 -> 返回 SwitchWalk 命令”的分工；当前地面许可和速度是 Demo 规则，不是原作参数的完整还原。

## 按这个顺序阅读

1. `Source/Wuwa/Private/Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.cpp` 的 `ResolveCommand`：只在走跑输入的 `Pressed` 且允许切换时产生 `SwitchWalk`，不修改角色。
2. `Source/Wuwa/Private/Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.cpp` 的 `ExecuteInputCommand`：再次检查许可，然后切换唯一的 `DesiredGait`。
3. 同一文件的 `GetMaxSpeed`：Walk 使用 `WalkSpeed`（默认 200 cm/s），Run 沿用角色已有 `MaxWalkSpeed`。没有直接改写实际速度，也没有覆盖跑速配置。

`FWuwaInputCommand` 在 `Input/WuwaInputCommand.h` 中，本阶段只有 `Type`；不要为了尚未实现的动作提前添加大批字段。

## 已接好的实际输入

- `IMC_Character`：`IA_WalkRun` 原来的 LeftControl 映射迁移到 **LeftAlt**。
- `DA_InputActionTagAsset`：`IA_WalkRun` -> `Player.Common.Movement.WalkRun` -> `Input.Route.Movement`。
- Controller 的已有 `Started` 绑定打包 `FWuwaInputEvent`，由 Router 交给 MoveInputHandler。
- Handler 每次取 Controller 当前的 Pawn，不缓存旧角色。
- 按住、松开、取消都不会额外翻转；空中、攀爬、游泳、飞行、蹲伏和禁用移动状态拒绝切换。
- 地面静止时也能切换，它只改变下次移动使用的走跑策略，不会让角色自行移动。

## 清理与保留

- 已移除旧 `UWuwaInputComponent` 对 WalkRun 的委托注册，避免同一 IA 再走旧通路。旧反射属性保留并标记弃用，以免直接破坏已有蓝图资产；不要再接旧 `OnWalkRun`。
- 已修复 Move/Look 同时消费 Started 和 Triggered 的首帧重复问题。
- 已修复 Character BeginPlay 先解引用空 Controller 再判断的问题。
- 没有删除其他动作的旧绑定、重写攀爬或改技能系统；它们不属于本次迁移。
- 没有删除已有动画数据类型或重做 AnimBP。`UWuwaAnimInstance` 已在更新时读取 Movement 的步态；如果现有动画蓝图尚未消费该数据，速度切换仍然生效，但走路动画不会自动接好。
- 本次验证单机走跑切换，未实现多人步态预测/同步。

## 你可以这样验证

打开项目运行，确保角色在地面：按一次左 Alt 后移动，再按一次恢复跑步。Output Log 会出现 `[CommonMove] SwitchWalk -> Walk/Run` 和速度上限。按住或松开不应反复打印。跳起或攀爬时按 Alt 不应切换。

自动化测试：`Wuwa.Input.WalkRun`（输入解析、命令执行、真实资产配置、Router 到当前角色的集成测试），以及已有 `Wuwa.Locomotion` 回归测试。

本次验证：UE 5.7.4 Development Editor 完整编译通过；8 个自动化测试全部通过，0 个失败、0 个测试警告。报告位于 `Saved/Automation/AltWalkRun/index.json`。集成测试使用临时世界，不代替手动 PIE 的键盘和动画表现验收。

本次两个输入资产修改前的备份位于：`Saved/Backups/AltWalkRun-20260917-101940/`。`Scripts/ConfigureWalkRunInput.py` 默认只读，只有显式传入 `-WuwaApplyWalkRun` 才保存这两个资产；它是一次性编辑器配置工具，不参与运行时输入。
