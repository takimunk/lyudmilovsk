#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class ULyudGameScreen;
struct FSlateBrush;

/** The HUD: the date and time and the followed quest's next step (top right), short notes (top left), the binoculars' view. */
class SLyudHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLyudHud) {}
		SLATE_ARGUMENT(ULyudGameScreen*, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void Toast(const FText& Text, bool bQuest);
	/** The quest lines changed. */
	void RefreshQuest();

private:
	struct FToast
	{
		FText Text;
		bool bQuest = false;
		double Born = 0.0;
	};

	TWeakObjectPtr<ULyudGameScreen> Owner;
	TSharedPtr<SVerticalBox> ToastBox;
	TSharedPtr<SVerticalBox> QuestBox;
	TArray<FToast> Toasts;
	TSharedPtr<FSlateBrush> MaskBrush;
	FString Language;
	bool bToastsDirty = false;
};
