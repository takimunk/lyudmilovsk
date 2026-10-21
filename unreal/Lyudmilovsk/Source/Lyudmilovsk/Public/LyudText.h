#pragma once

#include "CoreMinimal.h"

class UTexture2D;
class USoundBase;

/**
 * Every word the game shows comes from Content/Localization/strings_<lang>.json (flat "key": "text"), the same files
 * tools/gen_localized.py prints on textures. The language is the console variable lyud.Language ("ru" by default; "en" is there too);
 * a key missing in it falls back to Russian, then to the key itself. Textures that carry words live in
 * /Game/<Folder>/Localized/<lang>/ (labels, icons), with the same fallback. Strings are re-read at every level start, so an
 * edit shows on the next Play, and the language switches the same way (or by the next menu / bottle). docs/localization.md.
 */
namespace LyudText
{
	FString Language();

	/** The text for Key; {0} {1} ... in it are filled from Args. */
	FText Get(const FString& Key, const TArray<FStringFormatArg>& Args = TArray<FStringFormatArg>());

	/** Drops the cached files: the next Get reads them again. */
	void Reload();

	/** /Game/<Folder>/Localized/<lang>/<Name> in the current language, else in Russian; null if neither exists. */
	UTexture2D* LoadTexture(const FString& Folder, const FString& Name);

	/** The voice of line Key (line.<speaker>.<scene>.<n>): /Game/Voice/<lang>/V_<key with dots as underscores>, else the Russian one; null if none. */
	USoundBase* LoadVoice(const FString& Key);
}
