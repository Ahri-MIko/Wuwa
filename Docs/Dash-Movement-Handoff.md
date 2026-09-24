# Dash 移动取消与根运动交接

`UWuwaGameplayAbilityBase::StopMontageForMovement(BlendOutTime)` 用于已经获准的移动取消。
它只关闭本 GA 当前蒙太奇播放实例的根运动提取，然后经 ASC 开始混出。不会修改
AnimBP 的 Root Motion Mode、角色 Velocity、蒙太奇资产或 Run_End 的根运动。

## 蓝图接法

```text
取消窗口内持续检查（包含进入窗口时已有的移动输入）
  → Stop Montage For Movement（Blend Out Time 默认 0.1）
  → 返回 true：End Ability
  → 返回 false：继续当前动作
```

这个方法不判断取消窗口，不能从动作开始就无条件轮询。已有窗口打开/关闭逻辑仍由
Dash GA 管理；进入窗口时检查一次，窗口内输入发生变化后继续检查。不要只在单个
通知到达时检查一次，否则错过“窗口开启后才按方向”的输入。

自然完成继续走播放任务的 `OnCompleted → EndAbility`。受击、死亡和其他技能打断
不应无条件套用移动取消入口。无输入时本方法返回 false，保留蒙太奇和根运动。
如果已有播放回调会结束 GA，不要在多个回调里重复安排清理。

## 为什么需要单独交接

UE 5.7 的 Everything 路径可能在正数混出的最后一帧仍把零权重根运动加入提取队列。
这份位移虽然是零，但 `bHasRootMotion` 仍为 true，CMC 会跳过普通加速，并用根运动
覆盖速度。下一帧才恢复输入移动，就形成先归零再加速的现象。

本方法在 Stop 之前调用实例的 `PushDisableRootMotion()`，避免这次播放在混出时
继续提供根运动。姿势仍可混出，普通移动则接管速度。禁用属于已经确定停止的实例，
直到它销毁；不要在 GA End 时立即 Pop 恢复提取。重播相同资产会创建新的实例。
也不要将 Root Motion Scale 设为零或清空整个提取队列：前者不是结束根运动控制，
后者可能丢弃其他动画或当前帧已经提取的合法位移。

## 原版 JavaScript 中确认的流程

以下文件位于 `C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/Game/`。
这是对已有导出代码的解读；本项目的实例禁用方法不是已还原的库洛原生实现。

- `AnimNotify/TsAnimNotifyEndSkill.js`：设置 `IsMainSkillReadyEnd=true`、当前优先级 0，
  调用 `CallAnimBreakPoint()`；不会在通知函数本身直接停止蒙太奇。
- `NewWorld/Character/Common/Component/CharacterMoveComponent.js`：
  `UpdateInputOrder → PlayerMovementInput` 持续处理方向输入。满足全身技能标记、
  输入许可、`IsMainSkillReadyEnd`、非零方向及其他标签限制后，调用
  `StopGroup1Skill("移动打断技能")`，随后地面分支继续 `D_AddMovementInput`。
- `NewWorld/Character/Common/Component/Skill/BaseSkillComponent.js`：根据技能模式结束
  ActiveAbility 或请求停止技能蒙太奇。`CallAnimBreakPoint()` 发出打断点事件，
  输入组件另行尝试消费缓存技能输入，和持续方向输入取消是不同的处理入口。
- `NewWorld/Character/Common/Component/CharacterAnimationComponent.js`：查询
  `ShellAnimInstance.HasKuroRootMotionAnim()`；部分根运动处理仍在库洛原生代码中。

已查看的这条移动取消/技能清理调用链没有固定设置 RunSpeed 的补速代码。
`OverrideTerminalVelocity=0` 清除的是终端速度覆盖配置，不是 `Velocity=0`。

## 使用范围与验证边界

当前用于本地 Dash 验证。ASC 的停止仍走标准入口，但额外的实例根运动禁用计数不自动
参与网络预测或复制；联网需要为服务端/预测端接入一致的取消处理。

本方法在调用时不恢复一个已经归零的速度。如果输入取消以前已经播到刹停尾段，或
底层状态机正在播放根运动停步，仍需分别检查。它不应通过强设跑速来掩盖这些情况。

源代码增加了新蓝图函数，当前已打开的编辑器需要重新编译并加载模块后才能显示节点。
本次没有修改或覆盖打开的蓝图资产，移动取消分支需要按上面的流程连接。

## 本次验证结果

独立验证项目已通过 UE 5.7 Development Editor 编译，以及
`Wuwa.Ability.MontageMovementHandoff` 自动化测试。测试读取现有长离前闪资产，验证：

- 无移动输入、播放归属错误或非法混出时间时，不停止动作或改变根运动。
- 接受移动取消后，实例仍有姿势权重，但不再提取根运动，包括混出结束帧。
- 交接函数不会直接修改角色速度或全局 Root Motion Mode。
- 重新播放同一资产时，新实例正常提取非零根运动位移。

这是蒙太奇实例提取机制的回归测试，没有运行当前蓝图的完整 Slot、状态机和 CMC
更新流程；不能代替接线后的 PIE 速度曲线验证。测试出现一条项目未配置
GameplayCueNotifyPaths 的警告，测试结果为 Success。
