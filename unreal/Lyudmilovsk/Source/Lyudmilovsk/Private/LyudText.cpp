#include "LyudText.h"

#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

#include <atomic>

static TAutoConsoleVariable<FString> CVarLanguage(
	TEXT("lyud.Language"), TEXT("ru"),
	TEXT("Language of the game's words and of the textures that carry words: ru, en, or any strings_<lang>.json in Content/Localization."));

namespace
{
	TMap<FString, TMap<FString, FString>>& Tables()
	{
		static TMap<FString, TMap<FString, FString>> T;
		return T;
	}

	/** The tiny subset of JSON the string tables use: one flat object of string values. */
	TMap<FString, FString> ParseFlat(const FString& Json)
	{
		TMap<FString, FString> Out;
		int32 I = 0;
		const int32 N = Json.Len();
		auto ReadString = [&](FString& S) -> bool
		{
			while (I < N && Json[I] != TEXT('"')) { ++I; }
			if (I >= N) { return false; }
			++I;
			S.Reset();
			while (I < N && Json[I] != TEXT('"'))
			{
				TCHAR C = Json[I++];
				if (C == TEXT('\\') && I < N)
				{
					const TCHAR E = Json[I++];
					switch (E)
					{
					case TEXT('n'): C = TEXT('\n'); break;
					case TEXT('t'): C = TEXT('\t'); break;
					case TEXT('u'):
					{
						C = (TCHAR)FCString::Strtoi(*Json.Mid(I, 4), nullptr, 16);
						I += 4;
						break;
					}
					default: C = E; break;
					}
				}
				S.AppendChar(C);
			}
			++I;
			return true;
		};
		while (I < N)
		{
			FString Key, Value;
			if (!ReadString(Key))
			{
				break;
			}
			while (I < N && Json[I] != TEXT(':')) { ++I; }
			if (!ReadString(Value))
			{
				break;
			}
			Out.Add(Key, Value);
		}
		return Out;
	}

	const TMap<FString, FString>& Table(const FString& Lang)
	{
		if (TMap<FString, FString>* T = Tables().Find(Lang))
		{
			return *T;
		}
		FString Text;
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Localization") / FString::Printf(TEXT("strings_%s.json"), *Lang)));
		return Tables().Add(Lang, ParseFlat(Text));
	}
}

namespace
{
	/**
	 * The current language as an immutable, never-freed string: Language() is called every frame by the HUD and screens and may be
	 * read off the game thread, so a cvar change swaps an atomic pointer to an interned copy instead of rewriting a shared string.
	 */
	std::atomic<const FString*> GLanguage{ nullptr };

	const FString* InternLanguage(const FString& Raw)
	{
		static FCriticalSection Lock;
		static TArray<const FString*> Interned;   // leaked on purpose: readers may hold a reference
		const FString L = Raw.IsEmpty() ? FString(TEXT("ru")) : Raw;
		FScopeLock Guard(&Lock);
		for (const FString* S : Interned)
		{
			if (S->Equals(L, ESearchCase::CaseSensitive))
			{
				return S;
			}
		}
		const FString* S = new FString(L);
		Interned.Add(S);
		return S;
	}

	void RefreshLanguage(IConsoleVariable* Var)
	{
		GLanguage.store(InternLanguage(Var ? Var->GetString() : FString()));
	}

	/** Paths LoadObject already failed for: a missing label or voice is not looked up on disk again (cleared by Reload). */
	TSet<FString>& Missing()
	{
		static TSet<FString> M;
		return M;
	}

	/** PathOf(lang) in the current language, else in Russian (tried once: not again when the language is Russian). */
	template <typename T, typename FPathOf>
	T* LoadLocalized(FPathOf PathOf)
	{
		const FString Lang = LyudText::Language();
		const bool bGameThread = IsInGameThread();
		const FString Langs[2] = { Lang, FString(TEXT("ru")) };
		const int32 Tries = Lang == TEXT("ru") ? 1 : 2;
		for (int32 I = 0; I < Tries; ++I)
		{
			const FString Path = PathOf(Langs[I]);
			if (bGameThread && Missing().Contains(Path))
			{
				continue;
			}
			if (T* Obj = LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Obj;
			}
			if (bGameThread)
			{
				Missing().Add(Path);
			}
		}
		return nullptr;
	}
}

namespace LyudText
{
	const FString& Language()
	{
		static const bool bBound = []
		{
			IConsoleVariable* Var = CVarLanguage.AsVariable();
			RefreshLanguage(Var);
			Var->OnChangedDelegate().AddStatic(&RefreshLanguage);
			return true;
		}();
		(void)bBound;
		return *GLanguage.load();
	}

	void Reload()
	{
		Tables().Reset();
		Missing().Reset();
	}

	FText Get(const FString& Key, const TArray<FStringFormatArg>& Args)
	{
		const FString* Found = Table(Language()).Find(Key);
		if (!Found)
		{
			Found = Table(TEXT("ru")).Find(Key);
		}
		if (!Found)
		{
			return FText::FromString(Key);
		}
		return FText::FromString(Args.Num() ? FString::Format(**Found, Args) : *Found);
	}

	UTexture2D* LoadTexture(const FString& Folder, const FString& Name)
	{
		return LoadLocalized<UTexture2D>([&](const FString& Lang) { return FString::Printf(TEXT("/Game/%s/Localized/%s/%s.%s"), *Folder, *Lang, *Name, *Name); });
	}

	USoundBase* LoadVoice(const FString& Key)
	{
		const FString Name = Key.Replace(TEXT("."), TEXT("_"));
		return LoadLocalized<USoundBase>([&](const FString& Lang) { return FString::Printf(TEXT("/Game/Voice/%s/V_%s.V_%s"), *Lang, *Name, *Name); });
	}
}
