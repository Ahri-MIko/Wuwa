# 尚无足够字段证据的鸣潮 EffectModel

本轮补充数据模型时，有 3 种原生类型只能确认其存在，尚不能还原其可配置字段。没有为它们创建可实例化的空类，也没有把名称相似的 UE 类型当作原作实现。

这不代表这些原生类没有属性。`UsedInfo` 和 `PuertsWrapper` 记录的是脚本绑定所需的内容，并非完整的原生反射声明。

| 类型 | 原生类路径 | 脚本绑定导出 |
|---|---|---|
| CurveTrailDecal | `/Script/KuroGameplay.EffectModelCurveTrailDecal` | `IsWrapper=false`；`Properties`、`Functions` 均为空 |
| NDC | `/Script/KuroGameplay.EffectModelNDC` | `IsWrapper=false`；`Properties`、`Functions` 均为空 |
| SequencePose | `/Script/KuroGameplay.EffectModelSequencePose` | `IsWrapper=false`；`Properties`、`Functions` 均为空 |

## 已核实的本地证据

共同导出根目录：

`C:/GamePakExtractor/Output/Exports/Client/Content/Aki/`

- [PuertsWrapper.json](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/RunTimeLibs/PuertsWrapper.json:33010)：CurveTrailDecal 记录始于 33010 行；NDC 始于 33279 行；SequencePose 始于 33485 行。
- [UsedInfo.json](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/RunTimeLibs/UsedInfo.json:28917)：`NativeClassArray` 内这 3 类的 `Properties` 和 `Functions` 为空。
- [EffectModelCurveTrailDecal.json](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/TypeScript/Game/Render/Effect/Data/EffectModelCurveTrailDecal.json)：确认 `EffectModelCurveTrailDecal_C` 的 `SuperStruct` 为上述原生类；导出的默认对象没有 `Properties`。
- [EffectModelNDC.json](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/TypeScript/Game/Render/Effect/Data/EffectModelNDC.json)：确认 `EffectModelNDC_C` 的原生父类；默认对象没有 `Properties`。
- [EffectModelSequencePose.json](C:/GamePakExtractor/Output/Exports/Client/Content/Aki/TypeScript/Game/Render/Effect/Data/EffectModelSequencePose.json)：确认 `EffectModelSequencePose_C` 的原生父类；默认对象没有 `Properties`。
- 对应 `TypeScript/Game/Render/Effect/Data/*.cpp` 只包含派生类声明，没有原生字段定义。
- 对应 `JavaScript/Game/Render/Effect/Data/*.js` 只是继承原生类的空包装；当前 `JavaScript/Game/Effect` 导出中未找到这 3 类的脚本 Spec。
- 当前 `Aki/Effect` 下已导出的 JSON 中，未找到以这 3 种类型存储实际配置的 EffectModel 对象。名称带有 `CurveTrailDecal` 的材质实例不能替代 Model 配置证据。

## 后续需要导出什么

优先在 FModel 中导出**实际使用这 3 类的 DataAsset 实例的 JSON 属性**，类型可能显示为原生类名，或者带 `_C` 的包装类名：

| 目标 | 要寻找的实际对象类型 |
|---|---|
| 曲线拖尾贴花 | `EffectModelCurveTrailDecal` 或 `EffectModelCurveTrailDecal_C` |
| NDC 特效模型 | `EffectModelNDC` 或 `EffectModelNDC_C` |
| 姿态序列特效模型 | `EffectModelSequencePose` 或 `EffectModelSequencePose_C` |

在包含这些子项的 `EffectModelGroup` 中，可以沿 `EffectData` 引用查找对应子资源。目标是带具体资源名称、配置值及资源引用的对象，不是 `Default__EffectModel…_C` 或 `TypeScriptGeneratedClass`。

每类最好提供两份不同配置的实际资产 JSON，并附上其所引用的专有数据资产 JSON；这样能够确认字段形状、枚举表示、引用类型和嵌套结构。只有一份资产时，未出现的默认值字段仍可能缺失。若 FModel 不能解析字段，还需要与游戏版本匹配的属性映射信息；重复导出当前包装类不会增加原生字段证据。

可以同时保留 `.uasset/.uexp` 作为研究资料，但这些 Cooked 资源和库洛专有类不能直接当作本项目可用的编辑器资产。

## 已实现模型中的专有后端边界

- `WuwaEffectModelMaterialController` 保存 `MaterialControllerData` 与 `MaterialControllerGroupData` 的资源软路径。字段见原作 `EffectModelMaterialControllerSpec.js`；相关角色通知使用 `PD_CharacterControllerData_C` 和 `PD_CharacterControllerDataGroup_C`。当前项目尚无对应角色材质控制器执行后端。
- `WuwaEffectModelGpuParticle` 保存 `Data`、变换、循环、倒放、往返及时间倍率字段。原作使用的是 `KuroGPUParticleComponent`，不是 Niagara GPU 模拟开关；`Data` 的具体原生资源类型在当前导出中不可确认，因此保留为软路径。
- `WuwaEffectModelMultiEffect` 保存重复子 Model 与 BuffBall 参数。原作脚本可确认 `Type == 0` 使用 `MultiEffectBuffBall`；`BaseNum` 参与随播放时间增长的期望数量计算，不能解读为固定总数量。当前仅建立数据结构，数量与位置算法仍属于后续执行层。

这几个模型的数值默认值是项目选择，不宣称与原作 CDO 完全一致。它们可以配置、序列化并作为 Group 子项引用；这不等同于专有播放后端已经实现。
