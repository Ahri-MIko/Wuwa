#include "Animation/WuwaAnimLogicParams.h"

void UWuwaAnimLogicParams::Reset()
{
	check(IsInGameThread());
	bHasValidData = false;
	MoveData = FWuwaAnimMoveData{};
	StateData = FWuwaAnimStateData{};
}
