#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ALyudRestaurant;
class SVerticalBox;

/**
 * The menu card of «Янтарь» (docs/restaurant.md): a printed card on cream paper (the diary's paper), the restaurant's amber band and
 * name, the dishes by section in two columns with their icon, name, a line about them and the price; − and + set how many, the
 * footer sums the order against the money in the pocket. "Заказать" orders (ALyudRestaurant::PlaceOrder), "Попросить счёт" asks for
 * the bill, Esc or "Закрыть" puts the card down. Scaled to fit the screen.
 */
class SLyudRestMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudRestMenu) {}
		SLATE_ARGUMENT(ALyudRestaurant*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Rebuild();
	/** Sets a dish's count (tests). */
	void SetCount(FName Id, int32 N);
	bool Order();

private:
	TWeakObjectPtr<ALyudRestaurant> Owner;
	TMap<FName, int32> Counts;
	FText Status;
	TSharedRef<SWidget> Column(const TArray<FName>& Sections);
	TSharedRef<SWidget> Row(FName Id);
	int32 Total() const;
};
