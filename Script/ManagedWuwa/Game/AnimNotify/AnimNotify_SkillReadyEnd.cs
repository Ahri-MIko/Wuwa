using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;
using ManagedWuwa.Game.NewWorld.Input;

namespace ManagedWuwa.Animation.Notifies;

/// <summary>进入让位阶段；当前 GA 和蒙太奇继续播放，直到被替换或自行结束。</summary>
[UClass]
public partial class UAnimNotify_SkillReadyEnd : UWuwaAnimNotify_SkillReadyEndBridge
{
    public override string GetNotifyName() => "技能进入让位阶段";

    protected override void ReachSkillReadyEnd_Implementation(UWuwaSkillBridgeComponent skills, int skillHandle)
    {
        bool valid = skills.IsValid();
        if (CombatInputTrace.Enabled)
            CombatInputTrace.Write("READY_END_ENTER",
                $"requestedHandle={skillHandle} {(valid ? CombatInputTrace.Context(skills) : "skills=invalid")}");
        if (!valid) return;

        bool readyEndWritten = skills.SetMainSkillReadyEnd(skillHandle, true);
        // 保留原来的短路顺序：ReadyEnd 写入失败时不尝试改等级或发出断点。
        bool priorityWritten = readyEndWritten && skills.SetSkillInterruptLevel(skillHandle, 0);
        if (CombatInputTrace.Enabled)
            CombatInputTrace.Write("READY_END_STATE",
                $"requestedHandle={skillHandle} readyEndWritten={readyEndWritten} priorityAttempted={readyEndWritten} priorityWritten={priorityWritten} {CombatInputTrace.Context(skills)}");
        if (priorityWritten) skills.CallAnimBreakPoint(skillHandle);
    }
}
