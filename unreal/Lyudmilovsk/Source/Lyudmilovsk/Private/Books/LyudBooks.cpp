// The town library's classics: the catalogue, the books on loan, the reader and the lending desk's screen (docs/books.md).

#include "Books/LyudBooks.h"

#include "Books/BookLayout.h"
#include "Books/LyudReading.h"
#include "Books/ReadingModel.h"
#include "Books/SLyudBook.h"
#include "City/LyudBenchSit.h"
#include "City/LyudCityThings.h"
#include "City/LyudPublicSpaces.h"
#include "Dev/LyudDev.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "LyudGameMenu.h"
#include "LyudInventory.h"
#include "LyudSave.h"
#include "LyudText.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Plants/GameCalendar.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UI/LyudGameScreen.h"
#include "UI/LyudQuests.h"

DEFINE_LOG_CATEGORY_STATIC(LogLyudBooks, Log, All);

const FName ULyudBooks::ItemId(TEXT("library_book"));

namespace LyudBooksImpl
{
	const FString& Pick(const TMap<FString, FString>& M, const FString& Lang)
	{
		static const FString Empty;
		if (const FString* S = M.Find(Lang); S && !S->IsEmpty())
		{
			return *S;
		}
		if (const FString* S = M.Find(TEXT("en")); S && !S->IsEmpty())
		{
			return *S;
		}
		for (const TPair<FString, FString>& P : M)
		{
			if (!P.Value.IsEmpty())
			{
				return P.Value;
			}
		}
		return Empty;
	}

	/** Reading speed, characters a game minute (a teenager reading for pleasure). */
	double CharsPerMinute(const FString& Lang)
	{
		return Lang == TEXT("ja") ? 450.0 : 1000.0;
	}
}

const FString& FLyudBookInfo::TitleNow() const { return LyudBooksImpl::Pick(Title, LyudText::Language()); }
const FString& FLyudBookInfo::AuthorNow() const { return LyudBooksImpl::Pick(Author, LyudText::Language()); }
const FString& FLyudBookInfo::TitleOwn() const { return LyudBooksImpl::Pick(Title, Lang); }

// ---------------------------------------------------------------------------------------------------------------- ULyudBooks

ULyudBooks* ULyudBooks::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<ULyudBooks>() : nullptr;
}

void ULyudBooks::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<ULyudInventory>();
	if (ULyudSaveSubsystem* Saves = Collection.InitializeDependency<ULyudSaveSubsystem>())
	{
		Saves->RegisterSaveObject(TEXT("Books"), this);
		LoadHandle = Saves->OnAfterLoad.AddWeakLambda(this, [this](ULyudSaveGame* Save)
		{
			if (Save && !Save->Records.Contains(TEXT("Books")))
			{
				// a save from before the library lent books: nothing on loan
				BorrowedKeys.Reset();
				Marks.Reset();
				FinishedKeys.Reset();
			}
			if (Save && Save->Records.Contains(TEXT("Books")))
			{
				// a save from before reading anywhere: its loans count from today
				for (const FString& K : BorrowedKeys)
				{
					if (!BorrowDays.Contains(K))
					{
						BorrowDays.Add(K, Today());
					}
				}
			}
			SyncPocket();
		});
		NewGameHandle = Saves->OnNewGame.AddWeakLambda(this, [this] { ResetAll(); });
	}
}

void ULyudBooks::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (ULyudSaveSubsystem* Saves = GI->GetSubsystem<ULyudSaveSubsystem>())
		{
			Saves->OnAfterLoad.Remove(LoadHandle);
			Saves->OnNewGame.Remove(NewGameHandle);
			Saves->UnregisterSaveObject(TEXT("Books"));
		}
	}
	Super::Deinitialize();
}

FString ULyudBooks::PathOf(const FLyudBookInfo& Book)
{
	return FPaths::ProjectContentDir() / TEXT("Books") / Book.File;
}

void ULyudBooks::Load()
{
	bLoaded = true;
	Books.Reset();
	ByKey.Reset();
	const FString Path = FPaths::ProjectContentDir() / TEXT("Books/books.json");
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		UE_LOG(LogLyudBooks, Warning, TEXT("Books: no %s: the library's classics shelf is empty (run python3 tools/books/fetch_books.py fetch)"), *Path);
		return;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogLyudBooks, Error, TEXT("Books: %s is not valid JSON"), *Path);
		return;
	}
	const TSharedPtr<FJsonObject>* BooksObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("books"), BooksObj))
	{
		return;
	}
	int32 Missing = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& P : (*BooksObj)->Values)
	{
		const TSharedPtr<FJsonObject> O = P.Value.IsValid() ? P.Value->AsObject() : nullptr;
		if (!O.IsValid())
		{
			continue;
		}
		FLyudBookInfo B;
		B.Key = P.Key;
		B.Work = O->GetStringField(TEXT("id"));
		B.Lang = O->GetStringField(TEXT("lang"));
		B.File = O->GetStringField(TEXT("file"));
		B.Year = (int32)O->GetNumberField(TEXT("year"));
		B.License = O->GetStringField(TEXT("license"));
		B.Source = O->GetStringField(TEXT("source"));
		B.Chars = (int32)O->GetNumberField(TEXT("chars"));
		const TSharedPtr<FJsonObject>* T = nullptr;
		if (O->TryGetObjectField(TEXT("title"), T))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*T)->Values)
			{
				B.Title.Add(L.Key, L.Value->AsString());
			}
		}
		if (O->TryGetObjectField(TEXT("author"), T))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& L : (*T)->Values)
			{
				B.Author.Add(L.Key, L.Value->AsString());
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Creators = nullptr;
		if (O->TryGetArrayField(TEXT("creators"), Creators))
		{
			for (const TSharedPtr<FJsonValue>& C : *Creators)
			{
				const TSharedPtr<FJsonObject> CO = C.IsValid() ? C->AsObject() : nullptr;
				if (!CO.IsValid())
				{
					continue;
				}
				const FString Name = CO->GetStringField(TEXT("name"));
				const FString Role = CO->GetStringField(TEXT("role"));
				double Died = 0.0;
				const bool bDied = CO->TryGetNumberField(TEXT("died"), Died) && Died > 0.0 && Died < 3000.0;
				const FString K = FString::Printf(TEXT("ui.books.credit.%s%s"), Role == TEXT("translator") ? TEXT("translator") : TEXT("author"),
					bDied ? TEXT("") : TEXT("_nodate"));
				B.Credits.Add(bDied ? LyudText::Get(K, { FStringFormatArg(Name), FStringFormatArg((int32)Died) }).ToString()
									: LyudText::Get(K, { FStringFormatArg(Name) }).ToString());
			}
		}
		if (B.Work.IsEmpty() || B.File.IsEmpty())
		{
			continue;
		}
		if (!FPaths::FileExists(PathOf(B)))
		{
			++Missing;
			continue;
		}
		Books.Add(MoveTemp(B));
	}
	SortForLanguage();
	UE_LOG(LogLyudBooks, Display, TEXT("Books: %d texts in the catalogue (%d listed but not downloaded)"), Books.Num(), Missing);
}

void ULyudBooks::SortForLanguage()
{
	SortedFor = LyudText::Language();
	Books.Sort([](const FLyudBookInfo& A, const FLyudBookInfo& B)
	{
		const int32 C = A.AuthorNow().Compare(B.AuthorNow(), ESearchCase::IgnoreCase);
		if (C != 0)
		{
			return C < 0;
		}
		const int32 T = A.TitleNow().Compare(B.TitleNow(), ESearchCase::IgnoreCase);
		return T != 0 ? T < 0 : A.Lang < B.Lang;
	});
	ByKey.Reset();
	for (int32 I = 0; I < Books.Num(); ++I)
	{
		ByKey.Add(Books[I].Key, I);
	}
}

const TArray<FLyudBookInfo>& ULyudBooks::Catalog()
{
	if (!bLoaded)
	{
		Load();
	}
	else if (SortedFor != LyudText::Language())
	{
		SortForLanguage();
	}
	return Books;
}

const FLyudBookInfo* ULyudBooks::Find(const FString& Key)
{
	Catalog();
	const int32* I = ByKey.Find(Key);
	return I ? &Books[*I] : nullptr;
}

bool ULyudBooks::Borrow(const FString& Key)
{
	if (IsBorrowed(Key) || BorrowedKeys.Num() >= MaxBorrowed || !Find(Key))
	{
		return false;
	}
	BorrowedKeys.Add(Key);
	BorrowDays.Add(Key, Today());
	SyncPocket();
	ULyudQuests::Notify(this, TEXT("borrow"), Key);
	UE_LOG(LogLyudBooks, Display, TEXT("Books: borrowed %s (%d on loan)"), *Key, BorrowedKeys.Num());
	return true;
}

bool ULyudBooks::Return(const FString& Key)
{
	if (BorrowedKeys.Remove(Key) == 0)
	{
		return false;
	}
	BorrowDays.Remove(Key);
	SyncPocket();
	UE_LOG(LogLyudBooks, Display, TEXT("Books: returned %s (%d on loan)"), *Key, BorrowedKeys.Num());
	return true;
}

int32 ULyudBooks::Bookmark(const FString& Key) const
{
	const int32* M = Marks.Find(Key);
	return M ? *M : 0;
}

void ULyudBooks::SetBookmark(const FString& Key, int32 Offset)
{
	Marks.Add(Key, FMath::Max(0, Offset));
}

void ULyudBooks::MarkFinished(const FString& Key)
{
	if (!FinishedKeys.Contains(Key))
	{
		FinishedKeys.Add(Key);
		ULyudQuests::Notify(this, TEXT("read_book"), Key);
		if (ULyudQuests* Q = ULyudQuests::Get(this))
		{
			FString Lang, Work;
			Q->AddNote(TEXT("reading.note.finished"), { TEXT("@book.title.") + (Key.Split(TEXT("/"), &Lang, &Work) ? Work : Key) });
		}
	}
}

int32 ULyudBooks::DayOf(const FDateTime& When)
{
	// the game's first day (UGameCalendar's StartDate, 2026-10-20) is day 0
	static const FDateTime Start(2026, 10, 20);
	return (int32)FMath::FloorToInt((When - Start).GetTotalDays());
}

int32 ULyudBooks::Today() const
{
	const UGameInstance* GI = GetGameInstance();
	return DayOf(UGameCalendar::NowIn(GI ? GI->GetWorld() : nullptr));
}

int32 ULyudBooks::BorrowedOn(const FString& Key) const
{
	const int32* D = BorrowDays.Find(Key);
	return D ? *D : Today();
}

bool ULyudBooks::AddQuote(const FString& Book, int32 Offset, const FString& Text)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	for (const FLyudBookQuote& Q : Quotes)
	{
		if (Q.Book == Book && Q.Offset == Offset)
		{
			return false;
		}
	}
	FLyudBookQuote Q;
	Q.Book = Book;
	Q.Offset = Offset;
	Q.Text = Text;
	const UGameInstance* GI = GetGameInstance();
	Q.When = UGameCalendar::NowIn(GI ? GI->GetWorld() : nullptr);
	Quotes.Add(MoveTemp(Q));
	if (Quotes.Num() > (int32)LyudReading::FQuotes::MaxQuotes)
	{
		Quotes.RemoveAt(0);
	}
	ULyudQuests::Notify(this, TEXT("quote"), Book);
	return true;
}

void ULyudBooks::NoteRead(const FString& Key, int32 Chars)
{
	if (Chars <= 0)
	{
		return;
	}
	ReadChars.FindOrAdd(Key) += Chars;
	const int32 D = Today();
	if (D != TodayDay)
	{
		TodayDay = D;
		TodayChars.Reset();
	}
	TodayChars.FindOrAdd(Key) += Chars;
}

int32 ULyudBooks::PagesRead() const
{
	int32 N = 0;
	for (const TPair<FString, int32>& P : ReadChars)
	{
		N += LyudReading::StdPages(P.Value, P.Key.StartsWith(TEXT("ja/")));
	}
	return N;
}

int32 ULyudBooks::PagesReadOf(const FString& Key) const
{
	const int32* C = ReadChars.Find(Key);
	return C ? LyudReading::StdPages(*C, Key.StartsWith(TEXT("ja/"))) : 0;
}

FString ULyudBooks::LongestFinished(int32* OutPages) const
{
	FString Best;
	int32 BestPages = 0;
	ULyudBooks* Self = const_cast<ULyudBooks*>(this);
	for (const FString& K : FinishedKeys)
	{
		const FLyudBookInfo* B = Self->Find(K);
		const int32 Pages = B ? LyudReading::StdPages(B->Chars, B->Lang == TEXT("ja")) : 0;
		if (Pages > BestPages)
		{
			Best = K;
			BestPages = Pages;
		}
	}
	if (OutPages)
	{
		*OutPages = BestPages;
	}
	return Best;
}

TArray<FString> ULyudBooks::FinishedWorks() const
{
	TArray<FString> Out;
	for (int32 I = FinishedKeys.Num() - 1; I >= 0; --I)
	{
		FString Lang, Work;
		if (!FinishedKeys[I].Split(TEXT("/"), &Lang, &Work))
		{
			Work = FinishedKeys[I];
		}
		Out.AddUnique(Work);
	}
	return Out;
}

void ULyudBooks::SyncPocket()
{
	ULyudInventory* Inv = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULyudInventory>() : nullptr;
	if (!Inv)
	{
		return;
	}
	const int32 Have = Inv->Count(ItemId);
	const int32 Want = BorrowedKeys.Num();
	if (Have < Want)
	{
		Inv->Add(ItemId, Want - Have);
	}
	else if (Have > Want)
	{
		Inv->Remove(ItemId, Have - Want);
	}
}

void ULyudBooks::ResetAll()
{
	BorrowedKeys.Reset();
	Marks.Reset();
	FinishedKeys.Reset();
	BorrowDays.Reset();
	OverdueNoticed.Reset();
	Quotes.Reset();
	ReadChars.Reset();
	TodayDay = -1;
	TodayChars.Reset();
	ClubMonths.Reset();
	Advice.Reset();
	SyncPocket();
}

// ----------------------------------------------------------------------------------------------------------- ULyudBookScreen

ULyudBookScreen* ULyudBookScreen::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<ULyudBookScreen>() : nullptr;
}

void ULyudBookScreen::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// the library's lending desk: an act with "open": "books" (tools/city/content/x_p_library.json)
	if (ULyudCityThings* Things = Collection.InitializeDependency<ULyudCityThings>())
	{
		TWeakObjectPtr<ULyudBookScreen> Self = this;
		Things->OnOpen.AddWeakLambda(this, [Self](FName Screen, int32)
		{
			if (Self.IsValid() && Screen == TEXT("books"))
			{
				Self->OpenCatalog(true);
			}
		});
	}
}

void ULyudBookScreen::Deinitialize()
{
	Close();
	Super::Deinitialize();
}

void ULyudBookScreen::Lock(bool bLock)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bLocked == bLock)
	{
		return;
	}
	bLocked = bLock;
	PC->SetIgnoreLookInput(bLock);
	PC->SetIgnoreMoveInput(bLock);
	PC->bShowMouseCursor = bLock;
	if (bLock)
	{
		PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ULyudBookScreen::Show(TSharedRef<SWidget> Widget)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}
	if (Page.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
	}
	Page = Widget;
	Viewport->AddViewportWidgetContent(Widget, 66);
	OpenedFrame = GFrameCounter;
	Lock(true);
	if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
	{
		UI->SetExternalScreen(this);
	}
}

void ULyudBookScreen::OpenCatalog(bool bAtDesk)
{
	UWorld* World = GetWorld();
	ULyudBooks* Books = ULyudBooks::Get(World);
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	if (!Books || !World || !World->GetGameViewport() || (GameScreen && GameScreen->IsOpen()))
	{
		return;
	}
	TWeakObjectPtr<ULyudBookScreen> Self = this;
	Reader.Reset();
	CatalogPage = SNew(SLyudBookCatalog)
		.Books(Books)
		.AtDesk(bAtDesk)
		.OnRead_Lambda([Self](const FString& Key) { if (Self.IsValid()) { Self->OpenReader(Key); } })
		.OnClose_Lambda([Self] { if (Self.IsValid()) { Self->Close(); } });
	Show(CatalogPage.ToSharedRef());
	UE_LOG(LogLyudBooks, Display, TEXT("Books: catalogue open (%s, %d books, %d on loan)"), bAtDesk ? TEXT("desk") : TEXT("mine"),
		Books->Catalog().Num(), Books->Borrowed().Num());
}

void ULyudBookScreen::OpenReader(const FString& InKey)
{
	UWorld* World = GetWorld();
	ULyudBooks* Books = ULyudBooks::Get(World);
	if (!Books || !World || !World->GetGameViewport())
	{
		return;
	}
	FString Key = InKey;
	if (Key.IsEmpty())
	{
		if (Books->Borrowed().Num() == 1)
		{
			Key = Books->Borrowed()[0];
		}
		else
		{
			OpenCatalog(false);
			return;
		}
	}
	const FLyudBookInfo* Book = Books->Find(Key);
	if (!Book)
	{
		UE_LOG(LogLyudBooks, Warning, TEXT("Books: no book %s"), *Key);
		return;
	}
	// the light first (docs/books.md "Reading anywhere"): too dark or raining outdoors, the book stays shut
	float Bright = 1.f;
	if (const ULyudReading* Reading = ULyudReading::Get(World))
	{
		FText Why;
		if (!Reading->CanReadHere(&Why, &Bright))
		{
			if (ULyudGameScreen* G = ULyudGameScreen::Get(World))
			{
				G->Toast(Why);
			}
			UE_LOG(LogLyudBooks, Display, TEXT("Books: not here: %s"), *Why.ToString());
			return;
		}
	}
	TWeakObjectPtr<ULyudBookScreen> Self = this;
	CatalogPage.Reset();
	SessionChars = 0;
	Reader = SNew(SLyudBookReader)
		.Book(*Book)
		.Bookmark(Books->Bookmark(Key))
		.OnClose_Lambda([Self] { if (Self.IsValid()) { Self->Close(); } });
	Reader->SetBright(Bright);
	Show(Reader.ToSharedRef());
	UE_LOG(LogLyudBooks, Display, TEXT("Books: reading %s from offset %d"), *Key, Books->Bookmark(Key));
}

FString ULyudBookScreen::OpenBook() const
{
	return Reader.IsValid() ? Reader->BookKey() : FString();
}

void ULyudBookScreen::Close()
{
	if (!Page.IsValid())
	{
		return;
	}
	if (Reader.IsValid())
	{
		if (ULyudBooks* Books = ULyudBooks::Get(GetWorld()))
		{
			if (Reader->IsLaidOut())
			{
				Books->SetBookmark(Reader->BookKey(), Reader->Offset());
			}
			// a sitting of two pages or more: a line in the diary, one a book a day
			const FString Key = Reader->BookKey();
			const bool bJa = Reader->Lang() == TEXT("ja");
			const int32 Pages = LyudReading::StdPages(SessionChars, bJa);
			const int32 Day = Books->Today();
			const int32* WasDay = NotedDay.Find(Key);
			if (SessionChars >= 2 * LyudReading::CharsPerPage(bJa) && (!WasDay || *WasDay != Day))
			{
				NotedDay.Add(Key, Day);
				if (ULyudQuests* Q = ULyudQuests::Get(GetWorld()))
				{
					FString Lang, Work;
					Q->AddNote(TEXT("reading.note.session"), { TEXT("@book.title.") + (Key.Split(TEXT("/"), &Lang, &Work) ? Work : Key), FString::FromInt(Pages) });
				}
			}
			SessionChars = 0;
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(Page.ToSharedRef());
		}
		if (ULyudInventoryUI* UI = ULyudInventoryUI::Get(World))
		{
			UI->SetExternalScreen(nullptr);
		}
	}
	Page.Reset();
	Reader.Reset();
	CatalogPage.Reset();
	Lock(false);
}

void ULyudBookScreen::Turn(int32 Spreads)
{
	if (!Reader.IsValid())
	{
		return;
	}
	const int32 Read = Reader->Turn(Spreads);
	if (Read > 0)
	{
		PassReadingTime(Read, Reader->Lang());
		SessionChars += Read;
	}
	if (ULyudBooks* Books = ULyudBooks::Get(GetWorld()))
	{
		Books->NoteRead(Reader->BookKey(), Read);
		Books->SetBookmark(Reader->BookKey(), Reader->Offset());
		if (Reader->AtEnd())
		{
			Books->MarkFinished(Reader->BookKey());
		}
	}
}

void ULyudBookScreen::SetPageBright(float Bright)
{
	if (Reader.IsValid())
	{
		Reader->SetBright(Bright);
	}
}

bool ULyudBookScreen::CopyQuote(bool bFirst)
{
	ULyudBooks* Books = ULyudBooks::Get(GetWorld());
	if (!Reader.IsValid() || !Books)
	{
		return false;
	}
	if (bFirst && !Reader->InQuote() && !Reader->EnterQuote())
	{
		return false;
	}
	FString Text;
	int32 Offset = 0;
	ULyudGameScreen* G = ULyudGameScreen::Get(GetWorld());
	if (!Reader->ChosenQuote(Text, Offset))
	{
		return false;
	}
	const bool bNew = Books->AddQuote(Reader->BookKey(), Offset, Text);
	Reader->LeaveQuote();
	if (G)
	{
		// the toast shows its start only
		const FString Short = Text.Len() > 60 ? Text.Left(57) + TEXT("…") : Text;
		G->Toast(bNew ? LyudText::Get(TEXT("reading.quote.saved"), { FStringFormatArg(Short) }) : LyudText::Get(TEXT("reading.quote.again")));
	}
	UE_LOG(LogLyudBooks, Display, TEXT("Books: quote from %s at %d: %s"), *Reader->BookKey(), Offset, bNew ? TEXT("copied") : TEXT("already there"));
	return bNew;
}

void ULyudBookScreen::PassReadingTime(int32 Chars, const FString& Lang)
{
	// a spread of a novel is two or three minutes; never more than five a turn (a page of a dictionary-size book flipped through)
	const double Minutes = FMath::Clamp((double)Chars / LyudBooksImpl::CharsPerMinute(Lang), 0.0, 5.0);
	PendingSeconds += Minutes * 60.0;
}

void ULyudBookScreen::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !World->IsGameWorld())
	{
		return;
	}
	// the reading time goes onto the clock a little each frame (the people's life steps a second at a time)
	if (PendingSeconds > 0.0)
	{
		if (UGameCalendar* Cal = World->GetSubsystem<UGameCalendar>())
		{
			const double Add = FMath::Min(PendingSeconds, 30.0);
			Cal->SetNow(Cal->Now() + FTimespan::FromSeconds(Add));
			PendingSeconds -= Add;
		}
		else
		{
			PendingSeconds = 0.0;
		}
	}
	if (!IsOpen() || OpenedFrame == GFrameCounter)
	{
		return;
	}
	const ULyudGameMenuSubsystem* Menu = World->GetSubsystem<ULyudGameMenuSubsystem>();
	if (Menu && Menu->IsOpen())
	{
		return;  // the pause menu is over us: wait under it
	}
	const ULyudGameScreen* GameScreen = ULyudGameScreen::Get(World);
	if ((GameScreen && GameScreen->IsOpen()) || PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::Tab))
	{
		Close();
		return;
	}
	if (CatalogPage.IsValid())
	{
		return;  // the catalogue takes its keys through Slate (the search box, the buttons)
	}
	if (!Reader.IsValid())
	{
		return;
	}
	auto Pressed = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	if (Reader->InQuote())
	{
		if (Pressed(EKeys::C))
		{
			Reader->LeaveQuote();
		}
		else if (Pressed(EKeys::W) || Pressed(EKeys::Up) || Pressed(EKeys::A) || Pressed(EKeys::Left))
		{
			Reader->QuoteStep(-1);
		}
		else if (Pressed(EKeys::S) || Pressed(EKeys::Down) || Pressed(EKeys::D) || Pressed(EKeys::Right))
		{
			Reader->QuoteStep(1);
		}
		else if (Pressed(EKeys::E) || Pressed(EKeys::Enter) || Pressed(EKeys::SpaceBar))
		{
			CopyQuote();
		}
		Reader->TakeAskedTurns();
		return;
	}
	if (Pressed(EKeys::C) && !Reader->ContentsShown())
	{
		if (!Reader->EnterQuote())
		{
			if (ULyudGameScreen* G = ULyudGameScreen::Get(World))
			{
				G->Toast(LyudText::Get(TEXT("reading.quote.none")));
			}
		}
		return;
	}
	if (Reader->ContentsShown())
	{
		if (Pressed(EKeys::Q) || Pressed(EKeys::BackSpace))
		{
			Reader->ToggleContents();
		}
		else if (Pressed(EKeys::W) || Pressed(EKeys::Up))
		{
			Reader->ScrollContents(-1);
		}
		else if (Pressed(EKeys::S) || Pressed(EKeys::Down))
		{
			Reader->ScrollContents(1);
		}
		return;
	}
	if (Pressed(EKeys::D) || Pressed(EKeys::Right) || Pressed(EKeys::PageDown) || Pressed(EKeys::E) || Pressed(EKeys::SpaceBar))
	{
		Turn(1);
	}
	else if (Pressed(EKeys::A) || Pressed(EKeys::Left) || Pressed(EKeys::PageUp))
	{
		Turn(-1);
	}
	else if (Pressed(EKeys::Home))
	{
		Reader->GoToOffset(0);
	}
	else if (Pressed(EKeys::Q))
	{
		Reader->ToggleContents();
	}
	else if (Pressed(EKeys::Equals) || Pressed(EKeys::Add))
	{
		Reader->StepFont(1);
	}
	else if (Pressed(EKeys::Hyphen) || Pressed(EKeys::Subtract))
	{
		Reader->StepFont(-1);
	}
	// the reader's own clicks and wheel turn pages too (SLyudBookReader asks to be turned through this)
	if (const int32 Asked = Reader->TakeAskedTurns())
	{
		Turn(Asked);
	}
}

// ------------------------------------------------------------------------------------------------------------------- console

static FAutoConsoleCommandWithWorldAndArgs GBooksCmd(TEXT("lyud.Books"),
	TEXT("lyud.Books list [lang] | borrow <key> | return <key> | read [key] | desk | mine | turn <n> | quote | close | reset | stats"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ULyudBooks* Books = ULyudBooks::Get(World);
		ULyudBookScreen* Screen = ULyudBookScreen::Get(World);
		if (!Books || !Screen)
		{
			return;
		}
		const FString Cmd = Args.Num() > 0 ? Args[0].ToLower() : TEXT("stats");
		const FString Arg = Args.Num() > 1 ? Args[1] : FString();
		if (Cmd == TEXT("list"))
		{
			int32 N = 0;
			for (const FLyudBookInfo& B : Books->Catalog())
			{
				if (Arg.IsEmpty() || B.Lang == Arg)
				{
					UE_LOG(LogLyudBooks, Display, TEXT("  %-28s %s — %s (%d), %d chars%s"), *B.Key, *B.AuthorNow(), *B.TitleNow(), B.Year, B.Chars,
						Books->IsBorrowed(B.Key) ? TEXT(" [on loan]") : TEXT(""));
					++N;
				}
			}
			UE_LOG(LogLyudBooks, Display, TEXT("Books: %d listed"), N);
		}
		else if (Cmd == TEXT("borrow"))
		{
			UE_LOG(LogLyudBooks, Display, TEXT("Books: borrow %s: %d"), *Arg, Books->Borrow(Arg) ? 1 : 0);
		}
		else if (Cmd == TEXT("return"))
		{
			UE_LOG(LogLyudBooks, Display, TEXT("Books: return %s: %d"), *Arg, Books->Return(Arg) ? 1 : 0);
		}
		else if (Cmd == TEXT("read"))
		{
			if (!Arg.IsEmpty() && !Books->IsBorrowed(Arg))
			{
				Books->Borrow(Arg);
			}
			Screen->OpenReader(Arg);
		}
		else if (Cmd == TEXT("desk"))
		{
			Screen->OpenCatalog(true);
		}
		else if (Cmd == TEXT("mine"))
		{
			Screen->OpenCatalog(false);
		}
		else if (Cmd == TEXT("turn"))
		{
			Screen->Turn(Arg.IsEmpty() ? 1 : FCString::Atoi(*Arg));
		}
		else if (Cmd == TEXT("close"))
		{
			Screen->Close();
		}
		else if (Cmd == TEXT("reset"))
		{
			Books->ResetAll();
		}
		else if (Cmd == TEXT("quote"))
		{
			Screen->CopyQuote(true);
		}
		else
		{
			UE_LOG(LogLyudBooks, Display, TEXT("Books: %d in the catalogue, %d on loan, %d read to the end, reading: %s"),
				Books->Catalog().Num(), Books->Borrowed().Num(), Books->NumFinished(), *Screen->OpenBook());
		}
	}));

// ------------------------------------------------------------------------------------------------------------ developer mode

static LyudDev::FAutoAction GDevBooksDesk(TEXT("books.desk"), TEXT("books"), TEXT("dev.action.books.desk"), [](UWorld* W)
{
	// to the library's lending desk, then its catalogue (the librarian's hours are not asked here)
	if (ULyudCityThings* Things = ULyudCityThings::Get(W))
	{
		Things->GoToThing(TEXT("P-LIBRARY/lending_counter_2"));
	}
	if (ULyudBookScreen* S = ULyudBookScreen::Get(W))
	{
		S->OpenCatalog(true);
	}
});

static LyudDev::FAutoAction GDevBooksRead(TEXT("books.read"), TEXT("books"), TEXT("dev.action.books.read"), [](UWorld* W)
{
	// a book in the UI's language (else any), borrowed and opened where he stands
	ULyudBooks* Books = ULyudBooks::Get(W);
	ULyudBookScreen* S = ULyudBookScreen::Get(W);
	if (!Books || !S)
	{
		return;
	}
	FString Key = Books->Borrowed().Num() > 0 ? Books->Borrowed()[0] : FString();
	for (const FLyudBookInfo& B : Books->Catalog())
	{
		if (!Key.IsEmpty())
		{
			break;
		}
		if (B.Lang == LyudText::Language())
		{
			Key = B.Key;
		}
	}
	if (Key.IsEmpty() && Books->Catalog().Num() > 0)
	{
		Key = Books->Catalog()[0].Key;
	}
	if (!Key.IsEmpty())
	{
		Books->Borrow(Key);
		S->OpenReader(Key);
	}
});

static LyudDev::FAutoAction GDevBooksBench(TEXT("books.bench"), TEXT("books"), TEXT("dev.action.books.bench"), [](UWorld* W)
{
	if (ULyudBenchSit* B = ULyudBenchSit::Get(W))
	{
		B->SitNearest(6000.f);
	}
});

static LyudDev::FAutoAction GDevBooksReset(TEXT("books.reset"), TEXT("books"), TEXT("dev.action.books.reset"), [](UWorld* W)
{
	if (ULyudBooks* Books = ULyudBooks::Get(W))
	{
		Books->ResetAll();
	}
});

static LyudDev::FAutoTeleport GDevParkBench(TEXT("books.park_bench"), TEXT("books"), TEXT("dev.tp.books.park_bench"), [](UWorld* W, FTransform& Out)
{
	// a step in front of a bench in the park (the first one the town plan lists there)
	const ULyudPublicSpotsSubsystem* Spots = ULyudPublicSpotsSubsystem::Get(W);
	if (!Spots)
	{
		return false;
	}
	const TArray<FLyudPublicSpot> Benches = Spots->GetSpots(TEXT("bench"));
	const FLyudPublicSpot* Pick = Benches.FindByPredicate([](const FLyudPublicSpot& S) { return S.Id.ToString().StartsWith(TEXT("S-BENCH-PK")); });
	if (!Pick && Benches.Num() > 0)
	{
		Pick = &Benches[0];
	}
	if (!Pick)
	{
		return false;
	}
	const FVector Front = Pick->Location + Pick->Rotation.Vector() * 140.f + FVector(0.f, 0.f, 100.f);
	Out = FTransform(FRotator(0.f, Pick->Rotation.Yaw + 180.f, 0.f), Front);
	return true;
});
