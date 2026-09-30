# 第一段原资源参数提取与重建边界

`LayerParameters.json` 是真实导出数据的紧凑清单，`extract_layer_parameters.py` 可重跑。没有修改生产代码或 UE 资产。

读取顺序：`systems[].layers[]` 确定真实 renderer → material → `materials[].knownEffectiveOverrides` 取得叶实例覆盖父实例后的已知参数。每个材质参数包含来源文件与行号。`chainLeafToRoot` 保留继承链及缺失的根材质，不把已知覆盖误称为完整最终默认值。

## 可以直接采用的事实

- N_Dg 是 4 个 Mesh 层；N1_Dg 只有 Fether_B/Fether_R 两个非空 renderer，不要生成其空 renderer 的 BG/Liang。N_Dg1 是两层 Sprite；本地空间、速度对齐、自定义朝向、Pivot=(0.5,1) 均为明确导出值。
- N_Dg 的材质顺序是 BG=140109、Liang=140100、Fether_B=140062、Fether_R=140055_MP4。Sprite 两层分别是 140031_S 和 140031_S1。绑定在 manifest renderer 原字段中。
- Base_Alpha_Multiply 分别为 3、10、1.5、1、1、1，不能套一个通用不透明度值。
- Mesh 的 Base_AlphaSwitch：BG=(1,1,1,1)，其余三层=(0,0,0,1)。Mesh Mask_AlphaSwitch 均为 (1,0,0,0)。
- 两层 Sprite 的 Base_AlphaSwitch 为 (2,3,1,0) / (0,3,0,0)，Base_EmissiveSwitch 为 (1,3,2,0) / (0,2,0,0)，Mask_AlphaSwitch 均为 (0,0,1,0)。这些是向量参数的原值；没有主材质图时，不能断言内部一定是 dot、max 或选择器。若第一版采用 dot，应明确记录为重建算法。
- 贴图使用独立 Base/Second/Noise/Mask/Dissolve；UV 含负值，例如两个 Sprite 的 Base_UV=(1,-1,0,0)，以及 Fether_B/R 的 Base_UV=(1,1,-1,0)。应保留原向量，不在导入阶段 clamping 或取绝对值。UV 各分量如何接线仍未确认。
- 叶 MI 的 BlendMode 覆盖均为 Translucent，TwoSided 覆盖为 true。`BasePropertyOverrides` 中其他无 bOverride 标识的值不要当作该层显式更改。
- N_Dg1 的 DA 还启用 Rotation.Curve[2]：(-0.0027577877,16.835491)、(0.18540506,39.27856)、(0.9972422,46.85684)。完整三次切线已保存在 `dataAssetExplicitProperties`。不能仅凭数组下标认定具体旋转轴。

## Niagara 独立层时序

完整 76 条绑定及 LUT 位于 `systems[].layers[].curves`。`samples` 是原 LUT 按标量/RGBA分组的完整数值，`bindingSourceLine` 可回到原 NS JSON。`exportedDomain` 只放实际导出的字段，缺失字段不伪造。

| N_Dg 层 | Color LUT 首值 → 尾值 | 动态参数、运动 LUT 的例子 |
|---|---|---|
| BG | (0.58576775,0.05557406,0.073134065,1) → (0.007283002,0,0,0) | Index0 Param1 -0.5→0.5；Param2 0.2→0；Param3 2→0；Scale Alpha 1.6→0 |
| Liang | (4,1.2908871,0.6904495,1) → (2,0.3956505,0.19626164,1) | Rotation Rate 3→0；Index0 Param2 -1→1.05；Scale Alpha 恒1 |
| Fether_B | (1.803961,1.6065903,1.5217457,1) → (0.6220889,0.57438505,0.5359398,1) | Index0 Param1 -0.5→0.3；Index0 Param3 0→1；Index1 Param1 1→0.15；Scale Alpha 0→峰值1.9892391→1 |
| Fether_R | (1.82696,0.7974876,0.7830951,1) → (0.25,0.032026753,0.012106851,1) | Rotation Rate 1→0；Index0 Param1 -0.3371794→0.3；Index1 Param1 1→0.3 |

注意：**Scale Alpha 是颜色/透明度处理的曲线，不能据此缩放 Mesh 几何**。编译作用域显示 Color / DynamicMaterialParameters / UpdateMeshOrientation。没有证据证明上述所有曲线都使用 particle normalized age；域是采样函数输入范围，不能直接都当秒数。

Sprite 两条 LUT 的尺寸 X 范围 -49.998737~-30，Y 80~220.0219；Lifetime 0.3~1.2；Large Radius 80~180.015；Update Velocity Amount 50~135.5704。其有 TorusLocation、VortexVelocity、SolveForcesAndVelocity 模块。最终发射数量、发射时间和 LUT 输入未反编译验证，仍需重建决定，不能把上述范围当作已确认粒子常数。

## 当前无法精确还原的计算图

4 个父实例继续指向以下缺失 Material：

- `/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_Refraction_MP`
- `/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_SimpleEffect_MP`
- `/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_AdvancedEffect_MP`
- `/Game/Aki/Render/Shaders/Effect/EffectMaterial/Sprite/M_SimpleEffect_S`

现有 MI 无 MaterialExpression 节点，无法确认 UV、通道、溶解、折射以及 DynamicParameter 通道到公式的连线。`Index 0 Param 1` 不能自行命名为“溶解值”，除非额外验证了 Material 的消费者。

第一版建议按明确资源形成 6 个可配置材质实例：独立纹理/通道向量/UV/颜色与 Mask，保留顶点色并为 Fether_B/R 保留 `Opacity_bVertexAlphaToMask=1` 的接口；然后加可调溶解噪声、独立颜色曲线。若使用自定义公式，集中在一个重建主材质中，并让导出的实例参数继续可调。避免将所有层烘成同一条渐变色弧线。

原 LUT 可以制成曲线或查找纹理。缺少准确采样输入时，第一版采用的 elapsed/normalized-age 输入、寿命、burst count 和旋转轴必须在资产生成器里作为重建参数命名。不要以 `StartTime=2` 推断每个粒子都显示2秒。

白色占位纹理 `T_DefaultColorWhite_D` 在精确原路径未发现图片。若用常量白替代，应标注为可调整的占位重建；其他已知贴图的图片路径清单见 `textureAvailability`。`CachedReferencedTextures` 可能来自未启用的主材质分支，不应把整表都作为第一段必需资源。
