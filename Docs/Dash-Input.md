# Dash 读取移动输入

在继承 `WuwaGameplayAbilityBase` 的 Dash 蓝图中：

```text
ActivateAbility
  → Get Player Input State
  → Break Wuwa Player Input State
  → Has Move Input 为 false：选后闪蒙太奇
  → Has Move Input 为 true：选前闪蒙太奇，Move World Direction 是输入世界方向
```

`Move Axis` 保存最近收到的移动轴（X 左右、Y 前后，保留摇杆幅度），`Has Move Input` 按 Character 的 `Move Input Threshold` 判断。`Move World Direction` 按读取时的相机 Yaw 转为水平单位方向。无输入时方向为零；后闪方向由 Dash 自己使用角色朝向决定。

Character 的 `GetPlayerInputState()` 是其他模块的公共入口；GA 的同名节点只是通过自己的 Avatar 调用它。返回的是值快照，持续检查输入时重新调用。现有 `MoveInput` 缓存仍是输入来源，不再维护第二份可写缓存；`HandleMoveInput` 在 `CanApplyMove()` 前更新它，因此移动许可不会阻断意图记录。Released/Canceled 的归零继续使用现有 Controller 路径。

鸣潮导出参考：`C:/GamePakExtractor/Output/Exports/Client/Content/Aki/TypeScript/Game/NewWorld/Character/Common/Blueprint/Utils/TsMoveBlueprintFunctionLibrary.cpp` 包含 `GetInputDirect` 和 `HasMoveInput` 接口，但函数体为空壳。这里借鉴公共查询方式，结构体、归属和阈值是本项目实现，不能称为恢复了原作内部代码。

范围：本地、最新已处理的移动输入；不是全按键表、输入缓存队列或联网同步。现有输入事件处理顺序不变，同帧 Move 与 Dash 的回调顺序仍会影响首次读值；现有切人输入委托生命周期未在本次改造。GA 缺少角色上下文时返回零值。在 C++ `CanActivateAbility` 中读取输入，应使用传入的 `ActorInfo->AvatarActor` 找到 Character 再查询，不使用依赖实例上下文的无参 GA 节点。

验证：为避免对已打开的编辑器进行结构体热重载，使用 `Saved/Verification/DashInputReadback` 中与本次源码一致的临时项目完成 `WuwaEditor Win64 Development` 编译；`Wuwa.Input.PlayerState` 两项自动化测试全部通过，测试内无警告或错误。报告为该目录下 `TestReport/index.json`。未修改 Dash 蓝图连线，也未将新模块加载到当前编辑器；保存并关闭编辑器后，编译原项目再打开即可使用新增节点。
