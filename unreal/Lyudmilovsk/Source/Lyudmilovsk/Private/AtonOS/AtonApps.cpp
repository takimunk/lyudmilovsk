#include "AtonApps.h"

#include "AtonStyle.h"
#include "SAtonDesktop.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "LyudText.h"
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

	/** ui.* text in the current language. */
	FString L(const TCHAR* Key, const TArray<FStringFormatArg>& Args = {}) { return LyudText::Get(Key, Args).ToString(); }

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
		return L(TEXT("ui.aton.notes.untitled"));
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
			WriteNote(Dir, TEXT("note_001.txt"), L(TEXT("ui.aton.notes.first")));
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
							Button(L(TEXT("ui.aton.notes.new")), [this] { CreateNote(); })
						]
						+ SVerticalBox::Slot().FillHeight(1)
						[
							SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(List, SVerticalBox)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(8)
						[
							Button(L(TEXT("ui.aton.notes.delete")), [this] { DeleteNote(); })
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
					Button(L(TEXT("ui.aton.trash.empty_all")), [this]
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
			List->AddSlot().AutoHeight().Padding(10)[Label(L(TEXT("ui.aton.trash.is_empty")), 15, AtonStyle::InkSoft)];
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
					Button(L(TEXT("ui.aton.trash.restore")), [this, File]
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
	 * Pages of the local network, by page id. One line per block:
	 *   "# Title", "## Subtitle", "[link text](page id)", "---" rule, "?search" a search box, anything else a paragraph.
	 * "@name" is the string ui.aton.web.<name> in the current language. The address shown for a page is ui.aton.web.addr.<id>.
	 */
	const TMap<FString, FString>& PageTemplates()
	{
		static const TMap<FString, FString> P = {
			{ TEXT("home"), TEXT(
				"# @home.title\n"
				"@home.text\n"
				"?search\n"
				"---\n"
				"## @home.where\n"
				"[@link.news](news)\n"
				"[@link.weather](weather)\n"
				"[@link.help](help)\n") },
			{ TEXT("news"), TEXT(
				"# @news.title\n"
				"---\n"
				"## @news.1.title\n"
				"@news.1.text\n"
				"## @news.2.title\n"
				"@news.2.text\n"
				"## @news.3.title\n"
				"@news.3.text\n"
				"---\n"
				"[@link.home](home)\n") },
			{ TEXT("weather"), TEXT(
				"# @weather.title\n"
				"## @weather.now\n"
				"@weather.night\n"
				"@weather.tomorrow\n"
				"@weather.after\n"
				"---\n"
				"[@link.home](home)\n") },
			{ TEXT("help"), TEXT(
				"# @help.title\n"
				"@help.intro\n"
				"## @help.programs\n"
				"@help.notes\n"
				"@help.web\n"
				"@help.terminal\n"
				"@help.mail\n"
				"@help.calculator\n"
				"@help.trash\n"
				"@help.chosen\n"
				"---\n"
				"[@link.home](home)\n") },
		};
		return P;
	}

	const FString HomePage = TEXT("home");
	const FString SearchPage = TEXT("search");

	FString WebText(const FString& Name, const TArray<FStringFormatArg>& Args = {})
	{
		return LyudText::Get(TEXT("ui.aton.web.") + Name, Args).ToString();
	}

	/** A page template with its @names replaced by the words of the current language. */
	FString Expand(const FString& Template)
	{
		TArray<FString> Lines;
		Template.ParseIntoArrayLines(Lines, false);
		FString Out;
		for (const FString& Line : Lines)
		{
			FString Prefix, Rest;
			if (Line.Split(TEXT("@"), &Prefix, &Rest))
			{
				FString Name = Rest, Suffix;
				const int32 End = Rest.Find(TEXT("]("));
				if (End != INDEX_NONE)
				{
					Name = Rest.Left(End);
					Suffix = Rest.Mid(End);
				}
				Out += Prefix + WebText(Name) + Suffix + TEXT("\n");
			}
			else
			{
				Out += Line + TEXT("\n");
			}
		}
		return Out;
	}

	/** The address a page goes by in the current language ("дом.атон", "home.aton"). */
	FString Address(const FString& PageId)
	{
		return WebText(TEXT("addr.") + PageId).ToLower();
	}

	FString Normalize(FString Addr)
	{
		Addr = Addr.TrimStartAndEnd().ToLower();
		Addr.RemoveFromStart(TEXT("http://"));
		Addr.RemoveFromStart(TEXT("https://"));
		Addr.RemoveFromStart(TEXT("www."));
		Addr.RemoveFromEnd(TEXT("/"));
		return Addr;
	}

	/** A typed address (in any form) to "pageid" or "pageid?query"; unknown addresses stay as they are. */
	FString Resolve(const FString& Typed)
	{
		FString Path = Normalize(Typed), Query;
		Path.Split(TEXT("?"), &Path, &Query);
		if (Path.IsEmpty())
		{
			return HomePage;
		}
		FString Id;
		if (PageTemplates().Contains(Path) || Path == SearchPage)
		{
			Id = Path;
		}
		else if (Path == Address(SearchPage))
		{
			Id = SearchPage;
		}
		else
		{
			for (const TPair<FString, FString>& P : PageTemplates())
			{
				if (Path == Address(P.Key))
				{
					Id = P.Key;
				}
			}
		}
		if (Id.IsEmpty())
		{
			return Normalize(Typed);
		}
		return Query.IsEmpty() ? Id : Id + TEXT("?") + Query;
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
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0)[Button(L(TEXT("ui.aton.web.back")), [this] { Back(); }, 13)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[Button(L(TEXT("ui.aton.web.home")), [this] { Navigate(HomePage); }, 13)]
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
	void Navigate(const FString& Typed, bool bRemember = true)
	{
		const FString Where = Resolve(Typed);
		if (bRemember && (History.Num() == 0 || History.Last() != Where))
		{
			History.Add(Where);
		}
		FString Path = Where, Query;
		Where.Split(TEXT("?"), &Path, &Query);
		const bool bKnown = PageTemplates().Contains(Path) || Path == SearchPage;
		const FString Shown = bKnown ? Address(Path) : Path;
		AddressBar->SetText(T(Query.IsEmpty() ? Shown : Shown + TEXT("?") + Query));
		Render(Path, Query, bKnown);
	}

	void Back()
	{
		if (History.Num() > 1)
		{
			History.Pop();
			Navigate(History.Last(), false);
		}
	}

	void Render(const FString& Path, const FString& Query, bool bKnown)
	{
		Page->ClearChildren();
		FString Markup;
		if (Path == SearchPage)
		{
			Markup = SearchResults(DecodeQuery(Query));
		}
		else if (bKnown)
		{
			Markup = Expand(PageTemplates()[Path]);
		}
		else
		{
			Markup = TEXT("# ") + WebText(TEXT("notfound.title")) + TEXT("\n")
				+ WebText(TEXT("notfound.text"), { FStringFormatArg(Path) }) + TEXT("\n?search\n[")
				+ WebText(TEXT("link.home")) + TEXT("](home)\n");
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

	static FString DecodeQuery(const FString& Query)
	{
		FString Q = Query;
		Q.RemoveFromStart(TEXT("q="));
		return Q.Replace(TEXT("+"), TEXT(" "));
	}

	/** Searches the text of every page in the current language; every word of the query must be on a page. */
	static FString SearchResults(const FString& Query)
	{
		TArray<FString> Words;
		Query.ToLower().ParseIntoArrayWS(Words);
		FString Out = TEXT("# ") + WebText(TEXT("search.title")) + TEXT("\n?search\n---\n");
		if (Words.Num() == 0)
		{
			return Out + WebText(TEXT("search.empty")) + TEXT("\n");
		}
		int32 Hits = 0;
		for (const TPair<FString, FString>& P : PageTemplates())
		{
			const FString Text = Expand(P.Value);
			const FString Body = Text.ToLower();
			if (Algo::AllOf(Words, [&Body](const FString& W) { return Body.Contains(W); }))
			{
				FString Title;
				Text.Split(TEXT("\n"), &Title, nullptr);
				Title.RemoveFromStart(TEXT("# "));
				Out += FString::Printf(TEXT("[%s](%s)\n"), *Title, *P.Key);
				Hits++;
			}
		}
		if (Hits == 0)
		{
			Out += WebText(TEXT("search.nothing"), { FStringFormatArg(Query) }) + TEXT("\n");
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
							.HintText(LyudText::Get(TEXT("ui.aton.web.search.hint")))
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
					Button(L(TEXT("ui.aton.web.search.go")), [this, Box] { Navigate(SearchPage + TEXT("?q=") + Box->GetText().ToString().TrimStartAndEnd()); })
				];
			return SNew(SBox).Padding(FMargin(0, 8)).MaxDesiredWidth(520)[Bar];
		}
		FString Text, Target;
		if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT(")")) && Line.Split(TEXT("]("), &Text, &Target))
		{
			Text.RemoveFromStart(TEXT("["));
			Target.RemoveFromEnd(TEXT(")"));
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&AtonStyle::Ghost())
					.ContentPadding(FMargin(0, 2))
					.OnClicked_Lambda([this, Target] { Navigate(Target); return FReply::Handled(); })
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

		// An empty name is a blank line.
		for (const TCHAR* Key : { TEXT("boot.1"), TEXT(""), TEXT("boot.2"), TEXT("boot.3"), TEXT("boot.4"), TEXT("boot.5"), TEXT(""), TEXT("boot.6") })
		{
			Print(*Key ? TermText(Key) : FString());
		}
	}

	virtual TSharedPtr<SWidget> FocusTarget() const override { return Input; }

private:
	static FString TermText(const FString& Name, const TArray<FStringFormatArg>& Args = {})
	{
		return LyudText::Get(TEXT("ui.aton.term.") + Name, Args).ToString();
	}

	/** Words that mean Name: ui.aton.term.<Name> is a comma-separated list in the current language, plus the English ones given here. */
	static bool Means(const FString& Word, const FString& Name, const TArray<const TCHAR*>& English = {})
	{
		TArray<FString> Words;
		TermText(Name).ToLower().ParseIntoArray(Words, TEXT(","));
		for (const TCHAR* E : English)
		{
			Words.Add(E);
		}
		return Algo::AnyOf(Words, [&Word](const FString& W) { return W.TrimStartAndEnd() == Word; });
	}

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
		auto Is = [&Cmd](const TCHAR* Name, std::initializer_list<const TCHAR*> English)
		{
			return Means(Cmd, FString(TEXT("cmd.")) + Name, English);
		};

		if (!bAnswered && (Is(TEXT("yes"), { TEXT("yes"), TEXT("y") }) || Is(TEXT("no"), { TEXT("no"), TEXT("n") })))
		{
			bAnswered = true;
			Print(TermText(Is(TEXT("no"), { TEXT("no"), TEXT("n") }) ? TEXT("answer.no") : TEXT("answer.yes")));
			Print(TermText(TEXT("answer.hint")));
		}
		else if (Is(TEXT("help"), { TEXT("help"), TEXT("?") }))
		{
			for (const TCHAR* Key : { TEXT("help.title"), TEXT("help.help"), TEXT("help.open"), TEXT("help.notes"), TEXT("help.time"),
			                          TEXT("help.who"), TEXT("help.echo"), TEXT("help.aton"), TEXT("help.clear") })
			{
				Print(TermText(Key));
			}
			if (Shell->CanLeave())
			{
				Print(TermText(TEXT("help.leave")));
			}
		}
		else if (Is(TEXT("clear"), { TEXT("clear"), TEXT("cls") }))
		{
			Output->ClearChildren();
		}
		else if (Is(TEXT("time"), { TEXT("time"), TEXT("date") }))
		{
			Print(Shell->Clock());
		}
		else if (Is(TEXT("who"), { TEXT("whoami") }))
		{
			Print(TermText(TEXT("who")));
		}
		else if (Is(TEXT("echo"), { TEXT("echo") }))
		{
			Print(Rest);
		}
		else if (Is(TEXT("aton"), { TEXT("aton"), TEXT("ver"), TEXT("uname") }))
		{
			Print(TermText(TEXT("aton.1")));
			Print(TermText(TEXT("aton.2")));
			Print(TermText(TEXT("aton.3")));
		}
		else if (Is(TEXT("notes"), { TEXT("ls"), TEXT("dir"), TEXT("notes") }))
		{
			const TArray<FString> Notes = AtonApps::ListNotes(AtonApps::NotesDir());
			if (Notes.Num() == 0)
			{
				Print(TermText(TEXT("notes.none")));
			}
			for (const FString& F : Notes)
			{
				Print(TEXT("  ") + AtonApps::NoteTitle(AtonApps::NotesDir(), F));
			}
		}
		else if (Is(TEXT("open"), { TEXT("open"), TEXT("start") }))
		{
			struct FProgramName { FName App; const TCHAR* Name; TArray<const TCHAR*> English; };
			const FProgramName Names[] = {
				{ AtonApps::Notes, TEXT("app.notes"), { TEXT("notes"), TEXT("notepad") } },
				{ AtonApps::Browser, TEXT("app.browser"), { TEXT("browser"), TEXT("web"), TEXT("net") } },
				{ AtonApps::Mail, TEXT("app.mail"), { TEXT("mail") } },
				{ AtonApps::Calculator, TEXT("app.calculator"), { TEXT("calc"), TEXT("calculator") } },
				{ AtonApps::Computer, TEXT("app.computer"), { TEXT("files"), TEXT("computer") } },
				{ AtonApps::Trash, TEXT("app.trash"), { TEXT("trash") } },
				{ AtonApps::Chosen, TEXT("app.chosen"), { TEXT("chosen"), TEXT("game") } },
			};
			const FString Want = Rest.ToLower();
			const FProgramName* Found = Algo::FindByPredicate(Names, [&Want](const FProgramName& P) { return Means(Want, P.Name, P.English); });
			if (Found)
			{
				Shell->OpenApp(Found->App);
			}
			else
			{
				Print(Rest.IsEmpty() ? TermText(TEXT("open.what")) : TermText(TEXT("open.unknown"), { FStringFormatArg(Rest) }));
			}
		}
		else if (Is(TEXT("leave"), { TEXT("exit"), TEXT("quit"), TEXT("leave") }))
		{
			if (Shell->CanLeave())
			{
				Shell->Leave();
			}
			else
			{
				Print(TermText(TEXT("leave.cannot")));
			}
		}
		else
		{
			Print(TermText(TEXT("unknown"), { FStringFormatArg(Cmd) }));
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
		for (const TCHAR* Id : { TEXT("1"), TEXT("2") })
		{
			auto Part = [Id](const TCHAR* What) { return ::L(*FString::Printf(TEXT("ui.aton.mail.%s.%s"), Id, What)); };
			Letters.Add({ Part(TEXT("from")), Part(TEXT("subject")), Part(TEXT("date")), Part(TEXT("body")) });
		}
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
		Body->AddSlot().AutoHeight().Padding(0, 4, 0, 0)[Label(::L(TEXT("ui.aton.mail.from_line"), { FStringFormatArg(L.From), FStringFormatArg(L.Date) }), 13, AtonStyle::InkSoft)];
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
			return L(TEXT("ui.aton.calc.error"));
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
						+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)[Button(L(TEXT("ui.aton.files.up")), [this] { Open(TEXT("")); }, 13)]
						+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Font(AtonStyle::Mono(14)).ColorAndOpacity(AtonStyle::Ink)
							.Text_Lambda([this] { return T(Folder.IsEmpty() ? TEXT("C:\\") : TEXT("C:\\") + L(*(TEXT("ui.aton.files.") + Folder))); })
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
		Status = L(TEXT("ui.aton.files.disk"), { FStringFormatArg(12), FStringFormatArg(40) });
		if (Folder.IsEmpty())
		{
			Add(TEXT("[ ]  ") + L(TEXT("ui.aton.files.notes")), [this] { Open(TEXT("notes")); });
			Add(TEXT("[ ]  ") + L(TEXT("ui.aton.files.system")), [this] { Open(TEXT("system")); });
			Add(TEXT("  @  ") + L(TEXT("ui.aton.app.mail")), [this] { Shell->OpenApp(AtonApps::Mail); });
			Add(TEXT("  ×  ") + L(TEXT("ui.aton.app.trash")), [this] { Shell->OpenApp(AtonApps::Trash); });
			Add(TEXT("www  ") + L(TEXT("ui.aton.app.browser")), [this] { Shell->OpenApp(AtonApps::Browser); });
		}
		else if (Folder == TEXT("notes"))
		{
			const FString Dir = AtonApps::NotesDir();
			for (const FString& F : AtonApps::ListNotes(Dir))
			{
				Add(TEXT("  ≡  ") + AtonApps::NoteTitle(Dir, F), [this, F] { Shell->OpenApp(AtonApps::Notes, F); });
			}
		}
		else if (Folder == TEXT("system"))
		{
			for (const TCHAR* F : { TEXT("aton.sys"), TEXT("boot.cfg"), TEXT("user.dat") })
			{
				const FString Name = F;
				Add(TEXT("     ") + Name, [this, Name]
				{
					Status = L(Name == TEXT("user.dat") ? TEXT("ui.aton.files.userdat") : TEXT("ui.aton.files.denied"), { FStringFormatArg(Name) });
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
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(L(TEXT("ui.aton.os")), 34, AtonStyle::Ink, true)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)[Label(L(TEXT("ui.aton.about.version")), 15, AtonStyle::InkSoft)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 12)[Divider()]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(L(TEXT("ui.aton.about.memory")), 14)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Label(L(TEXT("ui.aton.about.user")), 14)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)[Label(L(TEXT("ui.aton.about.copyright")), 13, AtonStyle::InkSoft)],
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
				.Text_Lambda([this] { return LyudText::Get(bFailed ? TEXT("ui.aton.chosen.offline") : TEXT("ui.aton.chosen.loading")); })
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
		{ Computer, TEXT("ui.aton.app.computer"), TEXT("C:"), AtonStyle::C(0xEF, 0xDB, 0x8A), FVector2D(480, 380) },
		{ Notes, TEXT("ui.aton.app.notes"), TEXT("Aa"), AtonStyle::C(0xF7, 0xF6, 0xF0), FVector2D(660, 470) },
		{ Browser, TEXT("ui.aton.app.browser"), TEXT("www"), AtonStyle::C(0xBF, 0xD9, 0xEE), FVector2D(720, 520) },
		{ Terminal, TEXT("ui.aton.app.terminal"), TEXT(">_"), AtonStyle::C(0xB9, 0xE8, 0xC4), FVector2D(680, 440) },
		{ Mail, TEXT("ui.aton.app.mail"), TEXT("@"), AtonStyle::C(0xF3, 0xD3, 0xB5), FVector2D(640, 420) },
		{ Calculator, TEXT("ui.aton.app.calculator"), TEXT("±"), AtonStyle::C(0xD9, 0xD2, 0xF0), FVector2D(280, 380) },
		{ Chosen, TEXT("ui.aton.app.chosen"), TEXT("CH"), AtonStyle::C(0xE3, 0xB2, 0x61), FVector2D(896, 560) },
		{ Trash, TEXT("ui.aton.app.trash"), TEXT("×"), AtonStyle::C(0xC3, 0xCA, 0xCC), FVector2D(440, 320) },
		{ About, TEXT("ui.aton.app.about"), TEXT("i"), AtonStyle::Bar, FVector2D(360, 290), false },
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
