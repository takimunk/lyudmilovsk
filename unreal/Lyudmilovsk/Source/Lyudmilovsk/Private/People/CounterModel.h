#pragma once

// Counters with real service (docs/counters.md): the rules of the scene at a counter, plain C++ (tools/countersim checks them).
//   paying: the hero hands over the smallest note that covers what he bought, and gets the difference back
//   the line: how many stand before the hero

#include <vector>

namespace LyudCounter
{
	/** The notes (and the rouble coin) a purse of the time holds, smallest first. */
	constexpr int Notes[] = { 1, 3, 5, 10, 25, 50, 100 };

	/** The note the hero pays Total roubles with, out of a purse of Purse roubles before paying: the smallest one that
	 *  covers it and that he can have (no more than the purse). 0: he counts it out exactly (one note is exactly the
	 *  price, nothing to pay, or no single note fits his purse). */
	inline int PayWith(int Total, int Purse)
	{
		if (Total <= 0 || Purse < Total)
		{
			return 0;
		}
		for (const int N : Notes)
		{
			if (N >= Total && N <= Purse)
			{
				return N == Total ? 0 : N;
			}
		}
		return 0;
	}

	/** What comes back to him (0: none). */
	inline int Change(int Total, int Purse)
	{
		const int N = PayWith(Total, Purse);
		return N > 0 ? N - Total : 0;
	}

	/** People standing before the spot Mine in a line, front first (-1 a free spot, -2 the hero, a person's id otherwise). */
	inline int Ahead(const std::vector<int>& Line, int Mine)
	{
		int N = 0;
		for (int I = 0; I < Mine && I < (int)Line.size(); I++)
		{
			N += Line[(size_t)I] >= 0 ? 1 : 0;
		}
		return N;
	}

	/** The hero's spot in a line (-1: not in it). */
	inline int HeroSpot(const std::vector<int>& Line)
	{
		for (int I = 0; I < (int)Line.size(); I++)
		{
			if (Line[(size_t)I] == -2)
			{
				return I;
			}
		}
		return -1;
	}
}
