# 相机系统：第一版

## 运行链路

每个本地玩家由 UE 原本就会创建的 `AWuwaPlayerCameraManager` 持有一个 `UWuwaCameraRuntime`。不需要关卡中摆放相机系统 Actor，也没有相机专用的 ActorComponent。

```text
Enhanced Input（IA_Look / IA_CameraZoom）
  → InputRouter（Input.Route.Camera）
  → WuwaPlayerCameraManager
      旋转 → AddYawInput / AddPitchInput → ProcessViewRotation → ControlRotation
      缩放 → 累计本帧的语义增量

UE 更新玩家视角
  → 原生边界检查玩家、Pawn、脚本运行时
  → FWuwaCameraFrame：位置、ControlRotation、DeltaSeconds、ZoomDelta
  → C# WuwaCameraRuntime
      选择最高优先级的有效模式
      → WuwaCameraInputController：玩家缩放偏好、距离限制
      → WuwaFightCameraLogic：当前参数、模式混合、最终位置和旋转
  → FWuwaCameraView
  → 原生相机碰撞（ECC_Camera 球形 Sweep）
  → UE 的相机 Modifier / Camera Shake / 最终 POV
```

控制器与脚本之间传值。输入与计算代码不查找 Character、DisplayComponent 或 CameraComponent。C# 没有独立 Tick，由相机管理器统一驱动。普通鼠标旋转继续经过项目启用的 UE 输入缩放，滚轮增量只消费 `Triggered`，不重复消费 `Started`，也不乘帧时间。

## 配置和主要文件

- `Source/Wuwa/Public/Game/Camera/WuwaCameraTypes.h`：原生与脚本共享的数据。
- `Source/Wuwa/Private/Game/Camera/WuwaPlayerCameraManager.cpp`：生命周期、输入入口、世界碰撞、相机输出。
- `Source/Wuwa/Public/Game/Camera/WuwaCameraRuntimeBridge.h`：C# 反射边界，没有第二套原生策略。
- `Script/ManagedWuwa/Game/Camera/WuwaCameraRuntime.cs`：组织模式、输入和位置计算。
- `WuwaCameraInputController.cs`：缩放偏好。正滚轮值拉近。
- `WuwaFightCameraLogic.cs`：相机位置、缩放平滑、模式过渡。
- `WuwaOwnedRequests.cs`：泛型来源请求容器，集中处理有效性、优先级、句柄释放。
- `WuwaCameraSettingsUtility.cs`：集中检查配置中的非法数值和范围。

编辑 `/Game/Game/Camera/DA_Camera_Gameplay` 可调整默认距离、缩放上下限、缩放步长、平滑速度、俯仰限制、中心偏移、FOV 和碰撞探针。

默认距离 400 cm、范围 150–800 cm、每格 50 cm、FOV 90。中心是 Pawn 原点，默认没有额外高度偏移，保持原先 SpringArm 附在胶囊体上的位置关系。`PivotOffset` 是世界轴偏移。`BlendTime` 是进入所选配置的过渡时间；恢复基础配置时使用基础配置的过渡时间。

`BP_WuwaPlayerCameraManager` 是 UE 真正使用的相机管理器类型，其 `DefaultMode` 指向上述配置。`BP_WuwaPlayerController` 的 Player Camera Manager Class 已指向这个蓝图。无须手工调用 ActivateForPlayer 或 SetViewTarget 来启动普通跟随。

## 移动为何保持原逻辑

Character 不再创建 CameraBoom 和 FollowCamera。三处移动方向计算（移动输入、实际移动、GA 输入快照）都调用 `GetCameraRelativeMoveDirection()`，取 `ControlRotation.Yaw` 生成前、右方向，仍然忽略 Pitch，并保留原来的归一化行为。攀爬和步态规则没有移入相机系统。

普通跟随相机的旋转就是 ControlRotation。技能镜头的 `RotationOffset` 和 Camera Shake 属于画面效果，不会偷偷改变操作方向。如果以后某个模式要求“操作方向也跟着演出镜头转”，应显式增加该模式的操作朝向策略，而不是让移动读取震动后的最终相机。

保留旧 `Vector2ToCameraDirNormalized(axis, camera)` 作为废弃兼容入口；项目内部不再使用相机组件参数。切换 Pawn 时清理旧输入、镜头请求和当前插值状态。窗口失焦只清理尚未消费的输入，不改变玩家缩放偏好。

## 大招和模式切换

第一版已有可执行的参数模式接口。`PushCameraMode(Source, Mode)` 返回正整数句柄，失败返回 0。优先级最高的请求生效；同优先级取最后提交者。`PopCameraMode(Handle)` 只删除对应请求，重复释放不影响别人的镜头。来源对象失效时会自动清理请求。切换 Pawn 清空请求，但不循环使用旧句柄。

蓝图 GA 示例：

```text
ActivateAbility
  → Get Player Camera Manager（该技能所属的本地控制器）
  → Cast WuwaPlayerCameraManager
  → PushCameraMode(Self, DA_Camera_SkillExample)
  → 将返回的句柄保存到这次技能实例

EndAbility（正常结束、取消、受击打断都走这里）
  → PopCameraMode(保存的句柄)
  → 清空本次保存的句柄
```

示例配置的 `UseUserZoom=false`、`AllowZoomInput=false`、`AllowRotationInput=false`：演出期间使用配置距离并锁住相机输入，玩家之前的缩放偏好仍保存在输入控制器中。结束后恢复仍有效的下层模式，最终恢复普通跟随。不要把句柄存在共享 AnimNotify 或配置资产上；放在 GA 实例或独立的动作上下文中。

这是参数镜头的入口，并未把示例自动接到现有任何 GA。以后可以在这一层增加随时间变化的偏移、FOV 曲线、锁定目标等策略，依然输出同一个 `FWuwaCameraView`。

完整 Sequencer 演出走 UE 的独立 CameraActor / CineCameraActor 视角。当前管理器仅接管本地控制 Pawn 的普通视角，外部相机仍使用引擎原有计算和 `SetViewTargetWithBlend`。返回时切回玩家 Pawn。Sequence 的播放、抢占、取消与返场生命周期应由未来的演出控制器管理；本版没有实现 Sequence 播放器。相机输入在外部视角期间不累计，回到普通视角不会重放滚轮。

## 与鸣潮的对应和差异

鸣潮的 `FightCamera` 是脚本 Entity，包含脚本 Logic / Display；Display 创建真实 CameraActor。它的 Logic 维护 DesiredCamera / CurrentCamera，并组织输入、锁定、修改、碰撞等控制器。

本项目保留脚本组织策略、玩家缩放偏好、目标与当前参数、输入锁定、优先级和实例句柄这些职责。引擎输出由已有 PlayerCameraManager 承担；不再复制 Entity 到 UE Actor+两个组件。这是针对当前项目做的适配，不宣称与库洛源码逐行一致。

本版的模式请求支持恢复下层请求，是项目采用的规则。鸣潮 CameraModify 的单个修改实例替换规则与其全局相机模式选择是两层概念，不应把两者说成同一个通用栈。

## 范围与验证

第一版包含：角色跟随、保持原输入缩放的旋转、俯仰限制、滚轮限幅与平滑、基础碰撞、参数模式进入退出、输入及 Pawn 生命周期清理。相机碰撞只做基础球形 Sweep，还没有库洛的遮挡透明、复杂避障或碰撞恢复曲线。未包含锁定、攀爬专用镜头和技能动画曲线。

新增 `Wuwa.Camera.*` 自动化测试覆盖真实 UnrealSharp 运行时调用和资产配置；原有 `Wuwa.Input.PlayerState.*` 验证去掉 CameraComponent 后的移动快照。构建验证采用独立 .NET 构建及 UE 原生构建，不以编辑器增量热重载成功作为依据。

2026-09-26 验证结果：C++ 编辑器构建成功，独立 C# 构建 0 错误、0 警告；19 项 UE 测试按各自最后一次运行结果全部通过，包括实际 `/Game/Map/Wuwa` 的 PIE 启动、语义相机输入、模式进出、移动快照、步态/冲刺以及 GA 状态检查。另有纯计算辅助测试 32 条断言通过。PIE 使用 `-nullrhi`，验证了运行链路，没有代替实际画面的手感检查。

测试报告在 `Saved/Diagnostics/CameraFrameworkBuild/Verified`；其中三项相机输入测试修正夹具后单独重跑，最后结果在 `InputVerifiedFinal`，三项均无错误、无警告。旧的移动状态测试夹具遗漏了输入标签、路由标签和来源 Action，本次补齐了这些测试字段，生产步态规则未修改。

实际地图仍有非相机警告：输入表第 5 行是空占位行（已与修改前备份核对，原本就存在），`WBP_HPBar` 初始化出现除零，以及 GameplayCue 搜索路径未配置。本版没有顺带修改这些功能。之前 UnrealSharp 增量编译器的 Roslyn 异常也不属于本次修复范围；这里验证的是独立构建后的实际运行。

资产迁移命令 `-run=WuwaCameraSetup -Apply` 仅处理列出的输入、相机配置、控制器和角色蓝图，修改前将旧文件备份到 `Saved/Backups/CameraSetup`。不带 `-Apply` 只输出检查结果。
