#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"

void UWuwaAnimLogicParams::Reset()
{
	check(IsInGameThread());
	bHasValidData = false;
	MoveData = FWuwaAnimMoveData{};
	StateData = FWuwaAnimStateData{};
}
