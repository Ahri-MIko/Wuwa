> 历史审计说明：本文保留导出依据和旧实现记录。旧刀光运行链路及相关 commandlet 已于 2026-10-03 移除，当前状态与保留资源见 [刀光停用说明](../../Slash-FX.md)。下文中的运行、导入和预览命令不再适用。

# 长离第一段刀光：原资源与重建边界

本次更新：2026-09-29（UTC）。检查目录：`C:/GamePakExtractor/Output/Exports/Client/Content/Aki`。

第一段所需的 **4 份 PSKX、17 张实际 PNG、6 个叶材质实例和 4 个直接父材质实例 JSON 已取得**。此前“缺少模型与对应图片”的结论已过期。白色默认纹理仍使用单独生成的纯白占位图；基础 Material 图与 Niagara 完整模块运算尚未恢复。

现有方案把这些原网格和图片导入可编辑 UE 5.7 资产，重建材质与 Niagara，只更新 `Attack01`。`Attack02`～`Attack05` 仍保留旧程序化效果。使用与导入命令见 [刀光说明](../../Slash-FX.md)。

## 已确认的第一段引用链

```text
DA_Fx_Group_R1a01_01_N_Dg
  → DA_Fx_R1a01_01_N_Dg
  → NS_Fx_Changli_R1a01_N_Dg

DA_Fx_Group_R1a01_01_N_Dg1
  → DA_Fx_R1a01_01_N_Dg1
  → NS_Fx_Changli_R1a01_N_Dg1

DA_Fx_Group_R1a01_01_N1_Dg
  → DA_Fx_R1a01_01_N1_Dg
  → NS_Fx_Changli_R1a01_N1_Dg
```

| Niagara System | 实际非空 Renderer |
|---|---|
| `NS_Fx_Changli_R1a01_N_Dg` | 4 个 Mesh |
| `NS_Fx_Changli_R1a01_N_Dg1` | 2 个 Sprite |
| `NS_Fx_Changli_R1a01_N1_Dg` | 2 个 Mesh |

N1_Dg 的 BG/Liang 虽有启用的 emitter handle，但 RendererProperties 为 null，不应生成额外可见网格。N_Dg1 的 Trail_Wing 被禁用，不能把其活动效果称为 Ribbon。第一段四条纳入重建的原通知使用这三个系统，其中 N_Dg1 被放置两次。

三个组的 `EffectData` 各有一项，Value 为 0；结合已查看的 `EffectModelGroupSpec.OnInit/OnPlay`，没有组内额外延迟。`StartTime=2` 不能解释为等待两秒才生成粒子，也不能直接推导粒子寿命。

N_Dg1 子 DA 的 `Rotation.bUseCurve` 为 true，`Curve[2]` 含三个内嵌三次插值关键点：`(-0.0027577877,16.835491)`、`(0.18540506,39.27856)`、`(0.9972422,46.85684)`，切线也已导出。当前将其映射为局部 yaw，并换算时间与切线；数据来自原资源，具体轴向映射属于重建选择。引用和原始属性见 `EffectGroupReference.json`。

## 已得到的几何与图片

以下路径相对于 `Client/Content/Aki/`。四个网格已具有实际 PSKX，不再只有属性 JSON：

| 网格 | 顶点 / 三角形 | 源顶点色 |
|---|---:|---|
| `Effect/Mesh/Changli/Mod_Changli_Daoguang_140003_a3` | 115 / 176 | 有，alpha 含渐变 |
| `Effect/Mesh/Changli/Mod_Changli_Daoguang_140004` | 93 / 120 | 无，导入使用白色 |
| `Effect/Mesh/Changli/Mod_Changli_Daoguang_140005` | 186 / 240 | 有，alpha 含渐变 |
| `Effect/Mesh/Changli/Mod_Changli_Zhuan_140001` | 85 / 128 | 有，alpha 含渐变 |

每份网格都有一个 UV 通道和导出法线。当前文件逐项验证了 point 与 wedge 索引一一对应。按照 CUE4Parse 对应格式的写出逻辑，预处理恢复 Y 轴镜像及交换的面索引，UV 保持原值，颜色按 RGBA 字节读取；恢复后的包围盒与原 UE 元数据最大误差小于 `0.000005 cm`。这验证的是预处理数据，UE 构建后的渲染数据仍需另行检查。

17 张实际 PNG 已原样复制到 `ArtSource/ChangliSlash/Textures`：

```text
Effect/Texture/Color/T_Color_009
Effect/Texture/Color/T_Color_140009
Effect/Texture/Color/T_Color_40010
Effect/Texture/Color/T_Color_80007
Effect/Texture/Daoguang/T_Daoguang_40001
Effect/Texture/Mask/T_Mask_30013
Effect/Texture/Mask/T_Mask_30054_2
Effect/Texture/Mask/T_Mask_30157
Effect/Texture/Normal/T_Normal_011
Effect/Texture/Particle/T_Particle_002
Effect/Texture/Tile/T_Tile_046
Effect/Texture/Tile/T_Tile_19_Vec
Effect/Texture/Tile/T_Tile_300186
Effect/Texture/Tile/T_Wenli_20002
Effect/Texture/Wenli/T_Changli_140004_wings
Effect/Texture/Wenli/T_Changli_140006_wings_Trail
Effect/Texture/Wenli/T_Wenli_Changli_140002_3
```

第 18 项 `Render/Common/T_DefaultColorWhite_D` 仍没有原 PNG，recipe 明确记录了生成纯白占位图。不要把这张占位图算作原贴图。

贴图通道必须独立保留。例如 `T_Changli_140006_wings_Trail` 的 alpha 几乎全白，羽毛轮廓主要在 RGB；统一用所有贴图的 alpha 作透明度会得到矩形底片。sRGB、纹理寻址和采样公式在缺少原属性或母材质图时仍是重建设置，不能单凭 PNG 文件名确定。

`ImportedGeometry.json` 保存本轮几何/图片索引与校验数据。完整几何 JSON 已复制到 `ArtSource/ChangliSlash/Geometry`，无需 UE 直接导入 PSKX。

## 材质父链已推进到基础 Material

四个直接父材质实例 JSON 已取得：

```text
Render/MaterialInstance/Effect/MeshParticle/MI_AdvancedEffect_MP
Render/MaterialInstance/Effect/MeshParticle/MI_Refraction_MP
Render/MaterialInstance/Effect/MeshParticle/MI_SimpleEffect_MP
Render/MaterialInstance/Effect/Sprite/MI_SimpleEffect_S
```

它们继续指向以下基础 Material；当前仍缺可恢复计算图的资料：

```text
Render/Shaders/Effect/EffectMaterial/MeshParticle/M_AdvancedEffect_MP
Render/Shaders/Effect/EffectMaterial/MeshParticle/M_Refraction_MP
Render/Shaders/Effect/EffectMaterial/MeshParticle/M_SimpleEffect_MP
Render/Shaders/Effect/EffectMaterial/Sprite/M_SimpleEffect_S
```

叶实例和父实例的覆盖值已合并为已知参数，并保留各参数的来源。没有母材质图就无法确认 UV 变换、通道运算、折射、溶解以及动态材质参数的最终接线。`ArtSource/ChangliSlash/Evidence/LayerParameters.json` 和同目录说明保存这些事实；当前材质 HLSL 是重建公式。

## 当前导入方案与限制

- 保留 `Notify → Preset → SlashFxComponent` 的运行链路，不修改攻击 GA、输入缓存或技能取消规则。
- `/Game/Effects/ChangliSlash/Reference` 存放新纹理、网格、材质和三个 Niagara 系统；第一段旧预设改为引用这些系统。
- 原网格/图片及明确参数被采用；粒子发射数量、寿命、分布、运动和部分渐隐行为是可调整的重建配置，不是从原运行结果精确反编译得到。
- 部分网格旋转采用原旋转速率 LUT 积分后生成的材质 WPO；羽毛的平面分布、切向运动等由重建配置控制。这些实现不等于原 Niagara 模块图。
- 材质采用原 LUT 值，但当前重采样为 24 点，并用粒子归一化年龄采样。原曲线采样输入尚未确认，不能把 LUT 范围直接当最终寿命、速度或粒子数。
- Cooked Niagara 文件作为分析来源；本方案创建新的可编辑系统，没有宣称直接还原了原编辑器模块图。
- `WuwaSlashFxImport` 默认预检，`-Apply` 才写资产；输入是持久保存的 `ArtSource/ChangliSlash/Attack01.json` 及同目录资源。备份路径为 `Saved/Diagnostics/SlashFx/ReferenceBackup/<UTC年月日-时分秒>-<GUID>/`。
- 本文不宣告本轮新资产的自动化或实际渲染测试已通过；验证结果以本轮最终日志为准。

`OriginalMaterialDependencies.json` 保留早期 Renderer → Mesh/Material → Texture/Parent 的引用映射，但其中“文件缺失”的计数是补导出前的历史快照，**不能用于判断当前文件是否存在**。当前可用性以本页、`ImportedGeometry.json` 和 `ArtSource` 中保留的源文件为准。
