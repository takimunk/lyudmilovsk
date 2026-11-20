#pragma once

#include "CoreMinimal.h"
#include "LyudText.h"
#include "Widgets/SCompoundWidget.h"

class IAtonShell;

/** Base of every ATON OS program shown inside a window. */
class SAtonProgram : public SCompoundWidget
{
public:
	/** The widget that should get the keyboard when the window comes to the front. */
	virtual TSharedPtr<SWidget> FocusTarget() const { return nullptr; }
	/** Called when the program is opened (again) with an argument, e.g. a note name or an address. */
	virtual void HandleArg(const FString& Arg) {}
};

struct FAtonAppInfo
{
	FName Id;
	/** String key of the program name, ui.aton.app.<id>. */
	const TCHAR* TitleKey = nullptr;
	FText Title() const { return LyudText::Get(TitleKey); }
	/** Drawn on the desktop icon tile. */
	FString Glyph;
	FLinearColor Tile;
	FVector2D WindowSize;
	bool bOnDesktop = true;
	/** The machines that have it: AtonApps::Club (the hero's computer), AtonApps::Lab (the Academgorodok terminals), or both. */
	int32 Profiles = 1;
};

namespace AtonApps
{
	const FName Computer("computer");
	const FName Notes("notes");
	const FName Browser("browser");
	const FName Terminal("terminal");
	const FName Mail("mail");
	const FName Calculator("calculator");
	const FName Trash("trash");
	const FName Chosen("chosen");
	const FName About("about");
	// the lab network's programs (AtonLabApps.cpp, docs/aton_lab.md)
	const FName Session("session");
	const FName LabMail("labmail");
	const FName Adventure("adventure");
	const FName Chess("chess");
	const FName Weather("weather");
	const FName Scope("scope");

	/** The machine's profile names and their bits in FAtonAppInfo::Profiles. */
	const FName ProfileClub("club");
	const FName ProfileLab("lab");
	constexpr int32 Club = 1;
	constexpr int32 Lab = 2;
	inline int32 ProfileBit(FName Profile) { return Profile == ProfileLab ? Lab : Club; }

	/**
	 * Where the Chosen game is loaded from: a local build first (tools/chosen_local.sh serves it), the
	 * public site when that is not running. Set by AAtonComputer.
	 */
	extern FString ChosenLocalURL;
	extern FString ChosenURL;

	const TArray<FAtonAppInfo>& List();
	const FAtonAppInfo* Find(FName Id);
	TSharedRef<SAtonProgram> Make(FName Id, IAtonShell& Shell);

	/** Notes live as text files in Saved/AtonOS/Notes; deleted ones go to Saved/AtonOS/Trash. */
	FString NotesDir();
	FString TrashDir();
	/** Note file names (without folder), oldest first. */
	TArray<FString> ListNotes(const FString& Dir);
	/** The first line of a note, shortened: what lists show as its name. */
	FString NoteTitle(const FString& Dir, const FString& File);
}
