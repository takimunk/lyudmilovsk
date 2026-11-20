#include "AtonLabApps.h"

#include "AtonApps.h"
#include "AtonLab.h"
#include "AtonStyle.h"
#include "Lab/LabAdventure.h"
#include "Lab/LabChess.h"
#include "Lab/LabNet.h"
#include "SAtonDesktop.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Sky/LyudSky.h"
#include "Weather/LyudWeather.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace AtonLabUI
{
	// ---------------------------------------------------------------- small things every program uses

	FText T(const FString& S) { return FText::FromString(S); }

	/** ui.* text in the current language. */
	FString L(const FString& Key, const TArray<FStringFormatArg>& Args = {}) { return LyudText::Get(Key, Args).ToString(); }

	/** The text of a key the plain models name (UTF-8). */
	FString LK(const std::string& Key) { return L(FString(UTF8_TO_TCHAR(Key.c_str()))); }

	TSharedRef<SWidget> Label(const FString& Text, int32 Size = 15, FLinearColor Color = AtonStyle::Ink, bool bBold = false)
	{
		return SNew(STextBlock).Text(T(Text)).Font(AtonStyle::Font(Size, bBold)).ColorAndOpacity(Color).AutoWrapText(true);
	}

	TSharedRef<SWidget> MonoLabel(const FString& Text, int32 Size = 15, FLinearColor Color = AtonStyle::Green)
	{
		return SNew(STextBlock).Text(T(Text)).Font(AtonStyle::Mono(Size)).ColorAndOpacity(Color).AutoWrapText(true);
	}

	TSharedRef<SWidget> Panel(const FLinearColor& Color, const TSharedRef<SWidget>& Content, FMargin Padding = FMargin(0))
	{
		return SNew(SBorder).BorderImage(AtonStyle::Flat()).BorderBackgroundColor(Color).Padding(Padding)[Content];
	}

	TSharedRef<SWidget> Button(const FString& Text, TFunction<void()> OnClick, int32 Size = 14)
	{
		return SNew(SButton)
			.ButtonStyle(&AtonStyle::Button())
			.ContentPadding(FMargin(10, 4))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([OnClick] { OnClick(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(T(Text)).Font(AtonStyle::Font(Size)).ColorAndOpacity(AtonStyle::Ink)
			];
	}

	TSharedRef<SWidget> Divider(const FLinearColor& Color = AtonStyle::PaperLine)
	{
		return SNew(SBox).HeightOverride(1)[Panel(Color, SNullWidget::NullWidget)];
	}

	const FLinearColor Side = AtonStyle::C(0xE9, 0xE5, 0xD8);

	ULyudAtonLab* LabOf(const IAtonShell* Shell)
	{
		return Shell ? ULyudAtonLab::Get(Shell->ShellWorld()) : nullptr;
	}

	/** What the hero did on the lab network; a state of its own when the machine stands in no game world. */
	AtonLab::FLabState& StateOf(const IAtonShell* Shell)
	{
		if (ULyudAtonLab* Lab = LabOf(Shell))
		{
			return Lab->State();
		}
		static AtonLab::FLabState Loose;
		return Loose;
	}

	void Report(const IAtonShell* Shell, const TCHAR* What)
	{
		if (ULyudAtonLab* Lab = LabOf(Shell))
		{
			Lab->Report(What);
		}
	}

	std::string LoadUtf8(const TCHAR* ContentFile)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / ContentFile));
		return std::string(TCHAR_TO_UTF8(*Text));
	}

	/** The letters of Content/AtonOS/lab_mail.json, read once. */
	const std::vector<AtonLab::FLabLetter>& Letters()
	{
		static std::vector<AtonLab::FLabLetter> List;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			std::string Error;
			if (!AtonLab::LoadLetters(LoadUtf8(TEXT("AtonOS/lab_mail.json")), List, &Error))
			{
				UE_LOG(LogTemp, Warning, TEXT("ATONLAB mail: %s"), *FString(UTF8_TO_TCHAR(Error.c_str())));
			}
		}
		return List;
	}

	// ---------------------------------------------------------------- Сеанс: guest, or the duty shift by its password

	class SAtonSession : public SAtonProgram
	{
	public:
		SLATE_BEGIN_ARGS(SAtonSession) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			ChildSlot
			[
				Panel(AtonStyle::Paper, SAssignNew(Body, SVerticalBox), FMargin(22, 16))
			];
			Rebuild();
		}

		virtual TSharedPtr<SWidget> FocusTarget() const override { return Password; }

		virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
		{
			SAtonProgram::Tick(G, Time, Delta);
			// the session may change from outside this window (the developer mode's button)
			if (StateOf(Shell).bStaff != bShownStaff)
			{
				Rebuild();
			}
		}

	private:
		void Rebuild()
		{
			const AtonLab::FLabState& S = StateOf(Shell);
			bShownStaff = S.bStaff;
			Password.Reset();
			Body->ClearChildren();
			Body->AddSlot().AutoHeight()[Label(L(TEXT("ui.aton.lab.session.title")), 22, AtonStyle::Ink, true)];
			Body->AddSlot().AutoHeight().Padding(0, 2, 0, 0)[Label(L(TEXT("ui.aton.lab.session.net")), 13, AtonStyle::InkSoft)];
			Body->AddSlot().AutoHeight().Padding(0, 10)[Divider()];
			if (S.bStaff)
			{
				Body->AddSlot().AutoHeight()[Label(L(TEXT("ui.aton.lab.session.user_staff")), 16, AtonStyle::Ink, true)];
				Body->AddSlot().AutoHeight().Padding(0, 8)[Label(L(TEXT("ui.aton.lab.session.open_staff")), 14)];
				Body->AddSlot().AutoHeight().Padding(0, 10, 0, 0).HAlign(HAlign_Left)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Button(L(TEXT("ui.aton.lab.session.to_mail")), [this] { Shell->OpenApp(AtonApps::LabMail); })]
					+ SHorizontalBox::Slot().AutoWidth()[Button(L(TEXT("ui.aton.lab.session.logout")), [this] { StateOf(Shell).bStaff = false; Rebuild(); })]
				];
				return;
			}
			Body->AddSlot().AutoHeight()[Label(L(TEXT("ui.aton.lab.session.user_guest")), 16, AtonStyle::Ink, true)];
			Body->AddSlot().AutoHeight().Padding(0, 8)[Label(L(TEXT("ui.aton.lab.session.open_guest")), 14)];
			Body->AddSlot().AutoHeight().Padding(0, 10, 0, 4)[Label(L(TEXT("ui.aton.lab.session.password")), 14, AtonStyle::InkSoft)];
			Body->AddSlot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					Panel(AtonStyle::Ink,
						Panel(AtonStyle::White,
							SAssignNew(Password, SEditableText)
							.Style(&AtonStyle::EditText(false))
							.IsPassword(true)
							.ClearKeyboardFocusOnCommit(false)
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
							{
								if (How == ETextCommit::OnEnter)
								{
									TryLogin(Text.ToString());
								}
							}),
							FMargin(8, 5)),
						FMargin(1))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					Button(L(TEXT("ui.aton.lab.session.login")), [this] { if (Password.IsValid()) { TryLogin(Password->GetText().ToString()); } })
				]
			];
			Body->AddSlot().AutoHeight().Padding(0, 8, 0, 0)
			[
				SNew(STextBlock).Font(AtonStyle::Font(14)).ColorAndOpacity(AtonStyle::C(0xA0, 0x30, 0x20)).AutoWrapText(true)
				.Text_Lambda([this] { return Message; })
			];
		}

		void TryLogin(const FString& Typed)
		{
			AtonLab::FLabState& S = StateOf(Shell);
			if (AtonLab::PasswordOk(std::string(TCHAR_TO_UTF8(*Typed))))
			{
				S.bStaff = true;
				S.FailedLogins = 0;
				Message = FText::GetEmpty();
				Report(Shell, TEXT("login"));
				Rebuild();
				Shell->OpenApp(AtonApps::LabMail);
				return;
			}
			S.FailedLogins++;
			// after a couple of misses the machine itself says where to look
			Message = T(L(S.FailedLogins >= 2 ? TEXT("ui.aton.lab.session.wrong_hint") : TEXT("ui.aton.lab.session.wrong")));
			if (Password.IsValid())
			{
				Password->SetText(FText::GetEmpty());
			}
		}

		IAtonShell* Shell = nullptr;
		bool bShownStaff = false;
		FText Message;
		TSharedPtr<SVerticalBox> Body;
		TSharedPtr<SEditableText> Password;
	};

	// ---------------------------------------------------------------- Почта ЛАБ-2: the lab's old correspondence

	class SAtonLabMail : public SAtonProgram
	{
	public:
		SLATE_BEGIN_ARGS(SAtonLabMail) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(230)[Panel(Side, SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(List, SVerticalBox)])]
				]
				+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1)[Panel(AtonStyle::PaperLine, SNullWidget::NullWidget)]]
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					Panel(AtonStyle::Paper, SAssignNew(BodyScroll, SScrollBox) + SScrollBox::Slot().Padding(18, 14)[SAssignNew(Body, SVerticalBox)])
				]
			];
			RebuildList();
			const std::vector<const AtonLab::FLabLetter*> Guest = StateOf(Shell).Visible(Letters(), "guest");
			if (!Guest.empty())
			{
				Show(Guest.front()->Id);
			}
		}

		virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
		{
			SAtonProgram::Tick(G, Time, Delta);
			const AtonLab::FLabState& S = StateOf(Shell);
			if (S.bStaff != bShownStaff || (int32)S.Read.size() != ShownRead)
			{
				RebuildList();
			}
		}

	private:
		static FString Part(const std::string& Id, const TCHAR* What)
		{
			return L(FString::Printf(TEXT("ui.aton.lab.mail.%s.%s"), *FString(UTF8_TO_TCHAR(Id.c_str())), What));
		}

		void RebuildList()
		{
			const AtonLab::FLabState& S = StateOf(Shell);
			bShownStaff = S.bStaff;
			ShownRead = (int32)S.Read.size();
			List->ClearChildren();
			for (const char* Box : { "guest", "staff" })
			{
				const bool bStaffBox = FCStringAnsi::Strcmp(Box, "staff") == 0;
				List->AddSlot().AutoHeight().Padding(10, 10, 10, 4)
				[
					Label(L(bStaffBox ? TEXT("ui.aton.lab.mail.box_staff") : TEXT("ui.aton.lab.mail.box_guest")), 12, AtonStyle::InkSoft, true)
				];
				if (bStaffBox && !S.bStaff)
				{
					List->AddSlot().AutoHeight().Padding(10, 2)[Label(L(TEXT("ui.aton.lab.mail.locked")), 13, AtonStyle::InkSoft)];
					continue;
				}
				for (const AtonLab::FLabLetter* Letter : S.Visible(Letters(), Box))
				{
					const std::string Id = Letter->Id;
					List->AddSlot().AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(AtonStyle::Flat())
						.BorderBackgroundColor_Lambda([this, Id] { return Selected == Id ? AtonStyle::Select : FLinearColor::Transparent; })
						.Padding(0)
						[
							SNew(SButton)
							.ButtonStyle(&AtonStyle::Ghost())
							.ContentPadding(FMargin(10, 6))
							.OnClicked_Lambda([this, Id] { Show(Id); return FReply::Handled(); })
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock)
									.Text(T(Part(Id, TEXT("from"))))
									.Font(AtonStyle::Font(14, S.Read.count(Id) == 0))
									.ColorAndOpacity(AtonStyle::Ink)
								]
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Text(T(Part(Id, TEXT("subject")))).Font(AtonStyle::Font(13)).ColorAndOpacity(AtonStyle::InkSoft).AutoWrapText(true)
								]
							]
						]
					];
				}
			}
		}

		void Show(const std::string& Id)
		{
			AtonLab::FLabState& S = StateOf(Shell);
			Selected = Id;
			const bool bNew = S.Read.insert(Id).second;
			Body->ClearChildren();
			Body->AddSlot().AutoHeight()[Label(Part(Id, TEXT("subject")), 20, AtonStyle::Ink, true)];
			Body->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
			[
				Label(L(TEXT("ui.aton.mail.from_line"), { FStringFormatArg(Part(Id, TEXT("from"))), FStringFormatArg(Part(Id, TEXT("date"))) }), 13, AtonStyle::InkSoft)
			];
			Body->AddSlot().AutoHeight()[Label(L(TEXT("ui.aton.lab.mail.to_line"), { FStringFormatArg(Part(Id, TEXT("to"))) }), 13, AtonStyle::InkSoft)];
			Body->AddSlot().AutoHeight().Padding(0, 10)[Divider()];
			Body->AddSlot().AutoHeight()[Label(Part(Id, TEXT("body")), 15)];
			BodyScroll->ScrollToStart();
			if (bNew)
			{
				// a read letter may open the next one of its thread
				RebuildList();
				if (S.bStaff && S.Read.size() >= Letters().size())
				{
					Report(Shell, TEXT("mail_all"));
				}
			}
		}

		IAtonShell* Shell = nullptr;
		bool bShownStaff = false;
		int32 ShownRead = 0;
		std::string Selected;
		TSharedPtr<SVerticalBox> List;
		TSharedPtr<SVerticalBox> Body;
		TSharedPtr<SScrollBox> BodyScroll;
	};

	// ---------------------------------------------------------------- «Ночная вахта»: the text adventure

	class SAtonAdventure : public SAtonProgram
	{
	public:
		SLATE_BEGIN_ARGS(SAtonAdventure) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			ChildSlot
			[
				Panel(AtonStyle::Console,
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1)
					[
						SAssignNew(Scroll, SScrollBox)
						+ SScrollBox::Slot()[SAssignNew(Output, SVerticalBox)]
						+ SScrollBox::Slot().Padding(0, 10, 0, 0)[SAssignNew(ChoiceBox, SVerticalBox)]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock).Text(T(TEXT("> "))).Font(AtonStyle::Mono(15)).ColorAndOpacity(AtonStyle::Green)
						]
						+ SHorizontalBox::Slot().FillWidth(1)
						[
							SAssignNew(Input, SEditableText)
							.Style(&AtonStyle::EditText(true))
							.ClearKeyboardFocusOnCommit(false)
							.HintText(LyudText::Get(TEXT("ui.aton.advui.hint")))
							.OnTextChanged_Lambda([this](const FText& Text) { Typed(Text.ToString(), false); })
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
							{
								if (How == ETextCommit::OnEnter)
								{
									Typed(Text.ToString(), true);
								}
							})
						]
					],
					FMargin(18, 14))
			];

			std::string Error;
			bLoaded = Game.Load(LoadUtf8(TEXT("AtonOS/adventure.json")), &Error);
			if (!bLoaded)
			{
				UE_LOG(LogTemp, Warning, TEXT("ATONLAB adventure: %s"), *FString(UTF8_TO_TCHAR(Error.c_str())));
				Print(L(TEXT("ui.aton.advui.broken")));
				return;
			}
			Print(LK(Game.TitleKey()).ToUpper());
			Print(L(TEXT("ui.aton.advui.intro")));
			Print(FString());
			const std::string& Kept = StateOf(Shell).AdvSave;
			if (!Kept.empty() && Game.Restore(Kept) && !Game.Ended())
			{
				Print(L(TEXT("ui.aton.advui.resumed"), { FStringFormatArg(Game.Moves()) }));
				Print(FString());
			}
			else
			{
				Game.Start();
			}
			PrintOutput();
			ShowChoices();
		}

		virtual TSharedPtr<SWidget> FocusTarget() const override { return Input; }

	private:
		void Print(const FString& Line, const FLinearColor& Color = AtonStyle::Green)
		{
			Output->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[MonoLabel(Line, 15, Color)];
		}

		void PrintOutput()
		{
			for (const std::string& Key : Game.Output())
			{
				Print(LK(Key));
			}
			Scroll->ScrollToEnd();
		}

		void ShowChoices()
		{
			ChoiceBox->ClearChildren();
			TArray<FString> Lines;
			if (Game.Ended())
			{
				const AtonLab::FLabState& S = StateOf(Shell);
				ChoiceBox->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
				[
					MonoLabel(L(TEXT("ui.aton.advui.ending"), { FStringFormatArg(LK("ui.aton.adv.end." + Game.Ending())) }), 15, AtonStyle::White)
				];
				ChoiceBox->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
				[
					MonoLabel(L(TEXT("ui.aton.advui.found"), { FStringFormatArg((int32)S.Endings.size()), FStringFormatArg((int32)Game.Endings().size()) }), 15, AtonStyle::Green)
				];
				Lines.Add(L(TEXT("ui.aton.advui.again")));
			}
			else
			{
				for (const std::string& Key : Game.Choices())
				{
					Lines.Add(LK(Key));
				}
			}
			ShownChoices = Lines;
			for (int32 I = 0; I < Lines.Num(); ++I)
			{
				const int32 Number = I + 1;
				ChoiceBox->AddSlot().AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(&AtonStyle::Ghost())
					.ContentPadding(FMargin(0, 3))
					.IsFocusable(false)
					.OnClicked_Lambda([this, Number] { Pick(Number); return FReply::Handled(); })
					[
						SNew(STextBlock)
						.Text(T(FString::Printf(TEXT("%d. %s"), Number, *Lines[I])))
						.Font(AtonStyle::Mono(15))
						.ColorAndOpacity(AtonStyle::C(0xE6, 0xF0, 0xB0))
						.AutoWrapText(true)
					]
				];
			}
			Scroll->ScrollToEnd();
		}

		/** A digit picks at once while there are at most nine choices; longer numbers wait for Enter. */
		void Typed(const FString& Text, bool bEnter)
		{
			if (bTyping || !bLoaded)
			{
				return;
			}
			const FString Clean = Text.TrimStartAndEnd();
			if (Clean.IsEmpty() || !Clean.IsNumeric())
			{
				if (bEnter || !Clean.IsEmpty())
				{
					ClearInput();
				}
				return;
			}
			if (bEnter || ShownChoices.Num() <= 9)
			{
				const int32 Number = FCString::Atoi(*Clean);
				ClearInput();
				Pick(Number);
			}
		}

		void ClearInput()
		{
			bTyping = true;
			Input->SetText(FText::GetEmpty());
			bTyping = false;
		}

		void Pick(int32 Number)
		{
			if (!bLoaded || Number < 1 || Number > ShownChoices.Num())
			{
				return;
			}
			AtonLab::FLabState& S = StateOf(Shell);
			Print(FString());
			Print(FString::Printf(TEXT("> %d. %s"), Number, *ShownChoices[Number - 1]), AtonStyle::C(0xE6, 0xF0, 0xB0));
			if (Game.Ended())
			{
				Print(TEXT("* * *"));
				Game.Start();
			}
			else if (!Game.Choose(Number))
			{
				return;
			}
			PrintOutput();
			if (Game.Ended())
			{
				S.AdvSave.clear();
				const bool bFirst = S.Endings.insert(Game.Ending()).second;
				if (bFirst)
				{
					Report(Shell, TEXT("night_watch"));
				}
				if (Game.Ending() == Game.BestEnding())
				{
					Report(Shell, TEXT("night_watch_best"));
				}
			}
			else
			{
				S.AdvSave = Game.Save();
			}
			ShowChoices();
		}

		IAtonShell* Shell = nullptr;
		AtonLab::Adv::FAdventure Game;
		bool bLoaded = false;
		bool bTyping = false;
		TArray<FString> ShownChoices;
		TSharedPtr<SScrollBox> Scroll;
		TSharedPtr<SVerticalBox> Output;
		TSharedPtr<SVerticalBox> ChoiceBox;
		TSharedPtr<SEditableText> Input;
	};

	// ---------------------------------------------------------------- Шахматы: the hero plays White against the machine

	class SAtonChess : public SAtonProgram
	{
	public:
		SLATE_BEGIN_ARGS(SAtonChess) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			AtonLab::FLabState& S = StateOf(Shell);
			Level = FMath::Clamp(S.ChessLevel, 1, 3);
			if (!S.ChessSave.empty() && Game.Restore(S.ChessSave) && Game.Result() == AtonLab::Chess::EChessResult::Playing)
			{
				Sheet.Add(L(TEXT("ui.aton.chess.resumed"), { FStringFormatArg(Game.Plies() / 2 + 1) }));
				bThinking = !Game.WhiteToMove();
			}
			else
			{
				Game.Reset();
			}
			SheetChanged();

			TSharedRef<SUniformGridPanel> Board = SNew(SUniformGridPanel);
			for (int32 Row = 0; Row < 8; ++Row)
			{
				for (int32 Col = 0; Col < 8; ++Col)
				{
					// White at the bottom: the top row is the eighth rank
					const int32 Sq = (7 - Row) * 8 + Col;
					Board->AddSlot(Col, Row)
					[
						SNew(SBox).WidthOverride(CellSize).HeightOverride(CellSize)
						[
							SNew(SBorder)
							.BorderImage(AtonStyle::Flat())
							.BorderBackgroundColor_Lambda([this, Sq] { return SquareColor(Sq); })
							.Padding(0)
							[
								SNew(SButton)
								.ButtonStyle(&AtonStyle::Ghost())
								.IsFocusable(false)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this, Sq] { Click(Sq); return FReply::Handled(); })
								[
									SNew(STextBlock)
									.Font(AtonStyle::Font(26, true))
									.Text_Lambda([this, Sq] { return PieceText(Sq); })
									.ColorAndOpacity_Lambda([this, Sq] { return FSlateColor(FChar::IsUpper(Game.At(Sq)) ? AtonStyle::White : AtonStyle::Ink); })
									.ShadowOffset(FVector2D(1, 1))
									.ShadowColorAndOpacity_Lambda([this, Sq] { return FChar::IsUpper(Game.At(Sq)) ? FLinearColor(0, 0, 0, 0.75f) : FLinearColor(1, 1, 1, 0.25f); })
								]
							]
						]
					];
				}
			}

			TSharedRef<SHorizontalBox> Levels = SNew(SHorizontalBox);
			for (int32 N = 1; N <= 3; ++N)
			{
				Levels->AddSlot().AutoWidth().Padding(0, 0, 4, 0)
				[
					SNew(SBorder)
					.BorderImage(AtonStyle::Flat())
					.BorderBackgroundColor_Lambda([this, N] { return Level == N ? AtonStyle::Ink : FLinearColor::Transparent; })
					.Padding(2)
					[
						Button(FString::FromInt(N), [this, N] { Level = N; StateOf(Shell).ChessLevel = N; }, 14)
					]
				];
			}

			ChildSlot
			[
				Panel(Side,
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
					[
						Panel(AtonStyle::Ink, Board, FMargin(2))
					]
					+ SHorizontalBox::Slot().FillWidth(1).Padding(12, 0, 0, 0)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Font(AtonStyle::Font(15, true)).ColorAndOpacity(AtonStyle::Ink).AutoWrapText(true)
							.Text_Lambda([this] { return T(Status()); })
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 2)[Label(L(TEXT("ui.aton.chess.level")), 13, AtonStyle::InkSoft)]
						+ SVerticalBox::Slot().AutoHeight()[Levels]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
						[
							SNew(STextBlock).Font(AtonStyle::Font(13)).ColorAndOpacity(AtonStyle::InkSoft).AutoWrapText(true)
							.Text_Lambda([this] { return T(Score()); })
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Divider(AtonStyle::BarDim)]
						+ SVerticalBox::Slot().FillHeight(1)
						[
							Panel(AtonStyle::Paper,
								SAssignNew(SheetScroll, SScrollBox)
								+ SScrollBox::Slot().Padding(8, 6)
								[
									SNew(STextBlock).Font(AtonStyle::Mono(13)).ColorAndOpacity(AtonStyle::Ink).AutoWrapText(true)
									.Text_Lambda([this] { return SheetText; })
								])
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(L(TEXT("ui.aton.chess.new")), [this] { NewGame(); }, 13)]
							+ SHorizontalBox::Slot().AutoWidth()[Button(L(TEXT("ui.aton.chess.undo")), [this] { TakeBack(); }, 13)]
						]
					],
					FMargin(10))
			];
		}

		virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
		{
			SAtonProgram::Tick(G, Time, Delta);
			if (!bThinking)
			{
				return;
			}
			if (ThinkAt <= 0.0)
			{
				// a breath before the answer, so the hero's own move is seen on the board first
				ThinkAt = Time + 0.4;
				return;
			}
			if (Time < ThinkAt)
			{
				return;
			}
			bThinking = false;
			ThinkAt = 0.0;
			if (Game.Result() != AtonLab::Chess::EChessResult::Playing || Game.WhiteToMove())
			{
				return;
			}
			const AtonLab::FLabState& S = StateOf(Shell);
			const unsigned Seed = (unsigned)(Game.Plies() * 31 + Level * 1009 + S.ChessWon[Level - 1] * 7 + S.ChessLost[Level - 1] * 13 + S.ChessDrawn[Level - 1] * 17);
			const AtonLab::Chess::FChessMove Move = Game.BestMove(Level, Seed);
			if (Move.Valid())
			{
				Made(Move);
			}
		}

	private:
		static constexpr float CellSize = 50.f;

		FLinearColor SquareColor(int32 Sq) const
		{
			const bool bLight = ((Sq / 8) + (Sq % 8)) % 2 == 1;
			FLinearColor Color = bLight ? AtonStyle::C(0xC9, 0xBB, 0x8E) : AtonStyle::C(0x6F, 0x8F, 0x62);
			const AtonLab::Chess::FChessMove Last = Game.LastMove();
			if (Last.Valid() && (Sq == Last.From || Sq == Last.To))
			{
				Color = FMath::Lerp(Color, AtonStyle::C(0xF2, 0xD9, 0x4E), 0.45f);
			}
			if (Targets.Contains(Sq))
			{
				Color = FMath::Lerp(Color, AtonStyle::C(0x3B, 0xA7, 0xC9), 0.55f);
			}
			if (Sq == Selected)
			{
				Color = AtonStyle::C(0xF2, 0xD9, 0x4E);
			}
			return Color;
		}

		FText PieceText(int32 Sq) const
		{
			// the international letters, the same in every language (the score sheet uses them too); made once, since
			// the 64 squares ask for their text on every repaint
			static const FString Kinds(TEXT("KQRBNP"));
			static const FText PieceLetters[] = { T(TEXT("K")), T(TEXT("Q")), T(TEXT("R")), T(TEXT("B")), T(TEXT("N")), T(TEXT("P")) };
			int32 Index = INDEX_NONE;
			Kinds.FindChar(FChar::ToUpper(TCHAR(Game.At(Sq))), Index);
			return Index == INDEX_NONE ? FText::GetEmpty() : PieceLetters[Index];
		}

		FString Status() const
		{
			using AtonLab::Chess::EChessResult;
			switch (Game.Result())
			{
			case EChessResult::WhiteMates: return L(TEXT("ui.aton.chess.won"));
			case EChessResult::BlackMates: return L(TEXT("ui.aton.chess.lost"));
			case EChessResult::Stalemate: return L(TEXT("ui.aton.chess.stalemate"));
			case EChessResult::DrawFifty:
			case EChessResult::DrawRepetition:
			case EChessResult::DrawMaterial: return L(TEXT("ui.aton.chess.draw"));
			default: break;
			}
			if (bThinking)
			{
				return L(TEXT("ui.aton.chess.thinking"));
			}
			return L(Game.InCheck() ? TEXT("ui.aton.chess.your_move_check") : TEXT("ui.aton.chess.your_move"));
		}

		FString Score() const
		{
			const AtonLab::FLabState& S = StateOf(Shell);
			return L(TEXT("ui.aton.chess.score"), { FStringFormatArg(Level), FStringFormatArg(S.ChessWon[Level - 1]), FStringFormatArg(S.ChessDrawn[Level - 1]),
				FStringFormatArg(S.ChessLost[Level - 1]) });
		}

		void Click(int32 Sq)
		{
			if (bThinking || Game.Result() != AtonLab::Chess::EChessResult::Playing || !Game.WhiteToMove())
			{
				return;
			}
			if (Selected >= 0 && Targets.Contains(Sq))
			{
				AtonLab::Chess::FChessMove Move;
				Move.From = Selected;
				Move.To = Sq;
				Selected = -1;
				Targets.Reset();
				Made(Move);
				return;
			}
			Selected = -1;
			Targets.Reset();
			if (FChar::IsUpper(Game.At(Sq)))
			{
				for (const AtonLab::Chess::FChessMove& Move : Game.LegalMovesFrom(Sq))
				{
					Targets.AddUnique(Move.To);
				}
				if (Targets.Num() > 0)
				{
					Selected = Sq;
				}
			}
		}

		/** The score sheet's text, joined when it changes rather than on every repaint. */
		void SheetChanged()
		{
			SheetText = T(FString::Join(Sheet, TEXT("\n")));
		}

		/** Plays a move of either side: the score sheet, the save, the end of the game, the machine's turn. */
		void Made(const AtonLab::Chess::FChessMove& Move)
		{
			using AtonLab::Chess::EChessResult;
			const bool bWhite = Game.WhiteToMove();
			const int32 Number = Game.Plies() / 2 + 1;
			const FString San = FString(UTF8_TO_TCHAR(Game.Notation(Move).c_str()));
			if (!Game.Play(Move))
			{
				return;
			}
			if (bWhite)
			{
				Sheet.Add(FString::Printf(TEXT("%d. %s"), Number, *San));
			}
			else if (Sheet.Num() > 0 && !Sheet.Last().Contains(TEXT("  ")) && Sheet.Last().StartsWith(FString::Printf(TEXT("%d. "), Number)))
			{
				Sheet.Last() += TEXT("  ") + San;
			}
			else
			{
				Sheet.Add(FString::Printf(TEXT("%d. ...  %s"), Number, *San));
			}
			SheetChanged();
			SheetScroll->ScrollToEnd();

			AtonLab::FLabState& S = StateOf(Shell);
			const EChessResult Result = Game.Result();
			if (Result == EChessResult::Playing)
			{
				S.ChessSave = Game.Save();
				bThinking = !Game.WhiteToMove();
				ThinkAt = 0.0;
				return;
			}
			// the game is over: counted once, and not kept for the next sitting
			S.ChessSave.clear();
			bThinking = false;
			if (Result == EChessResult::WhiteMates)
			{
				S.ChessWon[Level - 1]++;
				Report(Shell, TEXT("chess_win"));
				if (Level == 3)
				{
					Report(Shell, TEXT("chess_win_3"));
				}
			}
			else if (Result == EChessResult::BlackMates)
			{
				S.ChessLost[Level - 1]++;
			}
			else
			{
				S.ChessDrawn[Level - 1]++;
			}
		}

		void NewGame()
		{
			Game.Reset();
			Sheet.Reset();
			SheetChanged();
			Selected = -1;
			Targets.Reset();
			bThinking = false;
			ThinkAt = 0.0;
			StateOf(Shell).ChessSave.clear();
		}

		/** The hero's last move and the machine's answer come back. */
		void TakeBack()
		{
			if (bThinking || Game.Result() != AtonLab::Chess::EChessResult::Playing || !Game.WhiteToMove() || Game.Plies() < 2)
			{
				return;
			}
			if (Game.Undo() && Game.Undo())
			{
				if (Sheet.Num() > 0)
				{
					Sheet.Pop();
					SheetChanged();
				}
				Selected = -1;
				Targets.Reset();
				StateOf(Shell).ChessSave = Game.Save();
			}
		}

		IAtonShell* Shell = nullptr;
		AtonLab::Chess::FChessGame Game;
		int32 Level = 1;
		int32 Selected = -1;
		TArray<int32> Targets;
		TArray<FString> Sheet;
		FText SheetText;
		bool bThinking = false;
		double ThinkAt = 0.0;
		TSharedPtr<SScrollBox> SheetScroll;
	};

	// ---------------------------------------------------------------- shared by the two programs that are tables of live values

	/** Rows of (name, value) that are built again only when their words change. */
	class SAtonTable : public SAtonProgram
	{
	public:
		struct FRow
		{
			/** 0 a (name, value) row, 1 a heading, 2 a note over the whole width, 3 a row with a button. */
			int32 Kind = 0;
			FString Name, Value, ButtonText;
			TFunction<void()> OnButton;
		};

	protected:
		void BuildTable(const FLinearColor& Back = AtonStyle::Paper)
		{
			ChildSlot
			[
				Panel(Back, SNew(SScrollBox) + SScrollBox::Slot().Padding(18, 12)[SAssignNew(Rows, SVerticalBox)])
			];
		}

		virtual void Fill(TArray<FRow>& Out) = 0;

		void Refresh()
		{
			TArray<FRow> Now;
			Fill(Now);
			FString Signature;
			for (const FRow& R : Now)
			{
				Signature += FString::Printf(TEXT("%d|%s|%s|%s\n"), R.Kind, *R.Name, *R.Value, *R.ButtonText);
			}
			if (Signature == Shown)
			{
				return;
			}
			Shown = Signature;
			Rows->ClearChildren();
			for (const FRow& R : Now)
			{
				if (R.Kind == 1)
				{
					Rows->AddSlot().AutoHeight().Padding(0, 10, 0, 4)[Label(R.Name, 17, AtonStyle::Ink, true)];
					Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[Divider()];
				}
				else if (R.Kind == 2)
				{
					Rows->AddSlot().AutoHeight().Padding(0, 2)[Label(R.Name, 14, AtonStyle::InkSoft)];
				}
				else
				{
					TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)[Label(R.Name, 14, AtonStyle::InkSoft)]
						+ SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Center)[Label(R.Value, 15)];
					if (R.Kind == 3 && R.OnButton)
					{
						const TFunction<void()> Do = R.OnButton;
						Line->AddSlot().AutoWidth().Padding(8, 0, 0, 0)[Button(R.ButtonText, [this, Do] { Do(); Refresh(); }, 13)];
					}
					Rows->AddSlot().AutoHeight().Padding(0, 3)[Line];
				}
			}
		}

	public:
		virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
		{
			SAtonProgram::Tick(G, Time, Delta);
			if (Time - LastRefresh > 2.0)
			{
				LastRefresh = Time;
				Refresh();
			}
		}

	protected:
		IAtonShell* Shell = nullptr;

	private:
		TSharedPtr<SVerticalBox> Rows;
		FString Shown;
		double LastRefresh = 0.0;
	};

	// ---------------------------------------------------------------- Метеостанция: the mast's readings, the game's own weather

	class SAtonWeather : public SAtonTable
	{
	public:
		SLATE_BEGIN_ARGS(SAtonWeather) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			BuildTable();
			Refresh();
		}

	protected:
		static FString Signed(float Value)
		{
			const float Rounded = FMath::RoundToFloat(Value * 10.f) / 10.f;
			return FString::Printf(TEXT("%s%.1f"), Rounded > 0.f ? TEXT("+") : (Rounded < 0.f ? TEXT("\u2212") : TEXT("")), FMath::Abs(Rounded));
		}

		static FString PrecipName(const FLyudWeatherState& W)
		{
			return L(FString::Printf(TEXT("ui.aton.wx.precip.%d"), int32(W.Precip)));
		}

		virtual void Fill(TArray<FRow>& Out) override
		{
			auto Head = [&Out](const FString& Text) { FRow R; R.Kind = 1; R.Name = Text; Out.Add(R); };
			auto Note = [&Out](const FString& Text) { FRow R; R.Kind = 2; R.Name = Text; Out.Add(R); };
			auto Row = [&Out](const TCHAR* Key, const FString& Value) { FRow R; R.Name = L(Key); R.Value = Value; Out.Add(R); };

			UWorld* World = Shell ? Shell->ShellWorld() : nullptr;
			const ULyudWeatherSubsystem* Weather = World ? ULyudWeatherSubsystem::Get(World) : nullptr;
			Head(L(TEXT("ui.aton.wx.title")));
			if (!Weather)
			{
				Note(L(TEXT("ui.aton.wx.offline")));
				return;
			}
			const FDateTime Now = Weather->WeatherTime();
			const FLyudWeatherState W = Weather->GetState();
			Note(L(TEXT("ui.aton.wx.at"), { FStringFormatArg(FString::Printf(TEXT("%02d.%02d"), Now.GetDay(), Now.GetMonth())),
				FStringFormatArg(FString::Printf(TEXT("%02d:%02d"), Now.GetHour(), Now.GetMinute())) }));

			Row(TEXT("ui.aton.wx.temp"), L(TEXT("ui.aton.wx.deg"), { FStringFormatArg(Signed(W.TempC)) }));
			Row(TEXT("ui.aton.wx.dew"), L(TEXT("ui.aton.wx.deg"), { FStringFormatArg(Signed(W.DewPointC)) }));
			Row(TEXT("ui.aton.wx.humidity"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(W.Humidity * 100.f)));
			const TCHAR* Trend = W.PressureTendency > 0.5f ? TEXT("ui.aton.wx.rising") : (W.PressureTendency < -0.5f ? TEXT("ui.aton.wx.falling") : TEXT("ui.aton.wx.steady"));
			Row(TEXT("ui.aton.wx.pressure"), L(TEXT("ui.aton.wx.hpa"), { FStringFormatArg(FMath::RoundToInt(W.PressureHPa)), FStringFormatArg(L(Trend)) }));
			Row(TEXT("ui.aton.wx.wind"), L(TEXT("ui.aton.wx.wind_value"), {
				FStringFormatArg(L(FString::Printf(TEXT("ui.aton.wx.dir.%d"), AtonLab::Compass8(W.WindFromDeg)))),
				FStringFormatArg(FString::Printf(TEXT("%.1f"), W.WindSpeed)),
				FStringFormatArg(AtonLab::Beaufort(W.WindSpeed)) }));
			Row(TEXT("ui.aton.wx.cloud"), L(TEXT("ui.aton.wx.oktas"), { FStringFormatArg(AtonLab::Oktas(W.CloudCover)) }));
			Row(TEXT("ui.aton.wx.precip"), W.PrecipRate > 0.05f
				? L(TEXT("ui.aton.wx.precip_rate"), { FStringFormatArg(PrecipName(W)), FStringFormatArg(FString::Printf(TEXT("%.1f"), W.PrecipRate)) })
				: PrecipName(W));
			Row(TEXT("ui.aton.wx.visibility"), W.VisibilityM >= 1000.f
				? L(TEXT("ui.aton.wx.km"), { FStringFormatArg(FMath::RoundToInt(W.VisibilityM / 1000.f)) })
				: L(TEXT("ui.aton.wx.m"), { FStringFormatArg(FMath::RoundToInt(W.VisibilityM / 10.f) * 10) }));
			Row(TEXT("ui.aton.wx.sea"), L(TEXT("ui.aton.wx.deg"), { FStringFormatArg(Signed(W.SeaTempC)) }));
			if (W.SnowDepthCm > 0.5f)
			{
				Row(TEXT("ui.aton.wx.snow"), L(TEXT("ui.aton.wx.cm"), { FStringFormatArg(FMath::RoundToInt(W.SnowDepthCm)) }));
			}
			// the pyranometer on the mast: the sun's height through today's cloud
			float SunAltitude = -90.f;
			if (TActorIterator<ALyudSky> It(World); It)
			{
				SunAltitude = It->SunAltitude();
			}
			Row(TEXT("ui.aton.wx.flux"), L(TEXT("ui.aton.wx.wm2"), { FStringFormatArg(FMath::RoundToInt(AtonLab::SolarFlux(SunAltitude, W.CloudCover))) }));
			Row(TEXT("ui.aton.wx.kp"), FString::Printf(TEXT("%.1f"), W.Kp));

			Head(L(TEXT("ui.aton.wx.forecast")));
			for (const int32 Ahead : { 6, 12, 18, 24 })
			{
				const FDateTime When = Now + FTimespan::FromHours(double(Ahead));
				const FLyudWeatherState F = Weather->GetStateAt(When);
				FRow R;
				R.Name = FString::Printf(TEXT("%02d.%02d  %02d:00"), When.GetDay(), When.GetMonth(), When.GetHour());
				R.Value = L(TEXT("ui.aton.wx.forecast_line"), { FStringFormatArg(Signed(F.TempC)), FStringFormatArg(PrecipName(F)), FStringFormatArg(AtonLab::Oktas(F.CloudCover)) });
				Out.Add(R);
			}
			Note(L(TEXT("ui.aton.wx.footer")));
		}
	};

	// ---------------------------------------------------------------- Очередь телескопа: four slots a night, one may be the hero's

	class SAtonScope : public SAtonTable
	{
	public:
		SLATE_BEGIN_ARGS(SAtonScope) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, IAtonShell* InShell)
		{
			Shell = InShell;
			BuildTable();
			Refresh();
		}

	protected:
		FString NightName(int32 Night) const
		{
			// a night is named by the morning it ends on
			const ULyudAtonLab* Lab = LabOf(Shell);
			const FDateTime Morning = Lab ? Lab->DayDate(Night + 1) : FDateTime(2026, 10, 21);
			return L(TEXT("ui.aton.scope.night"), { FStringFormatArg(FString::Printf(TEXT("%02d.%02d"), Morning.GetDay(), Morning.GetMonth())) });
		}

		static FString TargetName(int32 Target)
		{
			return L(FString(TEXT("ui.aton.scope.target.")) + UTF8_TO_TCHAR(AtonLab::ScopeTargetId(Target)));
		}

		virtual void Fill(TArray<FRow>& Out) override
		{
			auto Head = [&Out](const FString& Text) { FRow R; R.Kind = 1; R.Name = Text; Out.Add(R); };
			auto Note = [&Out](const FString& Text) { FRow R; R.Kind = 2; R.Name = Text; Out.Add(R); };

			ULyudAtonLab* Lab = LabOf(Shell);
			AtonLab::FLabState& S = StateOf(Shell);
			Head(L(TEXT("ui.aton.scope.title")));
			if (!S.bStaff)
			{
				Note(L(TEXT("ui.aton.scope.locked")));
				return;
			}
			if (Lab)
			{
				Lab->ResolveScope();
			}
			Note(L(TEXT("ui.aton.scope.rules")));
			const double NowHours = Lab ? Lab->NowHours() : 16.0;
			const int32 Tonight = AtonLab::ScopeNightOf(NowHours);
			for (int32 Night = Tonight; Night < Tonight + 3; ++Night)
			{
				Head(NightName(Night));
				const std::vector<AtonLab::FScopeSlot> Slots = AtonLab::ScopeNight(Night);
				const std::map<int, int>::const_iterator Mine = S.Bookings.find(Night);
				for (int32 Slot = 0; Slot < AtonLab::ScopeSlots; ++Slot)
				{
					const AtonLab::FScopeSlot& Info = Slots[(size_t)Slot];
					const bool bPassed = AtonLab::ScopeSlotAt(Night, Slot) <= NowHours;
					const bool bMine = Mine != S.Bookings.end() && Mine->second == Slot;
					FRow R;
					R.Name = FString::Printf(TEXT("%02d:00   %s"), AtonLab::ScopeSlotHour(Slot), *TargetName(Info.Target));
					if (bMine)
					{
						R.Value = L(TEXT("ui.aton.scope.yours"));
						if (!bPassed)
						{
							R.Kind = 3;
							R.ButtonText = L(TEXT("ui.aton.scope.unbook"));
							R.OnButton = [this, Night] { if (ULyudAtonLab* Net = LabOf(Shell)) { StateOf(Shell).Unbook(Night, Net->NowHours()); } };
						}
					}
					else if (Info.Owner >= 0)
					{
						R.Value = L(FString::Printf(TEXT("ui.aton.scope.staff.%d"), Info.Owner + 1));
					}
					else if (bPassed)
					{
						R.Value = L(TEXT("ui.aton.scope.unused"));
					}
					else
					{
						R.Value = L(TEXT("ui.aton.scope.free"));
						if (Mine == S.Bookings.end())
						{
							R.Kind = 3;
							R.ButtonText = L(TEXT("ui.aton.scope.book"));
							R.OnButton = [this, Night, Slot]
							{
								if (ULyudAtonLab* Net = LabOf(Shell))
								{
									if (StateOf(Shell).Book(Night, Slot, Net->NowHours()))
									{
										Net->Report(TEXT("scope_book"));
									}
								}
							};
						}
					}
					Out.Add(R);
				}
			}

			Head(L(TEXT("ui.aton.scope.journal")));
			if (S.Results.empty())
			{
				Note(L(TEXT("ui.aton.scope.journal_empty")));
			}
			for (int32 I = (int32)S.Results.size() - 1; I >= 0; --I)
			{
				const AtonLab::FScopeResult& Result = S.Results[(size_t)I];
				FRow R;
				R.Name = FString::Printf(TEXT("%s, %02d:00\n%s"), *NightName(Result.Night), AtonLab::ScopeSlotHour(Result.Slot), *TargetName(Result.Target));
				R.Value = Result.bClear
					? L(FString(TEXT("ui.aton.scope.result.")) + UTF8_TO_TCHAR(AtonLab::ScopeTargetId(Result.Target)))
					: L(FString::Printf(TEXT("ui.aton.scope.cloudy.%d"), 1 + FMath::Abs(Result.Night) % 2));
				Out.Add(R);
			}
		}
	};

	TSharedPtr<SAtonProgram> MakeProgram(FName Id, IAtonShell& Shell)
	{
		if (Id == AtonApps::Session) return SNew(SAtonSession, &Shell);
		if (Id == AtonApps::LabMail) return SNew(SAtonLabMail, &Shell);
		if (Id == AtonApps::Adventure) return SNew(SAtonAdventure, &Shell);
		if (Id == AtonApps::Chess) return SNew(SAtonChess, &Shell);
		if (Id == AtonApps::Weather) return SNew(SAtonWeather, &Shell);
		if (Id == AtonApps::Scope) return SNew(SAtonScope, &Shell);
		return nullptr;
	}
}
