#include "SLyudRestMenu.h"

#include "LyudInventory.h"
#include "LyudText.h"
#include "Restaurant/LyudRestaurant.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "UI/LyudDiaryStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace LyudRestMenu
{
	// a printed card: brown-black ink, the restaurant's amber, a faded rule
	const FLinearColor Ink(0.11f, 0.07f, 0.04f);
	const FLinearColor InkSoft(0.3f, 0.22f, 0.15f);
	const FLinearColor Amber(0.72f, 0.36f, 0.04f);
	const FLinearColor AmberDeep(0.42f, 0.17f, 0.02f);
	const FLinearColor Rule(0.55f, 0.42f, 0.28f, 0.6f);
	const FLinearColor Warn(0.62f, 0.14f, 0.08f);

	FSlateFontInfo Font(int32 Size, const char* Face = "Regular")
	{
		return FCoreStyle::GetDefaultFontStyle(Face, Size);
	}

	/** Lines with a price: the engine's Roboto Regular and Bold have no ₽ (U+20BD), its Medium face has. */
	FSlateFontInfo MoneyFont(int32 Size)
	{
		return FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Medium.ttf"), Size);
	}

	const FButtonStyle& Button()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), 4.f, AmberDeep, 1.5f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.78f, 0.35f, 0.35f), 4.f, AmberDeep, 1.5f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 0.7f, 0.25f, 0.55f), 4.f, AmberDeep, 1.5f))
			.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), 4.f, Rule, 1.f))
			.SetNormalPadding(FMargin(10.f, 4.f)).SetPressedPadding(FMargin(10.f, 5.f, 10.f, 3.f));
		return Style;
	}

	const FSlateBrush* Band()
	{
		static const FSlateColorBrush Brush(Amber);
		return &Brush;
	}

	const FSlateBrush* Line()
	{
		static const FSlateColorBrush Brush(Rule);
		return &Brush;
	}
}

void SLyudRestMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;
	Rebuild();
}

int32 SLyudRestMenu::Total() const
{
	int32 T = 0;
	for (const TPair<FName, int32>& C : Counts)
	{
		if (const FLyudRestDishDef* D = ALyudRestaurant::FindDish(C.Key))
		{
			T += (Owner.IsValid() ? Owner->PriceToday(*D) : D->Price) * C.Value;
		}
	}
	return T;
}

void SLyudRestMenu::SetCount(FName Id, int32 N)
{
	if (N <= 0)
	{
		Counts.Remove(Id);
	}
	else
	{
		Counts.Add(Id, FMath::Min(N, 9));
	}
	Rebuild();
}

bool SLyudRestMenu::Order()
{
	ALyudRestaurant* R = Owner.Get();
	if (!R || Counts.Num() == 0)
	{
		return false;
	}
	if (!R->IsOpen())
	{
		Status = LyudText::Get(TEXT("ui.rest.closed"));
		Rebuild();
		return false;
	}
	const ALyudRestaurant::EOrderResult Result = R->PlaceOrder(Counts);
	if (Result == ALyudRestaurant::EOrderResult::Ok)
	{
		Status = LyudText::Get(TEXT("ui.rest.ordered"));
		Counts.Reset();
		Rebuild();
		return true;
	}
	Status = LyudText::Get(Result == ALyudRestaurant::EOrderResult::Closed ? TEXT("ui.rest.closed")
		: Result == ALyudRestaurant::EOrderResult::NotSeated ? TEXT("ui.rest.not_seated")
		: Result == ALyudRestaurant::EOrderResult::NoMoney ? TEXT("ui.rest.no_money")
		: Result == ALyudRestaurant::EOrderResult::KitchenClosing ? TEXT("ui.rest.kitchen_closing") : TEXT("ui.rest.order_failed"));
	Rebuild();
	return false;
}

TSharedRef<SWidget> SLyudRestMenu::Row(FName Id)
{
	using namespace LyudRestMenu;
	const FLyudRestDishDef* D = ALyudRestaurant::FindDish(Id);
	ULyudInventoryUI* UI = ULyudInventoryUI::Get(Owner.Get());
	const int32 N = Counts.FindRef(Id);
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
		[
			SNew(SBox).WidthOverride(46.f).HeightOverride(46.f)[SNew(SImage).Image(UI ? UI->IconBrush(Id) : nullptr)]
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(16, "Bold")).ColorAndOpacity(Ink).Text(ULyudInventory::ItemName(Id))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(11)).ColorAndOpacity(InkSoft).AutoWrapText(true).Text(ULyudInventory::ItemDescription(Id))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(11, "Bold")).ColorAndOpacity(Amber)
				.Visibility(D && Owner.IsValid() && Owner->IsCatchOfDay(*D) ? EVisibility::Visible : EVisibility::Collapsed)
				.Text(LyudText::Get(TEXT("ui.rest.catch_of_day")))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 8, 0)
		[
			SNew(SBox).MinDesiredWidth(58.f)
			[
				SNew(STextBlock).Font(MoneyFont(15)).ColorAndOpacity(AmberDeep).Justification(ETextJustify::Right)
				.Text(D && D->Price > 0 ? LyudText::Get(TEXT("ui.rest.price"), { FStringFormatArg(Owner.IsValid() ? Owner->PriceToday(*D) : D->Price) }) : LyudText::Get(TEXT("ui.rest.free")))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(&Button()).IsEnabled(N > 0)
				.OnClicked_Lambda([this, Id, N]() { SetCount(Id, N - 1); return FReply::Handled(); })
				[SNew(STextBlock).Font(Font(14, "Bold")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("−")))]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
			[
				SNew(SBox).MinDesiredWidth(16.f)
				[
					SNew(STextBlock).Font(Font(15, "Bold")).ColorAndOpacity(N > 0 ? Ink : Rule).Justification(ETextJustify::Center)
					.Text(FText::AsNumber(N))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).ButtonStyle(&Button()).IsEnabled(D && !D->bFree && N < 9)
				.OnClicked_Lambda([this, Id, N]() { SetCount(Id, N + 1); return FReply::Handled(); })
				[SNew(STextBlock).Font(Font(14, "Bold")).ColorAndOpacity(Ink).Text(FText::FromString(TEXT("+")))]
			]
		];
}

TSharedRef<SWidget> SLyudRestMenu::Column(const TArray<FName>& Sections)
{
	using namespace LyudRestMenu;
	TSharedRef<SVerticalBox> Col = SNew(SVerticalBox);
	for (const FName& S : Sections)
	{
		// a heading only over dishes that are on today
		if (!ALyudRestaurant::Menu().ContainsByPredicate([&](const FLyudRestDishDef& D) { return ALyudRestaurant::IsInSectionToday(Owner.Get(), D, S); }))
		{
			continue;
		}
		Col->AddSlot().AutoHeight().Padding(0, 10, 0, 4)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(19, "Bold")).ColorAndOpacity(Amber)
				.Text(LyudText::Get(FString::Printf(TEXT("ui.rest.section.%s"), *S.ToString())))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 4)[SNew(SBox).HeightOverride(1.5f)[SNew(SImage).Image(Line())]]
		];
		for (const FLyudRestDishDef& D : ALyudRestaurant::Menu())
		{
			if (ALyudRestaurant::IsInSectionToday(Owner.Get(), D, S))
			{
				Col->AddSlot().AutoHeight().Padding(0, 3)[Row(D.Id)];
			}
		}
	}
	return Col;
}

void SLyudRestMenu::Rebuild()
{
	using namespace LyudRestMenu;
	ALyudRestaurant* R = Owner.Get();
	ULyudInventory* Inv = ULyudInventory::Get(R);
	const int32 Money = Inv ? Inv->Rubles : 0;
	const int32 T = Total();
	const bool bOpen = R && R->IsOpen();
	const bool bCanBill = R && R->TabTotal() > 0 && !R->IsBillAsked();
	TArray<FName> Left, Right;
	Left = { TEXT("special"), TEXT("starters"), TEXT("soups"), TEXT("mains") };
	Right = { TEXT("desserts"), TEXT("drinks") };
	if (ALyudRestaurant::IsFestival(R))
	{
		Right.Add(TEXT("festival"));
	}
	const FText Footer = LyudText::Get(TEXT("ui.rest.total"), { FStringFormatArg(T) });
	const FText Pocket = LyudText::Get(TEXT("ui.rest.pocket"), { FStringFormatArg(Money) });
	const FText TabLine = R && R->TabTotal() > 0 ? LyudText::Get(TEXT("ui.rest.tab"), { FStringFormatArg(R->TabTotal()) }) : FText::GetEmpty();

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.35f))]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(30.f)
		[
			SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::Both)
			[
				SNew(SBox).WidthOverride(1180.f).HeightOverride(820.f)
				[
					SNew(SBorder).BorderImage(LyudDiary::PaperBrush(true)).Padding(0.f)
					[
						SNew(SVerticalBox)
						// the amber band with the name
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBorder).BorderImage(Band()).Padding(FMargin(36.f, 14.f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(34, "Bold")).ColorAndOpacity(FLinearColor(1.f, 0.95f, 0.85f))
									.Text(LyudText::Get(TEXT("ui.rest.title")))
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(15)).ColorAndOpacity(FLinearColor(1.f, 0.92f, 0.8f))
									.Text(LyudText::Get(!bOpen ? TEXT("ui.rest.closed") : R->IsKitchenClosing() ? TEXT("ui.rest.kitchen_closing") : TEXT("ui.rest.subtitle")))
								]
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(36.f, 6.f, 36.f, 0.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 22, 0)[Column(Left)]
							+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1.5f)[SNew(SImage).Image(Line())]]
							+ SHorizontalBox::Slot().FillWidth(1.f).Padding(22, 0, 0, 0)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[Column(Right)]
								+ SVerticalBox::Slot().FillHeight(1.f)[SNew(SSpacer)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
								[
									SNew(STextBlock).Font(Font(13)).ColorAndOpacity(InkSoft).AutoWrapText(true).Text(LyudText::Get(TEXT("ui.rest.note")))
								]
							]
						]
						// the order and the buttons
						+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(36.f, 8.f, 36.f, 20.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SBox).HeightOverride(1.5f)[SNew(SImage).Image(Line())]]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 24, 0)
								[
									SNew(STextBlock).Font(MoneyFont(19)).ColorAndOpacity(T > Money ? Warn : Ink).Text(Footer)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 24, 0)
								[
									SNew(STextBlock).Font(MoneyFont(14)).ColorAndOpacity(InkSoft).Text(Pocket)
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 24, 0)
								[
									SNew(STextBlock).Font(MoneyFont(14)).ColorAndOpacity(InkSoft).Text(TabLine)
								]
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(Font(14, "Bold")).ColorAndOpacity(AmberDeep).AutoWrapText(true).Text(Status)
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
								[
									SNew(SButton).ButtonStyle(&Button()).IsEnabled(bOpen && !R->IsKitchenClosing() && T > 0 && T <= Money)
									.OnClicked_Lambda([this]() { Order(); return FReply::Handled(); })
									[SNew(STextBlock).Font(Font(17, "Bold")).ColorAndOpacity(Ink).Text(LyudText::Get(TEXT("ui.rest.order")))]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
								[
									SNew(SButton).ButtonStyle(&Button()).IsEnabled(bCanBill)
									.OnClicked_Lambda([this]()
									{
										if (ALyudRestaurant* O = Owner.Get())
										{
											O->AskBill();
											Status = LyudText::Get(TEXT("ui.rest.bill_asked"));
											Rebuild();
										}
										return FReply::Handled();
									})
									[SNew(STextBlock).Font(Font(17, "Bold")).ColorAndOpacity(Ink).Text(LyudText::Get(TEXT("ui.rest.bill")))]
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
								[
									SNew(SButton).ButtonStyle(&Button())
									.OnClicked_Lambda([this]()
									{
										if (ALyudRestaurant* O = Owner.Get())
										{
											O->CloseMenu();
										}
										return FReply::Handled();
									})
									[SNew(STextBlock).Font(Font(17)).ColorAndOpacity(InkSoft).Text(LyudText::Get(TEXT("ui.rest.close")))]
								]
							]
						]
					]
				]
			]
		]
	];
}
