# 特效框架第 1 步：Model 数据资产

目前已建立 **14 种具体 Model**，共同继承 UWuwaEffectModelBase。Model 保存“播放什么、使用什么参数”；Spec 执行播放，Handle 标识一次播放，EffectSystem 提供入口。后续阶段现已接入 **Audio 的基础执行器和通用播放通知**，配置与使用见 [EffectSystem 第一阶段：用脚步声验证统一播放入口](Effect-Audio-FirstStep.md)。其余 Model 仍只有数据定义；创建资产本身不会自动播放表现。

这里补齐的是本地导出中有字段证据的数据层，并非恢复库洛完整原生 C++。未知字段和专有后端的边界见下文。

## 建议按这个顺序读

1. [WuwaEffectModelBase.h](../Source/Wuwa/Public/Game/EffectModel/Base/WuwaEffectModelBase.h)：公共生命周期、时间倍率、可见性和平台策略。它继承 UPrimaryDataAsset，是不能直接创建资产的抽象基类。
2. [WuwaEffectCurveTypes.h](../Source/Wuwa/Public/Game/EffectModel/Types/WuwaEffectCurveTypes.h) 和 [WuwaEffectVectorCurve.h](../Source/Wuwa/Public/Game/EffectModel/Types/WuwaEffectVectorCurve.h)：参数如何选择常量或曲线。
3. [WuwaEffectModelNiagara.h](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelNiagara.h) 与 [WuwaEffectModelAudio.h](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelAudio.h)：从粒子和声音理解一个 Model 如何描述一种播放内容。
4. [WuwaEffectModelGroup.h](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelGroup.h)：组合不同内容，每项可以设置独立延迟。
5. 按需要读下面的网格、贴花、灯光等模型；最后读 [WuwaEffectModelMultiEffect.h](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelMultiEffect.h)，理解重复生成与固定组合的区别。

本项目的 EffectModel 直接位于 Game 下，基类、公共类型与具体 Model 分开存放：

```text
Source/Wuwa/
├─ Public/Game/EffectModel/
│  ├─ Base/    WuwaEffectModelBase.h
│  ├─ Types/   WuwaEffectCurveTypes.h、WuwaEffectVectorCurve.h
│  ├─ Models/  全部 14 种具体 Model（含 WuwaEffectModelMultiEffect.h）
│  └─ Legacy/  WuwaSlashFxPreset.h（旧资产兼容）
└─ Private/Game/EffectModel/
   ├─ Base/    WuwaEffectModelBase.cpp
   └─ Models/  Audio、Group、MultiEffect、Niagara 的校验实现 .cpp
```

对应 Private/Game/EffectModel/Base 和 Models 中的 .cpp 当前用于编辑器数据校验，包含资源、时间和引用环检查，没有执行播放。先读懂数据结构，再读这些检查即可。

## 14 种 Model 的文件与用途

下表 14 种都是 UWuwaEffectModelBase 的直接子类。

| 分类 | 具体类与文件 | 配置内容 |
|---|---|---|
| 组合 | [UWuwaEffectModelGroup](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelGroup.h) | 多个子 Model 与各自延迟；整组局部变换。 |
| 组合 | [UWuwaEffectModelMultiEffect](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelMultiEffect.h) | 重复生成同一个 Model 的配置；已确认的行为类型为 BuffBall。 |
| 粒子 | [UWuwaEffectModelNiagara](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelNiagara.h) | Niagara System、用户参数曲线、变换、停止及渲染选项。 |
| 声音 | [UWuwaEffectModelAudio](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelAudio.h) | 主声音、停止拖尾声音、位置偏移、遮挡和淡出；区分 UE 声音与 Wwise 事件。 |
| 网格 | [UWuwaEffectModelStaticMesh](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelStaticMesh.h) | 静态网格、材质槽覆盖、材质参数曲线和变换。 |
| 网格 | [UWuwaEffectModelSkeletalMesh](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelSkeletalMesh.h) | 骨骼网格与动画资源、动画循环、开始隐藏帧数和变换。 |
| 朝向控制 | [UWuwaEffectModelBillboard](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelBillboard.h) | 原作 Billboard 的更新、朝向与尺寸控制参数；没有凭空添加贴图或材质字段。 |
| 贴花 | [UWuwaEffectModelDecal](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelDecal.h) | 贴花材质、参数曲线、变换，以及库洛 Z 方向衰减配置。 |
| 残影 | [UWuwaEffectModelGhost](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelGhost.h) | 哪些骨骼网格部位产生残影、统一材质和透明度曲线。 |
| 拖尾 | [UWuwaEffectModelTrail](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelTrail.h) | 带状网格的骨骼/插槽采样点、偏移曲线、材质与消散参数。 |
| 灯光 | [UWuwaEffectModelLight](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelLight.h) | 原作可见的角色灯光字段，以及本项目 UE 点光适配参数。 |
| 后处理 | [UWuwaEffectModelPostProcess](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelPostProcess.h) | 体积范围、混合强度、后处理材质、径向模糊，以及 UE 后处理设置适配。 |
| 专有粒子 | [UWuwaEffectModelGpuParticle](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelGpuParticle.h) | 原作 GPU 粒子数据路径、变换、循环、倒放、往返和时间倍率。 |
| 材质控制 | [UWuwaEffectModelMaterialController](../Source/Wuwa/Public/Game/EffectModel/Models/WuwaEffectModelMaterialController.h) | 原作单个角色材质控制器与控制器组的数据路径。 |

一个 Group 可以同时引用 Niagara、Audio 和 Light。它只保存子资产和延迟；之后分别由对应 Spec 创建粒子、提交声音、创建灯光，Group 不需要增加声音专属字段。

Model 不存角色实例、动态材质实例、播放组件或进度。例如 Ghost 的目标骨骼组件、生成间隔和单个残影寿命，在原作脚本中来自播放 Context，而非 Ghost Model。

## 打开示例看组合配置

内容浏览器目录：/Game/Effects/Models/Examples。

原有 DA_Example_Group 保留以下配置：

| EffectData 的 Key | 延迟（秒） | 含义 |
|---|---:|---|
| DA_Example_NiagaraSlash | 0 | 组开始时启动刀光子项。 |
| DA_Example_NiagaraEmbers | 0.1 | 组开始 0.1 秒后启动火星子项。 |

本轮新增混合示例 DA_Example_MixedGroup 的配置：

| EffectData 的 Key | 延迟（秒） | 子资产内容 |
|---|---:|---|
| DA_Example_NiagaraSlash | 0 | 复用既有刀光 Model。 |
| DA_Example_AudioTestTone | 0 | UnrealSound；Sound 指向引擎 /Engine/EngineSounds/1kSineTonePing 测试音。 |
| DA_Example_Light | 0.1 | 展示点光颜色、强度和半径的数据配置。 |

**测试音不是鸣潮挥刀声。** 示例只演示配置与组合关系，不宣称还原原作视觉、声音或时序。示例创建与重载验证结果统一记录在文末。

自行创建：内容浏览器右键 → 杂项（Miscellaneous）→ 数据资产（Data Asset），选择表中的具体 Model。先配置子 Model 的资源，再创建 Group，在 EffectData 中选择子资产并填写延迟。保存后可以运行 UE 数据验证。

## 时间、变换和曲线的准确含义

| 字段 | 含义 |
|---|---|
| StartTime | 起始播放段长度；不是生成延迟。负数保留原作持续播放语义。默认 1 秒是项目选择。 |
| LoopTime | 大于 0 表示循环段长度，等待外部停止才退出循环；不是“再播这么多秒后结束”。 |
| EndTime | 结束段长度；不自动替代粒子寿命或淡出曲线。 |
| Group.EffectData 的 Value | 子 Model 相对 Group 开始播放的延迟，单位秒。 |
| Location / Rotation / Scale | 局部变换：子 Model 相对父组，根 Model 相对未来传入的播放位置。并非每种 Model 都有这三个字段。 |
| Rotation 的 X / Y / Z | 分别是 Roll / Pitch / Yaw，单位度。 |
| bUseCurve | false 使用 Constant；true 使用曲线。未来 Spec 按具体参数语义提供采样时间。 |
| DeactivateOnStop | 停止时是否 Deactivate Niagara；不等于立即销毁已有粒子。 |
| SkeletalMesh.Looping / GpuParticle.Loop | 内容后端的循环开关，与基类 LoopTime 的特效生命周期配置不同。 |

曲线类型映射：

| 原作数据 | 本项目结构 | 曲线通道 |
|---|---|---|
| KuroCurveFloat | FWuwaEffectFloatCurve | Curve |
| KuroCurveVector | FWuwaEffectVectorCurve | CurveX / CurveY / CurveZ |
| KuroCurveLinearColor | FWuwaEffectColorCurve | CurveR / CurveG / CurveB / CurveA |

每个通道使用 UE FRuntimeFloatCurve，支持内嵌曲线及外部 CurveFloat。显式列出轴/通道是 UnrealSharp 适配：当前绑定对 FRuntimeVectorCurve.VectorCurves[3] 只生成单个 FRichCurve 字段，直接使用固定数组会缺少后两轴。这是表示方式适配，不是原资源二进制兼容。

内嵌曲线默认值与初始常量一致，例如 Location/Rotation 为 0、Scale 为 1。Constant 与曲线是独立配置，之后修改 Constant 再开启曲线，不会自动把常量烘焙成曲线。

不要假定所有横轴都是秒：原作 Ghost.AlphaCurve 使用**剩余寿命比例**，刚生成时为 1，消失前为 0。Trail.LocationsCurve 的 Key 是采样点下标，曲线按总播放时间采样。MultiEffect.BaseNum 参与随播放时间增长的期望数量计算，并非固定总数；SpinSpeed 在已导出算法中为弧度/秒。

## Group 与 MultiEffect 的引用规则

- Group.EffectData 是硬引用 Map：子 Model → 延迟。Map 顺序不是播放顺序，同一资产只能作一个 Key。相同 Niagara 需要两套参数或延迟时，创建两个引用相同 Niagara System 的 Model。
- Group 的直属子项不能是另一个 Group，对应原作 Spec 对非根 Group 的限制。
- MultiEffect.EffectData 也是硬引用，但表示“重复生成哪个 Model”。原作 Multi 通过 SpawnEffect 创建独立播放根，因此 **MultiEffect 可以引用 Group**，这个 Group 是每次生成的根组。
- 以上规则不允许绕出引用环。编辑器检查整个 Group/MultiEffect 引用图，拒绝自引用和跨资产间接循环，例如 Group → MultiEffect → Group 本身。多个父资产共享同一个无环子资产合法。

## 与鸣潮的对应和实现边界

已核对的导出根目录为 C:/GamePakExtractor/Output/Exports/Client/Content/Aki/。

| 证据文件（相对导出根目录） | 能确认的内容 |
|---|---|
| JavaScript/RunTimeLibs/UsedInfo.json | 脚本使用的原生类名和属性名。 |
| JavaScript/RunTimeLibs/PuertsWrapper.json | 原生类型与脚本绑定的属性列表；不是完整的带类型 C++ 声明。 |
| TypeScript/Game/Render/Effect/Data/EffectModel*.json / .cpp | 包装类原生父类，以及有导出的默认对象覆盖值。 |
| JavaScript/Game/Render/Effect/Data/EffectModel*.js | 多数是继承原生 Model 的空包装；不能据此断定原生类没有其他字段或逻辑。 |
| JavaScript/Game/Effect/EffectSpec/EffectModel*Spec.js | 字段如何使用、播放实例由谁持有、哪些行为依赖库洛原生组件。 |
| JavaScript/Game/Effect/EffectSpec/EffectSpec.js | 非根 Group 限制。 |
| JavaScript/Game/Effect/EffectLifeTime.js | 三阶段时间及 StartTime < 0 / LoopTime > 0 的持续播放判定。 |
| Effect/DataAsset/Niagara/R2T1ChangliMd10011/DA_Fx_R1a01_01_N_Dg1.json | 实际资产的 NiagaraRef、StartTime 和 Rotation 曲线表示。 |
| JavaScript/Game/Render/Effect/Data/EffectModelHelper.js | 旋转向量 X=Roll、Y=Pitch、Z=Yaw。 |

**可见字段不等于全部原生字段。** 原作 CDO 未导出的默认值采用项目选择，代码注释区分两者。例如 Light 的角色衰减指数 5、角色硬阴影黑色，以及 PostProcess 的 MaskScale=(5,5)、IgnoreTimeDilation=true 有导出证据；不能将所有初始化值都称为原作默认。

需要明确区分的适配点：

- **Audio**：原作 AudioEvent / TrailingAudioEvent 是 Wwise AkAudioEvent。本项目没有 Wwise 插件，因此保留 FSoftObjectPath，不把它伪装成 USoundBase。UE 声音资源填独立 Sound / TrailingSound 字段，通过项目枚举 AudioBackend 选择资源模式。Wwise 淡出曲线原始值与 UE EAudioFaderCurve 分开；项目 FadeOutTime 统一为秒，后端单位换算留给未来适配器。保留路径不代表能播放原作音频。
- **Light**：可确认原作创建点光，但原生字段未完整导出。Unreal Adapter 分类下的强度、颜色、半径是项目适配配置；角色专用衰减和硬阴影颜色不会自动获得库洛卡通渲染效果。
- **PostProcess**：Settings 是项目 FPostProcessSettings 适配入口。TOD、径向模糊等库洛扩展需要后续渲染适配；UE 默认后处理不能直接执行同名库洛接口。
- **GpuParticle**：原作依赖 KuroGPUParticleComponent，不是 Niagara 的 GPU 模拟开关。Data 的专有资源类型无法确认，保存资源路径；未创建假的专有组件。
- **MaterialController**：保存角色材质控制器数据及控制器组路径；原作执行依赖 CharRenderingComponent。它们不是普通 MaterialInstance，Model 也不保存动态材质实例。
- **Trail / Billboard**：原作依赖 KuroBezierMeshComponent / KuroBillboardComponent，目前只建立配置。Billboard.OrientAxis 的原生类型及取值映射无法确认，暂存原始整数并明确标记，不能当作已确认的 UE EAxis 枚举。
- **Base.SuperFarProgramFlag**：只有名称证据，没有足够类型与语义证据，因此未加入可编辑数据模型。

另有 **CurveTrailDecal、NDC、SequencePose** 三种类型只能确认存在，尚无足够字段证据，没有创建可实例化空壳。证据与后续所需导出内容见 [尚未还原的 Model](References/EffectModelUnavailable.md)。因此这里是 14 种具体 Model 的数据层，不是“原作 17 种已经全部实现”。

## 本阶段检查

本轮 WuwaEditor Development 编译及 UnrealSharp 绑定编译通过。UE 中实例化了全部 14 种具体 Model，并完成 21 项配置与资产检查：包括音频模式选择、循环拖尾拒绝、引用环拒绝、共享子图允许、混合 Group 和曲线参数配置，以及 3 个新增示例的数据验证。

另起 UE 进程重新加载新增示例，确认声音引用、混合子项及延迟、灯光曲线两个关键帧和颜色保存正确；旧版三个 Niagara/Group 示例也重新加载验证通过。原 ChangliSlash 美术资产的文件哈希保持一致。记录位于 Saved/Diagnostics/EffectModel/Expansion 下的 build.log、created.json、verified.json 和 previous-examples.log。

以上是 Model 数据层阶段的检查，验证反射、配置校验与保存加载，不代表各类表现已经能播放。后续仅 Audio 的基础播放已接入，当前支持范围见 [Audio 执行阶段说明](Effect-Audio-FirstStep.md)。命令行运行期间存在本机外部缓存目录的访问警告，不影响上述项目资产的保存加载结果。
