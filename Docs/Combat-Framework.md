# 轻量战斗框架收尾与使用入口

当前范围是主技能管理、战斗类别裁决、语义输入、预输入、动画接招和条件选招。
同一攻击键的普攻/派生配置见 [输入转换规则](Combat-Input-Commands.md)。
伤害、命中检测和受击行为尚未实现。
这轮框架可以作为这些玩法的接入基础，不需要继续给同一份技能状态增加管理组件。

## 模块如何协作

```text
InputRouter → AbilityInputHandler → C# CombatInputRuntime
                                   │ 解析语义 Tag / 保存原始输入 / 选一个命令
                                   ↓
                                  ASC → GA.CanActivate / PreActivate
                                            ↓
                                       C# SkillComponent → FightState
                                            │                裁决战斗类别/持有句柄
                                            └→ 结束旧 GA，登记新 Skill

Montage Notify → 验证实际播放身份 → C# 更新 Skill 权限
                                  → CallAnimBreakPoint
                                  → OnAnimBreakPoint 事件
                                  → 输入 Handler 立即检查缓存
```

SkillComponent 不持有 Controller、Handler 或 Runtime。
Handler 订阅当前角色的技能事件，切换 Pawn、输入重置或退出时解绑。
通知只是标记动作允许做什么；选哪个技能仍属于输入层，GAS 仍执行最终激活检查。
移动占用、DesiredGait、冲刺欲望和 StopMontageForMovement 保留原来的职责。

## 四个阶段及建议阅读顺序

1. [FightState](Combat-System.md)：战斗类别、优先级与归属句柄。
2. [Skill 生命周期](Combat-Skill-Lifecycle.md)：旧 GA 真正结束之后才登记新主技能。
3. [动画接招与缓存](Combat-Input-Windows.md)：通知放在哪里、缓存多久、如何选择和消费。
4. 本文：同步断点、清缓存、最终验证及资源接入情况。

本轮核心代码入口：

- `Script/ManagedWuwa/Game/NewWorld/Character/Common/Component/Skill/WuwaSkillComponent.cs`
  的 `CallAnimBreakPoint_Implementation`、`RequestInputCacheClear_Implementation`。
- `Source/Wuwa/Private/Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.cpp`
  的 `BindSkillEvents`、`HandleAnimBreakPoint`、`HandleInputCacheClear`。
- `Script/ManagedWuwa/Game/NewWorld/Input/WuwaCombatInputRuntime.cs`
  的 `ReadContext`、`ProcessPendingInput`。

## GA 蓝图可直接调用的接口

| GA 中的节点 | 做什么 | 不会做什么 |
| --- | --- | --- |
| Set Skill Accept Input(true/false) | 改当前技能同级接招权限 | 不直接结束 GA |
| Set Skill Ready End(true/false) | 改当前技能让位标记 | 不直接停止蒙太奇 |
| Call Anim Break Point | 立即要求按当前权限检查一次预输入 | 不开放接招权限 |
| Clear Buffered Input(Tag) | 只清指定语义 Tag 的缓存 | 不松开按键，不改变 held 状态 |
| Clear Buffered Input(空 Tag) | 清当前角色全部预输入 | 不取消当前 GA |

若从 GA 手动定义接招阶段：先设置权限，再调用 `Call Anim Break Point`。
已有两个 C# 动画通知会自动按这个顺序执行，不需要再手动连第二次调用。
ReadyEnd 通知先设置标记和运行时优先级 0，最后才发断点；避免新技能已接管后再修改旧状态。

GA 上这些操作只使用本次执行持有的 SkillHandle。旧 GA、非主技能以及结束中的执行不能
借接口清除或改变新技能。返回 true 表示请求被接受，不表示一定选出了技能或激活成功。
InputHandler 自身也提供 `ClearBufferedInput(Tag)`，适合角色外的输入流程使用，返回删除数量。
角色切换和失焦仍用原来的 Reset/Flush 路径。

## 本阶段修正的两处时序

第一，旧版在窗口 Begin 时只记一个编号，等帧末再看权限；短窗口若已关闭，缓存就错过机会。
现在通过 OnAnimBreakPoint 同步消费。没有输入时不会结束 GA；有输入时正常走旧 GA 的
EndAbility 清理和新 GA 的开始。GAS ScopeLock 内仍延后，避免绕过引擎生命周期锁。
SkillComponent 的交接保护和 Runtime 的重入保护继续保留。

第二，仅看“当前 SkillHandle 有没有改变”无法发现两次检查之间已开始又结束的另一个技能。
现在快照增加 `SkillStartSerial`，每次成功开始主技能递增，结束到 None 后仍保留。
输入层据此清掉上一批输入，防止新动作结束后又冒出旧动作的预输入。

## 当前适配边界

- 输入仍走项目的 InputTag 系统；没有写死键盘键位。
- 当前缓存 Pressed；InputTag 可经有序条件规则选择不同技能。选中的技能身份 Tag 必须唯一对应一个已授予 GA。
- 同一个 Spec 在动作尚未结束时重放需要 InstancedPerExecution。
- 两个正式技能通知放在 Montage 自身轨道、开始混出前；不支持底层 Sequence 通知。
- 这是本地框架。多技能组、联网预测及原作完整输入层尚未实现。
- FightState 中有受击等类别和规则，不代表已有受击动画、扣血或击退行为。

## 当前资源与验证

同键选招已经接入，配置和派生扩展方法见 [Combat-Input-Commands.md](Combat-Input-Commands.md)。
角色持有 `DA_Changli_InputCommands`；IA_Attack 发送 `Input.Combat.Attack`，由有序规则选择
`Abilities.Skill.Attack01` 至 `Attack05` 的唯一 GA。Dash 继续使用原来的直接身份映射。

2026-09-27：原生 Editor 编译、ManagedWuwa 发布、39 项集成/回归测试全部通过。
报告：`Saved/Automation/CombatInputCommands/index.json`。
保存后的真实配置已在新 UE 进程中重新加载验证：
`Saved/Diagnostics/CombatInputCommands/SetupAudit.json`。

`AM_Attack01` 的蒙太奇通知轨道目前为空。条件规则只选招，同级技能的接招时机仍需配置
`AnimNotifyState_SkillAcceptInput`，或由当前 GA 手动设置权限并触发断点。
本轮没有替用户决定各段的接招帧，也没有验证实机动画手感。

阶段 4 的历史测试报告保留在 `Saved/Automation/CombatStage4/index.json`。
旧版 GA_Attack/AM_Attack_1 接入步骤已被上述 Attack01～05 的配置取代。
