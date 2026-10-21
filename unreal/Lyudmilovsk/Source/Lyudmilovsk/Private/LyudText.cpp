#include "LyudText.h"

#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

namespace LyudText
{
	FString Language()
	{
		const FString L = CVarLanguage.GetValueOnAnyThread();
		return L.IsEmpty() ? TEXT("ru") : L;
	}

	void Reload()
	{
		Tables().Reset();
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
		for (const FString& Lang : { Language(), FString(TEXT("ru")) })
		{
			const FString Path = FString::Printf(TEXT("/Game/%s/Localized/%s/%s.%s"), *Folder, *Lang, *Name, *Name);
			if (UTexture2D* T = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return T;
			}
		}
		return nullptr;
	}

	USoundBase* LoadVoice(const FString& Key)
	{
		const FString Name = Key.Replace(TEXT("."), TEXT("_"));
		for (const FString& Lang : { Language(), FString(TEXT("ru")) })
		{
			const FString Path = FString::Printf(TEXT("/Game/Voice/%s/V_%s.V_%s"), *Lang, *Name, *Name);
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return S;
			}
		}
		return nullptr;
	}
}
