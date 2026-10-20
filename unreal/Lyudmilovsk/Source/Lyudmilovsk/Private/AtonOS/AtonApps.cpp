#include "AtonApps.h"

#include "AtonStyle.h"
#include "SAtonDesktop.h"
#include "Algo/AllOf.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText T(const FString& S) { return FText::FromString(S); }

	TSharedRef<SWidget> Label(const FString& Text, int32 Size = 15, FLinearColor Color = AtonStyle::Ink, bool bBold = false)
	{
		return SNew(STextBlock).Text(T(Text)).Font(AtonStyle::Font(Size, bBold)).ColorAndOpacity(Color).AutoWrapText(true);
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

	/** A full-width row in a list; highlighted when selected. */
	TSharedRef<SWidget> Row(const FString& Text, TFunction<void()> OnClick, TFunction<bool()> IsSelected = nullptr, bool bBold = false)
	{
		return SNew(SBorder)
			.BorderImage(AtonStyle::Flat())
			.BorderBackgroundColor_Lambda([IsSelected] { return IsSelected && IsSelected() ? AtonStyle::Select : FLinearColor::Transparent; })
			.Padding(0)
			[
				SNew(SButton)
				.ButtonStyle(&AtonStyle::Ghost())
				.ContentPadding(FMargin(10, 5))
				.OnClicked_Lambda([OnClick] { OnClick(); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(T(Text)).Font(AtonStyle::Font(14, bBold)).ColorAndOpacity(AtonStyle::Ink)
				]
			];
	}

	TSharedRef<SWidget> Divider(const FLinearColor& Color = AtonStyle::PaperLine)
	{
		return SNew(SBox).HeightOverride(1)[Panel(Color, SNullWidget::NullWidget)];
	}
}

// ---------------------------------------------------------------- notes on disk

FString AtonApps::NotesDir() { return FPaths::ProjectSavedDir() / TEXT("AtonOS/Notes"); }
FString AtonApps::TrashDir() { return FPaths::ProjectSavedDir() / TEXT("AtonOS/Trash"); }

TArray<FString> AtonApps::ListNotes(const FString& Dir)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.txt")), true, false);
	Files.Sort();
	return Files;
}

static FString ReadNote(const FString& Dir, const FString& File)
{
	FString Text;
	FFileHelper::LoadFileToString(Text, *(Dir / File));
	return Text;
}

static void WriteNote(const FString& Dir, const FString& File, const FString& Text)
{
	FFileHelper::SaveStringToFile(Text, *(Dir / File), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

FString AtonApps::NoteTitle(const FString& Dir, const FString& File)
{
	FString First;
	ReadNote(Dir, File).TrimStartAndEnd().Split(TEXT("\n"), &First, nullptr);
	if (First.IsEmpty())
	{
		First = ReadNote(Dir, File).TrimStartAndEnd();
	}
	First = First.TrimStartAndEnd();
	if (First.IsEmpty())
	{
		return TEXT("без названия");
	}
	return First.Len() > 22 ? First.Left(21) + TEXT("…") : First;
}

static FString NewNoteFile(const FString& Dir)
{
	int32 Max = 0;
	for (const FString& F : AtonApps::ListNotes(Dir))
	{
		Max = FMath::Max(Max, FCString::Atoi(*FPaths::GetBaseFilename(F).RightChop(5)));
	}
	return FString::Printf(TEXT("note_%03d.txt"), Max + 1);
}

// ---------------------------------------------------------------- Блокнот

class SAtonNotes : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonNotes) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		Dir = AtonApps::NotesDir();
		IFileManager::Get().MakeDirectory(*Dir, true);
		if (AtonApps::ListNotes(Dir).Num() == 0)
		{
			WriteNote(Dir, TEXT("note_001.txt"), TEXT("Привет\n\nЭто Блокнот АТОН OS.\nВсё, что здесь написано, сохраняется само."));
		}

		TSharedRef<SScrollBar> Scroll = SNew(SScrollBar).Thickness(FVector2D(8, 8));
		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(200)
				[
					Panel(AtonStyle::C(0xE9, 0xE5, 0xD8),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(8)
						[
							Button(TEXT("+ Новая заметка"), [this] { CreateNote(); })
						]
						+ SVerticalBox::Slot().FillHeight(1)
						[
							SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(List, SVerticalBox)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8)
						[
							Button(TEXT("В корзину"), [this] { DeleteNote(); })
						])
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1)[Panel(AtonStyle::PaperLine, SNullWidget::NullWidget)]]
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				Panel(AtonStyle::Paper,
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1).Padding(16, 12, 6, 12)
					[
						SAssignNew(Editor, SMultiLineEditableText)
						.TextStyle(&AtonStyle::PaperText(16))
						.AutoWrapText(true)
						.VScrollBar(Scroll)
						.OnTextChanged_Lambda([this](const FText& Text) { Save(Text.ToString()); })
					]
					+ SHorizontalBox::Slot().AutoWidth()[Scroll])
			]
		];

		const TArray<FString> Notes = AtonApps::ListNotes(Dir);
		Select(Notes.Last());
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return Editor; }

	virtual void HandleArg(const FString& Arg) override
	{
		if (AtonApps::ListNotes(Dir).Contains(Arg))
		{
			Select(Arg);
		}
	}

	virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
	{
		SAtonProgram::Tick(G, Time, Delta);
		// Notes can come back from the trash while this window is open.
		if (Time - LastPoll > 1.0)
		{
			LastPoll = Time;
			if (AtonApps::ListNotes(Dir) != Shown)
			{
				RebuildList();
			}
		}
	}

private:
	void Select(const FString& File)
	{
		Current = File;
		bLoading = true;
		Editor->SetText(T(ReadNote(Dir, File)));
		bLoading = false;
		RebuildList();
	}

	void Save(const FString& Text)
	{
		if (bLoading || Current.IsEmpty())
		{
			return;
		}
		const FString OldTitle = AtonApps::NoteTitle(Dir, Current);
		WriteNote(Dir, Current, Text);
		if (AtonApps::NoteTitle(Dir, Current) != OldTitle)
		{
			RebuildList();
		}
	}

	void CreateNote()
	{
		const FString File = NewNoteFile(Dir);
		WriteNote(Dir, File, FString());
		Select(File);
		FSlateApplication::Get().SetAllUserFocus(Editor, EFocusCause::SetDirectly);
	}

	void DeleteNote()
	{
		if (Current.IsEmpty())
		{
			return;
		}
		const FString Trash = AtonApps::TrashDir();
		IFileManager::Get().MakeDirectory(*Trash, true);
		FString Target = Current;
		if (IFileManager::Get().FileExists(*(Trash / Target)))
		{
			Target = FString::Printf(TEXT("%s_%lld.txt"), *FPaths::GetBaseFilename(Current), FDateTime::Now().ToUnixTimestamp());
		}
		IFileManager::Get().Move(*(Trash / Target), *(Dir / Current));
		TArray<FString> Notes = AtonApps::ListNotes(Dir);
		if (Notes.Num() == 0)
		{
			CreateNote();
		}
		else
		{
			Select(Notes.Last());
		}
	}

	void RebuildList()
	{
		Shown = AtonApps::ListNotes(Dir);
		if (!Shown.Contains(Current) && Shown.Num() > 0)
		{
			Select(Shown.Last());
			return;
		}
		List->ClearChildren();
		for (int32 i = Shown.Num() - 1; i >= 0; --i)
		{
			const FString File = Shown[i];
			List->AddSlot().AutoHeight()
			[
				Row(AtonApps::NoteTitle(Dir, File), [this, File] { Select(File); }, [this, File] { return Current == File; })
			];
		}
	}

	FString Dir;
	FString Current;
	TArray<FString> Shown;
	bool bLoading = false;
	double LastPoll = 0.0;
	TSharedPtr<SVerticalBox> List;
	TSharedPtr<SMultiLineEditableText> Editor;
};

// ---------------------------------------------------------------- Корзина

class SAtonTrash : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonTrash) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot
		[
			Panel(AtonStyle::Paper,
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1).Padding(8)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(List, SVerticalBox)]
				]
				+ SVerticalBox::Slot().AutoHeight()[Divider()]
				+ SVerticalBox::Slot().AutoHeight().Padding(10).HAlign(HAlign_Right)
				[
					Button(TEXT("Очистить корзину"), [this]
					{
						for (const FString& F : AtonApps::ListNotes(AtonApps::TrashDir()))
						{
							IFileManager::Get().Delete(*(AtonApps::TrashDir() / F));
						}
						Rebuild();
					})
				])
		];
		Rebuild();
	}

	virtual void Tick(const FGeometry& G, const double Time, const float Delta) override
	{
		SAtonProgram::Tick(G, Time, Delta);
		if (Time - LastPoll > 1.0)
		{
			LastPoll = Time;
			if (AtonApps::ListNotes(AtonApps::TrashDir()) != Shown)
			{
				Rebuild();
			}
		}
	}

private:
	void Rebuild()
	{
		const FString Dir = AtonApps::TrashDir();
		Shown = AtonApps::ListNotes(Dir);
		List->ClearChildren();
		if (Shown.Num() == 0)
		{
			List->AddSlot().AutoHeight().Padding(10)[Label(TEXT("Корзина пуста."), 15, AtonStyle::InkSoft)];
			return;
		}
		for (const FString& File : Shown)
		{
			List->AddSlot().AutoHeight().Padding(4, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(6, 0)
				[
					Label(AtonApps::NoteTitle(Dir, File), 14)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					Button(TEXT("Вернуть"), [this, File]
					{
						const FString Notes = AtonApps::NotesDir();
						FString Target = File;
						if (IFileManager::Get().FileExists(*(Notes / Target)))
						{
							Target = NewNoteFile(Notes);
						}
						IFileManager::Get().Move(*(Notes / Target), *(AtonApps::TrashDir() / File));
						Rebuild();
					}, 13)
				]
			];
		}
	}

	TArray<FString> Shown;
	double LastPoll = 0.0;
	TSharedPtr<SVerticalBox> List;
};

// ---------------------------------------------------------------- Сеть (browser of the in-game network)

namespace
{
	/**
	 * Pages of the local network. One line per block:
	 *   "# Title", "## Subtitle", "[link text](address)", "---" rule, "?search" a search box, anything else a paragraph.
	 */
	const TMap<FString, FString>& Pages()
	{
		static const TMap<FString, FString> P = {
			{ TEXT("дом.атон"), TEXT(
				"# Добро пожаловать в Сеть\n"
				"Домашняя страница АТОН OS.\n"
				"?search\n"
				"---\n"
				"## Куда пойти\n"
				"[Новости Людмиловска](новости.лм)\n"
				"[Погода](погода.лм)\n"
				"[Справка АТОН OS](справка.атон)\n") },
			{ TEXT("новости.лм"), TEXT(
				"# Новости Людмиловска\n"
				"---\n"
				"## В подъезде снова поменяли лампочку\n"
				"Жильцы просят её не выкручивать. Предыдущую так и не нашли.\n"
				"## Движущаяся дорожка работает круглосуточно\n"
				"Администрация напоминает: дорожка не выключается. Так задумано.\n"
				"## Перепись жильцов перенесена\n"
				"Новую дату сообщат тем, кого найдут.\n"
				"---\n"
				"[На главную](дом.атон)\n") },
			{ TEXT("погода.лм"), TEXT(
				"# Погода в Людмиловске\n"
				"## Сейчас: +4°, туман\n"
				"Ночью: туман.\n"
				"Завтра: туман.\n"
				"Послезавтра: данных нет.\n"
				"---\n"
				"[На главную](дом.атон)\n") },
			{ TEXT("справка.атон"), TEXT(
				"# Справка АТОН OS\n"
				"Программы открываются одним щелчком по значку на рабочем столе. Окно можно перетащить за заголовок, а закрыть крестиком.\n"
				"## Программы\n"
				"Блокнот: заметки, которые сохраняются сами.\n"
				"Сеть: этот браузер. Адрес можно набрать в строке сверху.\n"
				"Терминал: команды. Наберите «помощь».\n"
				"Письма: входящая почта.\n"
				"Калькулятор: считает. Можно с клавиатуры.\n"
				"Корзина: удалённые заметки, их можно вернуть.\n"
				"Chosen: дуэль на мечах над пропастью. WASD, мышь с зажатой кнопкой справа: меч, Shift: щит, пробел: прыжок.\n"
				"---\n"
				"[На главную](дом.атон)\n") },
		};
		return P;
	}

	const FString HomePage = TEXT("дом.атон");
	const FString SearchPage = TEXT("поиск.атон");

	FString Normalize(FString Address)
	{
		Address = Address.TrimStartAndEnd().ToLower();
		Address.RemoveFromStart(TEXT("http://"));
		Address.RemoveFromStart(TEXT("https://"));
		Address.RemoveFromStart(TEXT("www."));
		Address.RemoveFromEnd(TEXT("/"));
		return Address;
	}
}

class SAtonBrowser : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				Panel(AtonStyle::C(0xE9, 0xE5, 0xD8),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(TEXT("‹ Назад"), [this] { Back(); }, 13)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Button(TEXT("Домой"), [this] { Navigate(HomePage); }, 13)]
					+ SHorizontalBox::Slot().FillWidth(1)
					[
						Panel(AtonStyle::White,
							SAssignNew(AddressBar, SEditableText)
							.Style(&AtonStyle::EditText(false))
							.SelectAllTextWhenFocused(true)
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
							{
								if (How == ETextCommit::OnEnter)
								{
									Navigate(Text.ToString());
								}
							}),
							FMargin(8, 4))
					],
					FMargin(8))
			]
			+ SVerticalBox::Slot().AutoHeight()[Divider()]
			+ SVerticalBox::Slot().FillHeight(1)
			[
				Panel(AtonStyle::Paper, SAssignNew(Page, SScrollBox))
			]
		];
		Navigate(HomePage);
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return AddressBar; }
	virtual void HandleArg(const FString& Arg) override { Navigate(Arg); }

private:
	void Navigate(const FString& Address, bool bRemember = true)
	{
		const FString Addr = Normalize(Address).IsEmpty() ? HomePage : Normalize(Address);
		if (bRemember && (History.Num() == 0 || History.Last() != Addr))
		{
			History.Add(Addr);
		}
		AddressBar->SetText(T(Addr));
		Render(Addr);
	}

	void Back()
	{
		if (History.Num() > 1)
		{
			History.Pop();
			Navigate(History.Last(), false);
		}
	}

	void Render(const FString& Addr)
	{
		Page->ClearChildren();
		FString Path, Query;
		if (!Addr.Split(TEXT("?"), &Path, &Query))
		{
			Path = Addr;
		}

		FString Markup;
		if (Path == SearchPage)
		{
			Markup = SearchResults(FGenericPlatformHttpDecode(Query));
		}
		else if (const FString* Found = Pages().Find(Path))
		{
			Markup = *Found;
		}
		else
		{
			Markup = FString::Printf(TEXT("# Страница не найдена\nАдрес «%s» не отвечает. Проверьте, правильно ли он набран.\n?search\n[На главную](дом.атон)\n"), *Addr);
		}

		TArray<FString> Lines;
		Markup.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			Page->AddSlot().Padding(24, 4)[Block(Line)];
		}
		Page->AddSlot().Padding(0, 12)[SNullWidget::NullWidget];
		Page->ScrollToStart();
	}

	static FString FGenericPlatformHttpDecode(const FString& Query)
	{
		FString Q = Query;
		Q.RemoveFromStart(TEXT("q="));
		return Q.Replace(TEXT("+"), TEXT(" "));
	}

	/** Searches the text of every page; every word of the query must be on a page. */
	static FString SearchResults(const FString& Query)
	{
		TArray<FString> Words;
		Query.ToLower().ParseIntoArrayWS(Words);
		FString Out = FString::Printf(TEXT("# Поиск\n?search\n---\n"));
		if (Words.Num() == 0)
		{
			return Out + TEXT("Наберите, что искать.\n");
		}
		int32 Hits = 0;
		for (const TPair<FString, FString>& P : Pages())
		{
			const FString Body = P.Value.ToLower();
			if (Algo::AllOf(Words, [&Body](const FString& W) { return Body.Contains(W); }))
			{
				FString Title;
				P.Value.Split(TEXT("\n"), &Title, nullptr);
				Title.RemoveFromStart(TEXT("# "));
				Out += FString::Printf(TEXT("[%s](%s)\n"), *Title, *P.Key);
				Hits++;
			}
		}
		if (Hits == 0)
		{
			Out += FString::Printf(TEXT("По запросу «%s» ничего не найдено.\n"), *Query);
		}
		return Out;
	}

	TSharedRef<SWidget> Block(const FString& Line)
	{
		if (Line.StartsWith(TEXT("## ")))
		{
			return SNew(SBox).Padding(FMargin(0, 8, 0, 0))[Label(Line.RightChop(3), 17, AtonStyle::Ink, true)];
		}
		if (Line.StartsWith(TEXT("# ")))
		{
			return SNew(SBox).Padding(FMargin(0, 12, 0, 2))[Label(Line.RightChop(2), 24, AtonStyle::Ink, true)];
		}
		if (Line == TEXT("---"))
		{
			return SNew(SBox).Padding(FMargin(0, 6))[Divider()];
		}
		if (Line == TEXT("?search"))
		{
			TSharedPtr<SEditableText> Box;
			TSharedRef<SWidget> Bar = SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1)
				[
					Panel(AtonStyle::Ink,
						Panel(AtonStyle::White,
							SAssignNew(Box, SEditableText)
							.Style(&AtonStyle::EditText(false))
							.HintText(T(TEXT("искать в Сети")))
							.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
							{
								if (How == ETextCommit::OnEnter)
								{
									Navigate(SearchPage + TEXT("?q=") + Text.ToString().TrimStartAndEnd());
								}
							}),
							FMargin(8, 5)),
						FMargin(1))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
				[
					Button(TEXT("Найти"), [this, Box] { Navigate(SearchPage + TEXT("?q=") + Box->GetText().ToString().TrimStartAndEnd()); })
				];
			return SNew(SBox).Padding(FMargin(0, 8)).MaxDesiredWidth(520)[Bar];
		}
		FString Text, Address;
		if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT(")")) && Line.Split(TEXT("]("), &Text, &Address))
		{
			Text.RemoveFromStart(TEXT("["));
			Address.RemoveFromEnd(TEXT(")"));
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&AtonStyle::Ghost())
					.ContentPadding(FMargin(0, 2))
					.OnClicked_Lambda([this, Address] { Navigate(Address); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(T(TEXT("› ") + Text)).Font(AtonStyle::Font(16)).ColorAndOpacity(AtonStyle::Link)
					]
				];
		}
		if (Line.IsEmpty())
		{
			return SNew(SBox).HeightOverride(6);
		}
		return Label(Line, 15);
	}

	TArray<FString> History;
	TSharedPtr<SEditableText> AddressBar;
	TSharedPtr<SScrollBox> Page;
};

// ---------------------------------------------------------------- Терминал

class SAtonTerminal : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonTerminal) {}
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
					SAssignNew(Scroll, SScrollBox) + SScrollBox::Slot()[SAssignNew(Output, SVerticalBox)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
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
						.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type How)
						{
							if (How == ETextCommit::OnEnter)
							{
								Input->SetText(FText::GetEmpty());
								Run(Text.ToString().TrimStartAndEnd());
							}
						})
					]
				],
				FMargin(18, 14))
		];

		Print(TEXT("АТОН OS v1.0  (c) Людмиловск"));
		Print(TEXT(""));
		Print(TEXT("> проверка памяти ........ ОК"));
		Print(TEXT("> проверка памяти ........ ??"));
		Print(TEXT("> последний вход: никогда"));
		Print(TEXT("> пользователь не найден"));
		Print(TEXT(""));
		Print(TEXT("> ты проснулся?"));
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return Input; }

private:
	void Print(const FString& Line)
	{
		Output->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(T(Line)).Font(AtonStyle::Mono(15)).ColorAndOpacity(AtonStyle::Green).AutoWrapText(true)
		];
		Scroll->ScrollToEnd();
	}

	void Run(const FString& Line)
	{
		Print(TEXT("> ") + Line);
		if (Line.IsEmpty())
		{
			return;
		}
		FString Cmd = Line, Rest;
		Line.Split(TEXT(" "), &Cmd, &Rest);
		Cmd = Cmd.ToLower();
		Rest = Rest.TrimStartAndEnd();
		auto Is = [&Cmd](std::initializer_list<const TCHAR*> Names)
		{
			for (const TCHAR* N : Names)
			{
				if (Cmd == N)
				{
					return true;
				}
			}
			return false;
		};

		if (!bAnswered && Is({ TEXT("да"), TEXT("ага"), TEXT("yes"), TEXT("нет"), TEXT("no") }))
		{
			bAnswered = true;
			Print(Is({ TEXT("нет"), TEXT("no") }) ? TEXT("странно. ты же печатаешь.") : TEXT("хорошо. тогда начнём."));
			Print(TEXT("набери «помощь», если не знаешь, что делать."));
		}
		else if (Is({ TEXT("помощь"), TEXT("help"), TEXT("?") }))
		{
			Print(TEXT("команды:"));
			Print(TEXT("  помощь            этот список"));
			Print(TEXT("  открыть <что>     блокнот, сеть, письма, калькулятор, файлы, корзина, chosen"));
			Print(TEXT("  заметки           список заметок"));
			Print(TEXT("  время             который час"));
			Print(TEXT("  кто               кто здесь"));
			Print(TEXT("  эхо <текст>       повторить"));
			Print(TEXT("  атон              о системе"));
			Print(TEXT("  очистить          очистить экран"));
			if (Shell->CanLeave())
			{
				Print(TEXT("  встать            встать из-за компьютера"));
			}
		}
		else if (Is({ TEXT("очистить"), TEXT("clear"), TEXT("cls") }))
		{
			Output->ClearChildren();
		}
		else if (Is({ TEXT("время"), TEXT("time"), TEXT("date") }))
		{
			Print(Shell->Clock());
		}
		else if (Is({ TEXT("кто"), TEXT("whoami") }))
		{
			Print(TEXT("пользователь не найден"));
		}
		else if (Is({ TEXT("эхо"), TEXT("echo") }))
		{
			Print(Rest);
		}
		else if (Is({ TEXT("атон"), TEXT("aton"), TEXT("ver"), TEXT("uname") }))
		{
			Print(TEXT("АТОН OS v1.0"));
			Print(TEXT("память: 640 КБ (?\?)"));
			Print(TEXT("сеть: Людмиловск"));
		}
		else if (Is({ TEXT("заметки"), TEXT("ls"), TEXT("dir") }))
		{
			const TArray<FString> Notes = AtonApps::ListNotes(AtonApps::NotesDir());
			if (Notes.Num() == 0)
			{
				Print(TEXT("заметок нет"));
			}
			for (const FString& F : Notes)
			{
				Print(TEXT("  ") + AtonApps::NoteTitle(AtonApps::NotesDir(), F));
			}
		}
		else if (Is({ TEXT("открыть"), TEXT("open"), TEXT("start") }))
		{
			static const TMap<FString, FName> Names = {
				{ TEXT("блокнот"), AtonApps::Notes }, { TEXT("заметки"), AtonApps::Notes }, { TEXT("notes"), AtonApps::Notes },
				{ TEXT("сеть"), AtonApps::Browser }, { TEXT("браузер"), AtonApps::Browser }, { TEXT("browser"), AtonApps::Browser },
				{ TEXT("письма"), AtonApps::Mail }, { TEXT("почта"), AtonApps::Mail }, { TEXT("mail"), AtonApps::Mail },
				{ TEXT("калькулятор"), AtonApps::Calculator }, { TEXT("calc"), AtonApps::Calculator },
				{ TEXT("файлы"), AtonApps::Computer }, { TEXT("компьютер"), AtonApps::Computer },
				{ TEXT("корзина"), AtonApps::Trash }, { TEXT("trash"), AtonApps::Trash },
				{ TEXT("chosen"), AtonApps::Chosen }, { TEXT("игра"), AtonApps::Chosen }, { TEXT("игру"), AtonApps::Chosen },
			};
			if (const FName* App = Names.Find(Rest.ToLower()))
			{
				Shell->OpenApp(*App);
			}
			else
			{
				Print(Rest.IsEmpty() ? TEXT("что открыть?") : TEXT("нет такой программы: ") + Rest);
			}
		}
		else if (Is({ TEXT("встать"), TEXT("exit"), TEXT("выход") }))
		{
			if (Shell->CanLeave())
			{
				Shell->Leave();
			}
			else
			{
				Print(TEXT("встать не получается."));
			}
		}
		else
		{
			Print(TEXT("команда не найдена: ") + Cmd + TEXT(". набери «помощь»."));
		}
	}

	IAtonShell* Shell = nullptr;
	bool bAnswered = false;
	TSharedPtr<SScrollBox> Scroll;
	TSharedPtr<SVerticalBox> Output;
	TSharedPtr<SEditableText> Input;
};

// ---------------------------------------------------------------- Письма

class SAtonMail : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonMail) {}
	SLATE_END_ARGS()

	struct FLetter
	{
		FString From, Subject, Date, Body;
	};

	void Construct(const FArguments&)
	{
		Letters = {
			{ TEXT("АТОН"), TEXT("Добро пожаловать"), TEXT("сегодня, 03:12"), TEXT(
				"Здравствуйте!\n\n"
				"Это ваш компьютер с АТОН OS. На рабочем столе: Блокнот для заметок, Сеть для страниц Людмиловска, "
				"Терминал, Письма, Калькулятор и Корзина.\n\n"
				"Меню «АТОН OS» в левом верхнем углу.\n\n"
				"Хорошей работы.\nАТОН") },
			{ TEXT("Служба поддержки"), TEXT("Пароль не нужен"), TEXT("сегодня, 03:13"), TEXT(
				"Вход в систему выполнен автоматически.\n\n"
				"Пользователь не найден, поэтому вы вошли как Гость.\n\n"
				"Если это не вы, ничего не делайте.") },
		};
		Read.Init(false, Letters.Num());

		TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
		for (int32 i = 0; i < Letters.Num(); ++i)
		{
			List->AddSlot().AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(AtonStyle::Flat())
				.BorderBackgroundColor_Lambda([this, i] { return Selected == i ? AtonStyle::Select : FLinearColor::Transparent; })
				.Padding(0)
				[
					SNew(SButton)
					.ButtonStyle(&AtonStyle::Ghost())
					.ContentPadding(FMargin(10, 6))
					.OnClicked_Lambda([this, i] { Show(i); return FReply::Handled(); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(T(Letters[i].From))
							.Font_Lambda([this, i] { return AtonStyle::Font(14, !Read[i]); })
							.ColorAndOpacity(AtonStyle::Ink)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock).Text(T(Letters[i].Subject)).Font(AtonStyle::Font(13)).ColorAndOpacity(AtonStyle::InkSoft)
						]
					]
				]
			];
		}

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(200)[Panel(AtonStyle::C(0xE9, 0xE5, 0xD8), SNew(SScrollBox) + SScrollBox::Slot()[List])]
			]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(1)[Panel(AtonStyle::PaperLine, SNullWidget::NullWidget)]]
			+ SHorizontalBox::Slot().FillWidth(1)
			[
				Panel(AtonStyle::Paper, SNew(SScrollBox) + SScrollBox::Slot().Padding(18, 14)[SAssignNew(Body, SVerticalBox)])
			]
		];
		Show(0);
	}

private:
	void Show(int32 i)
	{
		Selected = i;
		Read[i] = true;
		const FLetter& L = Letters[i];
		Body->ClearChildren();
		Body->AddSlot().AutoHeight()[Label(L.Subject, 20, AtonStyle::Ink, true)];
		Body->AddSlot().AutoHeight().Padding(0, 4, 0, 0)[Label(TEXT("от: ") + L.From + TEXT("   ·   ") + L.Date, 13, AtonStyle::InkSoft)];
		Body->AddSlot().AutoHeight().Padding(0, 10)[Divider()];
		Body->AddSlot().AutoHeight()[Label(L.Body, 15)];
	}

	TArray<FLetter> Letters;
	TArray<bool> Read;
	int32 Selected = 0;
	TSharedPtr<SVerticalBox> Body;
};

// ---------------------------------------------------------------- Калькулятор

class SAtonCalculator : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonCalculator) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(3));
		const TCHAR* Keys[5][4] = {
			{ TEXT("C"), TEXT("±"), TEXT("%"), TEXT("÷") },
			{ TEXT("7"), TEXT("8"), TEXT("9"), TEXT("×") },
			{ TEXT("4"), TEXT("5"), TEXT("6"), TEXT("−") },
			{ TEXT("1"), TEXT("2"), TEXT("3"), TEXT("+") },
			{ TEXT("0"), TEXT(","), TEXT("←"), TEXT("=") },
		};
		for (int32 R = 0; R < 5; ++R)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				const FString K = Keys[R][Col];
				Grid->AddSlot(Col, R)
				[
					SNew(SBox).HeightOverride(44)
					[
						SNew(SButton)
						.ButtonStyle(&AtonStyle::Button())
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.IsFocusable(false)
						.OnClicked_Lambda([this, K] { Press(K); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(T(K)).Font(AtonStyle::Font(18)).ColorAndOpacity(AtonStyle::Ink)
						]
					]
				];
			}
		}

		ChildSlot
		[
			Panel(AtonStyle::C(0xE9, 0xE5, 0xD8),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(3, 3, 3, 8)
				[
					Panel(AtonStyle::C(0xC8, 0xD4, 0xB4),
						SNew(STextBlock)
						.Justification(ETextJustify::Right)
						.Font(AtonStyle::Mono(26))
						.ColorAndOpacity(AtonStyle::Ink)
						.Text_Lambda([this] { return T(Display()); }),
						FMargin(10, 8))
				]
				+ SVerticalBox::Slot().FillHeight(1)[Grid],
				FMargin(8))
		];
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return ConstCastSharedRef<SWidget>(AsShared()); }
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override
	{
		const TCHAR Ch = InCharacterEvent.GetCharacter();
		static const TMap<TCHAR, FString> Map = {
			{ '+', TEXT("+") }, { '-', TEXT("−") }, { '*', TEXT("×") }, { '/', TEXT("÷") }, { '=', TEXT("=") },
			{ '.', TEXT(",") }, { ',', TEXT(",") }, { '%', TEXT("%") }, { '\r', TEXT("=") }, { '\n', TEXT("=") },
			{ 'c', TEXT("C") }, { 'C', TEXT("C") }, { 8, TEXT("←") }, { 27, TEXT("C") },
		};
		if (FChar::IsDigit(Ch))
		{
			Press(FString(1, &Ch));
			return FReply::Handled();
		}
		if (const FString* K = Map.Find(Ch))
		{
			Press(*K);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

private:
	FString Display() const
	{
		FString S = Entry.IsEmpty() ? Format(Acc) : Entry;
		return S.Replace(TEXT("."), TEXT(","));
	}

	static FString Format(double V)
	{
		if (!FMath::IsFinite(V))
		{
			return TEXT("ошибка");
		}
		FString S = FString::Printf(TEXT("%.10g"), V);
		return S == TEXT("-0") ? TEXT("0") : S;
	}

	double Current() const { return Entry.IsEmpty() ? Acc : FCString::Atod(*Entry); }

	void Apply()
	{
		const double B = Current();
		if (Op == TEXT("+")) Acc += B;
		else if (Op == TEXT("−")) Acc -= B;
		else if (Op == TEXT("×")) Acc *= B;
		else if (Op == TEXT("÷")) Acc = B == 0.0 ? std::numeric_limits<double>::quiet_NaN() : Acc / B;
		else Acc = B;
		Entry.Empty();
	}

	void Press(const FString& K)
	{
		if (K.Len() == 1 && FChar::IsDigit(K[0]))
		{
			if (bJustEvaluated) { Op.Empty(); bJustEvaluated = false; }
			if (Entry == TEXT("0")) Entry.Empty();
			if (Entry.Len() < 14) Entry += K;
		}
		else if (K == TEXT(","))
		{
			if (bJustEvaluated) { Op.Empty(); bJustEvaluated = false; }
			if (!Entry.Contains(TEXT("."))) Entry += Entry.IsEmpty() ? TEXT("0.") : TEXT(".");
		}
		else if (K == TEXT("←"))
		{
			Entry.LeftChopInline(1);
		}
		else if (K == TEXT("C"))
		{
			Acc = 0.0; Entry.Empty(); Op.Empty(); bJustEvaluated = false;
		}
		else if (K == TEXT("±"))
		{
			if (Entry.IsEmpty()) Acc = -Acc;
			else Entry = Entry.StartsWith(TEXT("-")) ? Entry.RightChop(1) : TEXT("-") + Entry;
		}
		else if (K == TEXT("%"))
		{
			Entry = Format(Current() / 100.0);
		}
		else if (K == TEXT("="))
		{
			Apply();
			Op.Empty();
			bJustEvaluated = true;
		}
		else // + − × ÷
		{
			if (!Entry.IsEmpty() || Op.IsEmpty())
			{
				Apply();
			}
			Op = K;
			bJustEvaluated = false;
		}
	}

	double Acc = 0.0;
	FString Entry;
	FString Op;
	bool bJustEvaluated = false;
};

// ---------------------------------------------------------------- Мой компьютер

class SAtonComputer : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonComputer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, IAtonShell* InShell)
	{
		Shell = InShell;
		ChildSlot
		[
			Panel(AtonStyle::Paper,
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					Panel(AtonStyle::C(0xE9, 0xE5, 0xD8),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)[Button(TEXT("‹ Вверх"), [this] { Open(TEXT("")); }, 13)]
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(AtonStyle::Mono(14)).ColorAndOpacity(AtonStyle::Ink)
							.Text_Lambda([this] { return T(Folder.IsEmpty() ? TEXT("C:\\") : TEXT("C:\\") + Folder); })
						],
						FMargin(8))
				]
				+ SVerticalBox::Slot().AutoHeight()[Divider()]
				+ SVerticalBox::Slot().FillHeight(1).Padding(8)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(List, SVerticalBox)]
				]
				+ SVerticalBox::Slot().AutoHeight()[Divider()]
				+ SVerticalBox::Slot().AutoHeight().Padding(12, 6)
				[
					SNew(STextBlock).Font(AtonStyle::Font(13)).ColorAndOpacity(AtonStyle::InkSoft).Text_Lambda([this] { return T(Status); })
				])
		];
		Open(TEXT(""));
	}

private:
	void Add(const FString& Name, TFunction<void()> OnClick)
	{
		List->AddSlot().AutoHeight()[Row(Name, OnClick)];
	}

	void Open(const FString& InFolder)
	{
		Folder = InFolder;
		List->ClearChildren();
		Status = TEXT("Диск C:   свободно 12 МБ из 40 МБ");
		if (Folder.IsEmpty())
		{
			Add(TEXT("[ ]  Заметки"), [this] { Open(TEXT("Заметки")); });
			Add(TEXT("[ ]  Система"), [this] { Open(TEXT("Система")); });
			Add(TEXT("  @  Письма"), [this] { Shell->OpenApp(AtonApps::Mail); });
			Add(TEXT("  ×  Корзина"), [this] { Shell->OpenApp(AtonApps::Trash); });
			Add(TEXT("www  Сеть"), [this] { Shell->OpenApp(AtonApps::Browser); });
		}
		else if (Folder == TEXT("Заметки"))
		{
			const FString Dir = AtonApps::NotesDir();
			for (const FString& F : AtonApps::ListNotes(Dir))
			{
				Add(TEXT("  ≡  ") + AtonApps::NoteTitle(Dir, F), [this, F] { Shell->OpenApp(AtonApps::Notes, F); });
			}
		}
		else if (Folder == TEXT("Система"))
		{
			for (const TCHAR* F : { TEXT("aton.sys"), TEXT("boot.cfg"), TEXT("user.dat") })
			{
				const FString Name = F;
				Add(TEXT("     ") + Name, [this, Name]
				{
					Status = Name == TEXT("user.dat") ? TEXT("user.dat: файл пуст. Пользователь не найден.") : Name + TEXT(": системный файл, доступ запрещён.");
				});
			}
		}
	}

	IAtonShell* Shell = nullptr;
	FString Folder;
	FString Status;
	TSharedPtr<SVerticalBox> List;
};

// ---------------------------------------------------------------- О системе

class SAtonAbout : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonAbout) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		ChildSlot
		[
			Panel(AtonStyle::Paper,
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("АТОН OS"), 34, AtonStyle::Ink, true)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)[Label(TEXT("версия 1.0"), 15, AtonStyle::InkSoft)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12)[Divider()]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("память: 640 КБ (?\?)"), 14)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(TEXT("пользователь: не найден"), 14)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)[Label(TEXT("(c) Людмиловск"), 13, AtonStyle::InkSoft)],
				FMargin(20, 18))
		];
	}
};

// ---------------------------------------------------------------- Chosen (the sword-duel game, a web app)

FString AtonApps::ChosenLocalURL = TEXT("http://localhost:8787/");
FString AtonApps::ChosenURL = TEXT("https://chosen.stickies.fun/");

class SAtonChosen : public SAtonProgram
{
public:
	SLATE_BEGIN_ARGS(SAtonChosen) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		const bool bLocal = !AtonApps::ChosenLocalURL.IsEmpty();
		bFellBack = !bLocal;
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				Panel(AtonStyle::C(0x09, 0x08, 0x06), SNullWidget::NullWidget)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(Browser, SWebBrowser)
				.InitialURL(bLocal ? AtonApps::ChosenLocalURL : AtonApps::ChosenURL)
				.ShowControls(false)
				.ShowAddressBar(false)
				.ShowErrorMessage(false)
				.ShowInitialThrobber(false)
				.SupportsTransparency(false)
				.BackgroundColor(FColor(0x09, 0x08, 0x06))
				.BrowserFrameRate(60)
				.OnLoadCompleted_Lambda([this] { bLoaded = true; })
				.OnLoadError_Lambda([this]
				{
					// No local build running: play on the public site.
					if (!bFellBack)
					{
						bFellBack = true;
						Browser->LoadURL(AtonApps::ChosenURL);
					}
					else
					{
						bFailed = true;
					}
				})
			]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(AtonStyle::Mono(16))
				.ColorAndOpacity(AtonStyle::C(0xD8, 0xC9, 0xA3))
				.Visibility_Lambda([this] { return bLoaded ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				.Text_Lambda([this] { return T(bFailed ? TEXT("нет связи с Chosen") : TEXT("загрузка Chosen…")); })
			]
		];
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return Browser; }

private:
	TSharedPtr<SWebBrowser> Browser;
	bool bFellBack = false;
	bool bLoaded = false;
	bool bFailed = false;
};

// ---------------------------------------------------------------- registry

const TArray<FAtonAppInfo>& AtonApps::List()
{
	static const TArray<FAtonAppInfo> Apps = {
		{ Computer, T(TEXT("Мой компьютер")), TEXT("C:"), AtonStyle::C(0xEF, 0xDB, 0x8A), FVector2D(480, 380) },
		{ Notes, T(TEXT("Блокнот")), TEXT("Аа"), AtonStyle::C(0xF7, 0xF6, 0xF0), FVector2D(660, 470) },
		{ Browser, T(TEXT("Сеть")), TEXT("www"), AtonStyle::C(0xBF, 0xD9, 0xEE), FVector2D(720, 520) },
		{ Terminal, T(TEXT("Терминал")), TEXT(">_"), AtonStyle::C(0xB9, 0xE8, 0xC4), FVector2D(680, 440) },
		{ Mail, T(TEXT("Письма")), TEXT("@"), AtonStyle::C(0xF3, 0xD3, 0xB5), FVector2D(640, 420) },
		{ Calculator, T(TEXT("Калькулятор")), TEXT("±"), AtonStyle::C(0xD9, 0xD2, 0xF0), FVector2D(280, 380) },
		{ Chosen, T(TEXT("Chosen")), TEXT("CH"), AtonStyle::C(0xE3, 0xB2, 0x61), FVector2D(896, 560) },
		{ Trash, T(TEXT("Корзина")), TEXT("×"), AtonStyle::C(0xC3, 0xCA, 0xCC), FVector2D(440, 320) },
		{ About, T(TEXT("О системе")), TEXT("i"), AtonStyle::Bar, FVector2D(360, 290), false },
	};
	return Apps;
}

const FAtonAppInfo* AtonApps::Find(FName Id)
{
	return List().FindByPredicate([Id](const FAtonAppInfo& A) { return A.Id == Id; });
}

TSharedRef<SAtonProgram> AtonApps::Make(FName Id, IAtonShell& Shell)
{
	if (Id == Notes) return SNew(SAtonNotes);
	if (Id == Browser) return SNew(SAtonBrowser);
	if (Id == Terminal) return SNew(SAtonTerminal, &Shell);
	if (Id == Mail) return SNew(SAtonMail);
	if (Id == Calculator) return SNew(SAtonCalculator);
	if (Id == Computer) return SNew(SAtonComputer, &Shell);
	if (Id == Trash) return SNew(SAtonTrash);
	if (Id == Chosen) return SNew(SAtonChosen);
	return SNew(SAtonAbout);
}
