# 长离普攻刀光挂接参考

本目录仅记录原作导出和项目已有审计数据，没有修改生产源码或资产。JSON 保留逐条通知的路径、时点、挂点、位置、旋转、缩放、启用状态及原始文件行号。

新一批导出的候选 Niagara、材质与网格元数据已检查，见 [ExportAudit.md](ExportAudit.md)。下面仍是 Montage 通知的证据与第一版原创特效的挂接方案；新导出尚未用于替换现有特效。

## 可以确认的事实

- 原作素材：`C:/GamePakExtractor/Output/Exports/Client/Content/Aki/Character/Role/FemaleXL/ChangLi/CommonAnim`。
- 项目资产信息来自 `Saved/Diagnostics/AttackBuffer/GraphAudit.json`，审计时间为 2026-09-28 07:14:43 UTC。当前五个 Montage 文件的磁盘修改时间均早于该审计；本次没有启动 UE 重新读取资产。
- 项目 `AM_Attack05` 使用 `Attack04_1`，所以参考原作 `AM_Attack04_1`。原作此目录没有 `AM_Attack05`，不能按项目编号猜原作文件。
- 原作这些特效通知使用 `AnimNotifyEffect_C`，引用 EffectGroup DataAsset。后续导出已补齐第一段三个组及其子 Niagara 配置 DA，并确认最终 NiagaraSystem；见 `EffectGroupReference.json` 和 `ExportAudit.md`。
- 名字带 `_Dg` 只用于筛选“刀光候选”。它不能证明特效使用 Mesh、Ribbon 或某个 Niagara Renderer，也不能证明每条通知对应一次挥刀。
- `bEnabled=true` 且 `NotifyTriggerChance>0` 才计入启用列表。原作 Attack01/02 的 `Trail_ZL01` 通知虽然启用标记为 true，但概率为 0，已排除。

## 时间轴映射

| 项目 Montage | 原作 Montage | 原作长度 | 项目长度 | 时间倍率 |
|---|---|---:|---:|---:|
| AM_Attack01 | AM_Attack01 | 3.033333 | 3.791667 | 1.25 |
| AM_Attack02 | AM_Attack02 | 4.166667 | 5.208333 | 1.25 |
| AM_Attack03 | AM_Attack03 | 4.500000 | 5.625000 | 1.25 |
| AM_Attack04 | AM_Attack04 | 6.033333 | 7.541667 | 1.25 |
| AM_Attack05 | AM_Attack04_1 | 5.566667 | 6.958333 | 1.25 |

现有五段都引用对应完整 Sequence：段起点为 0，播放倍率为 1，循环次数为 1。项目 Montage RateScale 和 GA 的 PlayMontageAndWait Rate 均为 1，Rate 引脚未接线。以相同完整动画帧序列为前提，第一版通知位置用：

`项目通知秒数 = 原作 LinkValue × 项目长度 / 原作长度 ≈ 原作 LinkValue × 1.25`

原作 `(NumFrames - 1) / SequenceLength` 约为 30。项目时长表现符合相同帧数按 24 fps 导入的结果，但本次没有读到导入设置，不能把原因当作已确认事实。

上表是**蒙太奇时间轴位置**，不是从按键开始的计时器。以后若只调整 Montage/GA 播放速度，通知随播放自然触发，不要再把通知位置乘一次播放倍率。若重新导入改变 Sequence 长度或裁剪动画，需要重新映射。

## 第一版原创弧光的建议触发

同一动作中，相隔不超过原作 0.010 秒的 Dg 候选通知合为一个预设触发；原有各层仍完整保存在 JSON。这个合并阈值是本项目的实现选择，不是声称原作这么分组。

| 项目动作 | 启用 Montage 效果数 | 其中 Dg 候选数 | 建议预设触发次数 | 项目通知位置（秒） |
|---|---:|---:|---:|---|
| Attack01 | 4 | 4 | 1 | 0.244116 |
| Attack02 | 4 | 4 | 1 | 0.220606 |
| Attack03 | 8 | 4 | 4 | 0.203034、0.465602、0.537434、0.632975 |
| Attack04 | 4 | 2 | 1 | 0.296237 |
| Attack05 | 7 | 5 | 4 | 0.500000、0.750000、0.833333、0.916667 |

这里的“预设触发一次”允许生成主弧、较细的第二层弧、细丝和火星。不要把 Attack01 的四条几乎同时触发的引用误做成四次完整大刀光。Attack03 的四个候选点跨越不同时间，应保留节奏再在预览中调整强弱；其另外四条不带 Dg 的启用效果也可能参与主要视觉，已保留，不能认定它们无关。

所有 Dg 候选在导出中都以 `Root` 为定位基准。候选的 Z 偏移多数在 60～130 cm；Attack05 后三次 DgW 没有显式 Location，不能误读成高度 90。具体每层 Transform 查看 JSON 的 `sourceEvents`。

新弧形 Mesh 的轴向、原点、尺寸由本项目定义，原作旋转和缩放不能不加转换地全量照搬。特别是原作若有 Z 负缩放，它表示镜像参考；不能把它直接当成新 Mesh 的必需参数。先用角色 Root 的位置/方向加独立预设偏移，预览核对刀刃路径。原作 `Attached=false` 的条目可确认是生成后不持续随骨骼挂接；未序列化的 `Attached` 为未知继承值，JSON 用 null 保留。

## 底层 Sequence 还有这些特效窗口

这些不是上表的 Dg 点通知，分别存放在 JSON 的 `sourceSequenceEffects`：

- Attack01/02/03：没有额外的 Effect/Trail 通知。
- Attack04：`DA_Fx_Group_San_end`、`DA_Fx_Group_R1a04_San`，均在 `WeaponProp02`；原作约 1.366～3.267 秒。其中 San_end 显式 `PlayOnEnd=true`，不要把窗口开始当作它的实际生成点。第一版独立弧光无需假造伞特效。
- Attack04_1：`DA_Fx_Group_R1a04_1_Trail` 窗口，原作 0.142072～0.583693 秒，对应项目 Attack05 的 0.177591～0.729617 秒；Root，Location=(0,0,80)。目前只有资源引用，内部是否 Ribbon 仍未知。

## 读取 JSON

- `sourceEvents`：Montage 中全部 Effect/Trail 通知，包含被排除的条目。
- `included`：通过启用与概率过滤。
- `dgNameCandidate`：仅文件名筛选。
- `proposedProjectBursts`：本项目第一版点通知位置及每组引用了哪些原作通知。
- `sourceSequenceEffects`：底层 Sequence 的额外 Effect 窗口。
- `location`、`rotation`、`scale` 或 `attachedExplicit` 为 null：原导出没有写这个覆盖值；不是零值，也不是已经确认的默认值。

需要重新生成真实刀光时，本项目的红金弧形 Mesh、材质、细丝与火星都是原创实现；这份参考只提供动画时点与空间布置证据，不包含原作资源的内部制作结构。
