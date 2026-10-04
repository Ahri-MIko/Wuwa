# 长离普攻刀光（旧播放系统已停用）

2026-10-03 移除了旧刀光播放组件、对应动画通知类，以及旧导入、生成、预览命令行工具和专属测试。新框架已开始搭建 [Model 数据层](Effect-Model.md)，当前仍不提供替代播放链路。攻击技能、输入缓存、移动和其他战斗通知继续使用现有实现。

`UWuwaSlashFxPreset` 只保留旧资产的数据结构，包括枚举、层配置和反射字段，用于读取现有 11 份预设并支持后续迁移。它不再带运行时变换计算方法，也不负责触发或管理特效播放。项目继续保留 Niagara 依赖供现有美术资源使用。

## 保留资源

| 内容 | 位置 |
|---|---|
| 11 份旧刀光预设 | `/Game/Effects/ChangliSlash/Presets` |
| 第一段参考重建的 Niagara、材质、网格和贴图 | `/Game/Effects/ChangliSlash/Reference` |
| 原程序化刀光的 Niagara、材质和网格 | `/Game/Effects/ChangliSlash/Niagara`、`Materials`、`Mesh` |
| 可迁移的数据类型 | `Source/Wuwa/Public/Game/EffectModel/Legacy/WuwaSlashFxPreset.h` |
| 持久美术源、recipe 和来源证据 | `ArtSource/ChangliSlash` |
| 原资源预处理脚本 | `Tools/SlashFx/preprocess_original_art.py` |
| 导出审计与参考记录 | `Docs/References/SlashFx` |

以上资源均保留。第一段资源仍属于基于导出网格、图片与参数的重建；原母材质计算图、Niagara 发射与运动模块图尚未完整恢复。后续架构设计可继续参考 [导出审计](References/SlashFx/ExportAudit.md) 和 `ArtSource/ChangliSlash/Evidence`，不要将现有重建公式视为原作完整实现。

`ArtSource/ChangliSlash/prepare_reference.py` 和预处理脚本继续保留用于整理源数据。旧 `WuwaSlashFxImport`、`WuwaSlashFxSetup`、`WuwaSlashFxPreview` 已移除，原导入和预览命令不再适用。新架构确定前，暂不重建自动导入、动画通知或播放管理器。

清理前的源码和文档备份位于 `Saved/Diagnostics/SlashFxRemoval/20261003-102252-992fd43f/Backup`。历史渲染、测试报告位于 `Saved/Diagnostics/SlashFx`；这些记录仅反映旧系统当时的结果，不能作为当前停用状态的运行验证。
