// The yacht school's news for the town (Public/Sailing/LyudSailNews.h): the default answers are empty until the school's
// engine registers its providers.
#include "Sailing/LyudSailNews.h"

namespace LyudSailNewsImpl
{
	FLyudSailNewsProvider& News()
	{
		static FLyudSailNewsProvider P;
		return P;
	}

	FLyudSailMorningProvider& Morning()
	{
		static FLyudSailMorningProvider P;
		return P;
	}
}

void LyudSailNewsOfDay(const UObject* WorldContext, int32 DayNumber, TArray<FLyudSailNewsItem>& Out)
{
	Out.Reset();
	if (LyudSailNewsImpl::News())
	{
		LyudSailNewsImpl::News()(WorldContext, DayNumber, Out);
	}
}

bool LyudSailMorningOf(const UObject* WorldContext, int32 DayNumber, FLyudSailMorning& Out)
{
	return LyudSailNewsImpl::Morning() ? LyudSailNewsImpl::Morning()(WorldContext, DayNumber, Out) : false;
}

void LyudSetSailNewsProviders(FLyudSailNewsProvider News, FLyudSailMorningProvider Morning)
{
	LyudSailNewsImpl::News() = MoveTemp(News);
	LyudSailNewsImpl::Morning() = MoveTemp(Morning);
}
