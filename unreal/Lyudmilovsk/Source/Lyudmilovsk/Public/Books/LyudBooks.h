#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "LyudBooks.generated.h"

class SWidget;
class SLyudBookReader;

/** A sentence copied from a book into the diary's «Цитаты» (docs/books.md "Quotes"). */
USTRUCT()
struct FLyudBookQuote
{
	GENERATED_BODY()

	/** "<lang>/<work>". */
	UPROPERTY(SaveGame) FString Book;
	/** Where the sentence starts in the text. */
	UPROPERTY(SaveGame) int32 Offset = 0;
	UPROPERTY(SaveGame) FString Text;
	UPROPERTY(SaveGame) FDateTime When;
};

/** A book he recommended to a friend at the lending desk; the friend writes what they thought some days later. */
USTRUCT()
struct FLyudBookAdvice
{
	GENERATED_BODY()

	/** The work id (any language). */
	UPROPERTY(SaveGame) FString Work;
	/** The friend's key (t_*). */
	UPROPERTY(SaveGame) FString Friend;
	UPROPERTY(SaveGame) int32 Day = 0;
	UPROPERTY(SaveGame) bool bAnswered = false;
};
class SLyudBookCatalog;
class ULyudSaveGame;

/** One text of the library's classics (Content/Books/books.json, written by tools/books/fetch_books.py). */
struct FLyudBookInfo
{
	/** "<lang>/<work id>": a work in several languages is several books. */
	FString Key;
	FString Work;
	/** The language the text is in: ru, en, ja. */
	FString Lang;
	/** The work's title and author in each UI language (ru, en, ja). */
	TMap<FString, FString> Title;
	TMap<FString, FString> Author;
	int32 Year = 0;
	/** Relative to Content/Books. */
	FString File;
	/** "Constance Garnett (translator, d. 1946)" lines and the licence, shown on the book's last page. */
	TArray<FString> Credits;
	FString License;
	FString Source;
	int32 Chars = 0;

	/** In the UI's language, else English, else any. */
	const FString& TitleNow() const;
	const FString& AuthorNow() const;
	/** In the book's own language (what is printed on its cover). */
	const FString& TitleOwn() const;
};

/**
 * The town library's classics (docs/books.md): the catalogue of the open texts downloaded into Content/Books, the books the
 * hero has borrowed (each one is a "library_book" in his pocket) and the page he left each book at. Saved as "Books".
 *
 *   ULyudBooks* B = ULyudBooks::Get(this);
 *   B->Borrow("ru/onegin");                  // at the lending desk; false when he already has MaxBorrowed
 *   ULyudBookScreen::Get(this)->OpenReader("ru/onegin");
 */
UCLASS()
class LYUDMILOVSK_API ULyudBooks : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBooks* Get(const UObject* WorldContext);
	/** The pocket's item: one per book on loan. */
	static const FName ItemId;
	static constexpr int32 MaxBorrowed = 5;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The catalogue, sorted by author then title in the UI's language (read on first use; empty when the texts were not fetched). */
	const TArray<FLyudBookInfo>& Catalog();
	const FLyudBookInfo* Find(const FString& Key);
	/** The absolute path of a book's text. */
	static FString PathOf(const FLyudBookInfo& Book);

	bool Borrow(const FString& Key);
	bool Return(const FString& Key);
	bool IsBorrowed(const FString& Key) const { return BorrowedKeys.Contains(Key); }
	const TArray<FString>& Borrowed() const { return BorrowedKeys; }

	/** Where he left the book: a character offset into its text (0: not opened). */
	int32 Bookmark(const FString& Key) const;
	void SetBookmark(const FString& Key, int32 Offset);
	/** Read to its last page at least once. */
	bool IsFinished(const FString& Key) const { return FinishedKeys.Contains(Key); }
	void MarkFinished(const FString& Key);
	int32 NumFinished() const { return FinishedKeys.Num(); }

	// --- reading anywhere (docs/books.md "Reading anywhere"): the day a loan was taken, the quotes, the reading stats

	/** Whole game days since the game began (UGameCalendar's StartDate: day 0). */
	static int32 DayOf(const FDateTime& When);
	int32 Today() const;
	/** The day a book on loan was taken (today's for a loan older than this record). */
	int32 BorrowedOn(const FString& Key) const;
	/** Copies a sentence into the diary; false when that very sentence is there already. */
	bool AddQuote(const FString& Book, int32 Offset, const FString& Text);
	const TArray<FLyudBookQuote>& GetQuotes() const { return Quotes; }
	/** The reader turned forward over Chars characters of a book (the stats, the diary's day line). */
	void NoteRead(const FString& Key, int32 Chars);
	/** Standard pages read in all, and of one book. */
	int32 PagesRead() const;
	int32 PagesReadOf(const FString& Key) const;
	/** The finished book with the most standard pages ("" none). */
	FString LongestFinished(int32* OutPages = nullptr) const;
	/** Pages read today of each book (the diary's day entry: docs/interactives_backlog.md item 23 may ask). */
	const TMap<FString, int32>& ReadToday() const { return TodayChars; }
	/** The work ids he finished, the latest first (any language). */
	TArray<FString> FinishedWorks() const;

	/** Makes the pocket's count of library_book match the books on loan (after a load, a new game, a console change). */
	void SyncPocket();

	/** Developer mode and tests: everything returned, every bookmark gone. */
	void ResetAll();

	UPROPERTY(SaveGame)
	TArray<FString> BorrowedKeys;
	UPROPERTY(SaveGame)
	TMap<FString, int32> Marks;
	UPROPERTY(SaveGame)
	TArray<FString> FinishedKeys;
	/** The day each book on loan was taken (DayOf). */
	UPROPERTY(SaveGame)
	TMap<FString, int32> BorrowDays;
	/** Loans the librarian has written about already ("<key>@<day taken>"). */
	UPROPERTY(SaveGame)
	TArray<FString> OverdueNoticed;
	UPROPERTY(SaveGame)
	TArray<FLyudBookQuote> Quotes;
	/** Characters read forward, by book. */
	UPROPERTY(SaveGame)
	TMap<FString, int32> ReadChars;
	/** Today's reading (its day), by book: the diary's line when the reader shuts. */
	UPROPERTY(SaveGame)
	int32 TodayDay = -1;
	UPROPERTY(SaveGame)
	TMap<FString, int32> TodayChars;
	/** The book club: the months he joined the talk ("YYYY-MM"). */
	UPROPERTY(SaveGame)
	TArray<FString> ClubMonths;
	/** Books he recommended to friends. */
	UPROPERTY(SaveGame)
	TArray<FLyudBookAdvice> Advice;

private:
	TArray<FLyudBookInfo> Books;
	TMap<FString, int32> ByKey;
	FString SortedFor;
	bool bLoaded = false;
	FDelegateHandle LoadHandle;
	FDelegateHandle NewGameHandle;
	void Load();
	void SortForLanguage();
};

/**
 * The library's screens: the lending desk's catalogue (OpenCatalog: take a book, give one back, read one he has) and the
 * open book (OpenReader: two pages, A/D or the arrows or the wheel turn, Q the contents, +/- the type size, Esc or Tab shut
 * it). Like the bird guide it does not pause the game: walking and looking stop, the clock goes on, and each page read
 * moves it on the time it takes to read it (seated on a bench in the park, the afternoon goes by).
 */
UCLASS()
class LYUDMILOVSK_API ULyudBookScreen : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static ULyudBookScreen* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(ULyudBookScreen, STATGROUP_Tickables); }

	/** The lending desk's catalogue (bAtDesk: books can be taken and given back; else only his own books, to read). */
	void OpenCatalog(bool bAtDesk);
	/** Opens a borrowed book at its bookmark; an empty key: the only book he has, or his books to choose from. */
	void OpenReader(const FString& Key);
	void Close();
	bool IsOpen() const { return Page.IsValid(); }
	bool IsReading() const { return Reader.IsValid(); }
	/** The book open now ("" when none). */
	FString OpenBook() const;

	/** Console and tests: turn pages (+ forward, - back) as if the keys were pressed. */
	void Turn(int32 Spreads);
	/** How bright the open page is drawn (ULyudReading: the dusk outdoors). */
	void SetPageBright(float Bright);
	/** Copies the sentence chosen in the quote mode (or, with bFirst, the spread's first sentence) into the diary. */
	bool CopyQuote(bool bFirst = false);

private:
	TSharedPtr<SWidget> Page;
	TSharedPtr<SLyudBookReader> Reader;
	TSharedPtr<SLyudBookCatalog> CatalogPage;
	uint64 OpenedFrame = 0;
	bool bLocked = false;
	void Lock(bool bLock);
	void Show(TSharedRef<SWidget> Widget);
	/** The game minutes the page just read took (by its characters), passed on the clock in a few small steps. */
	void PassReadingTime(int32 Chars, const FString& Lang);
	double PendingSeconds = 0.0;
	/** Characters read forward since the book was opened (the diary's line when it shuts). */
	int32 SessionChars = 0;
	/** The book and day of the last diary line about reading (one a book a day). */
	TMap<FString, int32> NotedDay;
};
