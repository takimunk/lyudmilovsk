#include "BirdLook.h"

namespace BirdLook
{
	void FPaint::Write(float* Out) const
	{
		Out[0] = Base.R; Out[1] = Base.G; Out[2] = Base.B;
		Out[3] = Accent.R; Out[4] = Accent.G; Out[5] = Accent.B;
		Out[6] = Tip; Out[7] = Root; Out[8] = Fringe; Out[9] = Bars; Out[10] = Spot; Out[11] = Gloss; Out[12] = Hue;
	}

	namespace
	{
		FLinearColor C(const TCHAR* Hex) { return FLinearColor::FromSRGBColor(FColor::FromHex(Hex)); }

		/** Paint builder: P(base, accent).TipF(..).Fr(..) ... */
		struct P : FPaint
		{
			P(const TCHAR* B, const TCHAR* A = nullptr) { Base = C(B); Accent = A ? C(A) : Base * 0.6f; }
			P& TipF(float V) { Tip = V; return *this; }
			P& RootF(float V) { Root = V; return *this; }
			P& Fr(float V) { Fringe = V; return *this; }
			P& Bar(float V) { Bars = V; return *this; }
			P& Sp(float V) { Spot = V; return *this; }
			P& Gl(float G, float H) { Gloss = G; Hue = H; return *this; }
		};
		FGroup G(const FPaint& Paint) { FGroup Out; Out.Paint = Paint; Out.Outer = Paint; return Out; }
		FGroup G(const FPaint& Paint, int32 N, const FPaint& Outer) { FGroup Out; Out.Paint = Paint; Out.OuterCount = N; Out.Outer = Outer; return Out; }

		/** Every group the same except where a species says otherwise. */
		FLook Plain(const FPaint& Wing, const FPaint& Coverts, const FPaint& Tail, const FPaint& Breast, const FPaint& Back)
		{
			FLook L;
			L.Primaries = L.Secondaries = L.Tertials = G(Wing);
			L.GreaterCoverts = L.LesserCoverts = L.PrimaryCoverts = G(Coverts);
			L.Scapulars = L.Back = G(Back);
			L.Tail = G(Tail);
			L.Breast = G(Breast);
			return L;
		}

		TMap<FName, FLook> Build()
		{
			TMap<FName, FLook> M;
			auto Add = [&M](const TCHAR* Id, FLook L) { M.Add(FName(Id), MoveTemp(L)); };

			// --- crows: black with a sheen, broad fingered wings
			{
				const P Black = P(TEXT("0b0b10"), TEXT("05050a")).Gl(0.7f, 0.85f);
				FLook L = Plain(Black, Black, Black, P(TEXT("15141a")).Gl(0.4f, 0.85f), Black);
				L.Beak = C(TEXT("2a2a2e")); L.BeakBase = C(TEXT("c9c3bd")); L.Legs = C(TEXT("1a1a1e"));
				L.BeakCm = 5.5f; L.BeakDepth = 0.32f; L.BeakHook = 0.15f; L.Wing = EWing::Broad; L.TailShape = ETail::Rounded; L.TailFrac = 0.38f;
				Add(TEXT("Rook"), L);
			}
			{
				const P Black = P(TEXT("111114")).Gl(0.4f, 0.8f);
				const P Grey = P(TEXT("8e8a86"), TEXT("6e6a66"));
				FLook L = Plain(Black, Black, Black, Grey, Grey);
				L.Beak = L.BeakBase = C(TEXT("101012")); L.Legs = C(TEXT("101012"));
				L.BeakCm = 5.4f; L.BeakDepth = 0.36f; L.BeakHook = 0.2f; L.Wing = EWing::Broad; L.TailShape = ETail::Rounded; L.TailFrac = 0.37f;
				Add(TEXT("HoodedCrow"), L);
			}
			{
				const P Dark = P(TEXT("1c1c22")).Gl(0.35f, 0.9f);
				FLook L = Plain(Dark, Dark, Dark, P(TEXT("3a3a40")), P(TEXT("2a2a30")));
				L.Beak = L.BeakBase = C(TEXT("101012")); L.Legs = C(TEXT("101012"));
				L.BeakCm = 2.5f; L.BeakDepth = 0.45f; L.BeakHook = 0.1f; L.Wing = EWing::Broad; L.TailFrac = 0.36f;
				Add(TEXT("Jackdaw"), L);
			}
			{
				const P Black = P(TEXT("0c0f12")).Gl(0.75f, 0.55f);
				FLook L = Plain(Black, Black, P(TEXT("0e1210")).Gl(0.9f, 0.1f), P(TEXT("f2f2ee")), Black);
				// the white of the primaries' inner webs flashes in every beat
				L.Primaries = G(P(TEXT("f0f0ec"), TEXT("0c0f12")).TipF(0.22f).Fr(0.18f));
				L.Scapulars = G(P(TEXT("f2f2ee"), TEXT("dcdcd6")));
				L.Beak = L.BeakBase = C(TEXT("0e0e10")); L.Legs = C(TEXT("101012"));
				L.BeakCm = 3.5f; L.BeakDepth = 0.38f; L.BeakHook = 0.12f; L.TailShape = ETail::Long; L.TailFrac = 0.52f;
				Add(TEXT("Magpie"), L);
			}
			// --- pigeons
			{
				const P Grey = P(TEXT("8a8f98"), TEXT("23252b"));
				FLook L = Plain(P(TEXT("4a4d55"), TEXT("2c2e33")), Grey, P(TEXT("858a93"), TEXT("1e2024")).TipF(0.22f), P(TEXT("8e8f98")).Gl(0.35f, 0.3f), Grey);
				L.Secondaries = G(P(TEXT("858a93"), TEXT("1c1d22")).TipF(0.25f)); // the two black bars
				L.GreaterCoverts = G(P(TEXT("8a8f98"), TEXT("1c1d22")).TipF(0.3f));
				L.Beak = C(TEXT("3a3536")); L.BeakBase = C(TEXT("e8e4e0")); L.Legs = C(TEXT("b8423a"));
				L.BeakCm = 1.8f; L.BeakDepth = 0.3f; L.BeakHook = 0.08f; L.Wing = EWing::Pointed; L.TailFrac = 0.36f;
				Add(TEXT("FeralPigeon"), L);
			}
			{
				const P Grey = P(TEXT("7f8792"));
				FLook L = Plain(P(TEXT("3a3c42"), TEXT("6c6f75")).Fr(0.06f), Grey, P(TEXT("7a828c"), TEXT("2a2b30")).TipF(0.28f), P(TEXT("b89a9a")), Grey);
				L.PrimaryCoverts = G(P(TEXT("f2f2f0"))); // the white band across the wrist
				L.Beak = C(TEXT("e8c060")); L.BeakBase = C(TEXT("d88a8a")); L.Legs = C(TEXT("c06a6a"));
				L.BeakCm = 1.9f; L.BeakDepth = 0.3f; L.BeakHook = 0.08f; L.Wing = EWing::Pointed; L.TailFrac = 0.4f;
				Add(TEXT("WoodPigeon"), L);
			}
			// --- gulls: grey mantle, black wing tips with white mirrors
			{
				const P Mantle = P(TEXT("b8c0c8"));
				FLook L = Plain(P(TEXT("b8c0c8"), TEXT("f4f4f2")).Fr(0.08f), Mantle, P(TEXT("f6f6f4")), P(TEXT("f8f8f6")), Mantle);
				L.Primaries = G(P(TEXT("b8c0c8"), TEXT("f4f4f2")).Fr(0.06f), 6, P(TEXT("b8c0c8"), TEXT("101012")).TipF(0.4f).Sp(0.12f));
				L.Secondaries = G(P(TEXT("b8c0c8"), TEXT("f6f6f4")).TipF(0.18f));
				L.Beak = C(TEXT("e8c23a")); L.BeakBase = C(TEXT("e8c23a")); L.Legs = C(TEXT("e0a8a0"));
				L.BeakCm = 5.5f; L.BeakDepth = 0.32f; L.BeakHook = 0.25f; L.Wing = EWing::Pointed; L.TailFrac = 0.3f;
				Add(TEXT("HerringGull"), L);
			}
			{
				const P Mantle = P(TEXT("c4cbd2"));
				FLook L = Plain(P(TEXT("c4cbd2"), TEXT("f6f6f4")).TipF(0.15f), Mantle, P(TEXT("f8f8f6")), P(TEXT("f8f8f6")), Mantle);
				L.Primaries = G(P(TEXT("f4f4f2"), TEXT("101012")).TipF(0.25f), 4, P(TEXT("f4f4f2"), TEXT("101012")).TipF(0.3f).Fr(0.1f));
				L.Beak = C(TEXT("6a1a1a")); L.BeakBase = C(TEXT("9a2a28")); L.Legs = C(TEXT("b83a32"));
				L.BeakCm = 3.3f; L.BeakDepth = 0.28f; L.BeakHook = 0.15f; L.Wing = EWing::Pointed; L.TailFrac = 0.3f;
				Add(TEXT("BlackHeadedGull"), L);
			}
			{
				const P Mantle = P(TEXT("a8b2bc"));
				FLook L = Plain(P(TEXT("a8b2bc"), TEXT("f6f6f4")).TipF(0.2f), Mantle, P(TEXT("f8f8f6")), P(TEXT("f8f8f6")), Mantle);
				L.Primaries = G(P(TEXT("a8b2bc"), TEXT("f6f6f4")), 6, P(TEXT("a8b2bc"), TEXT("101012")).TipF(0.45f).Sp(0.18f));
				L.Beak = C(TEXT("d8d070")); L.BeakBase = C(TEXT("c0c47a")); L.Legs = C(TEXT("c8c470"));
				L.BeakCm = 3.6f; L.BeakDepth = 0.28f; L.BeakHook = 0.15f; L.Wing = EWing::Pointed; L.TailFrac = 0.3f;
				Add(TEXT("CommonGull"), L);
			}
			// --- tits
			{
				FLook L = Plain(P(TEXT("4c5462"), TEXT("aab4c0")).Fr(0.1f), P(TEXT("56677a")), P(TEXT("4e5a6a")), P(TEXT("e2c435"), TEXT("101012")), P(TEXT("6b7a3a")));
				L.GreaterCoverts = G(P(TEXT("56677a"), TEXT("f2f2ec")).TipF(0.3f)); // the white wing bar
				L.Tail = G(P(TEXT("4e5a6a")), 1, P(TEXT("f0f0ea")));
				L.Beak = L.BeakBase = C(TEXT("101012")); L.Legs = C(TEXT("5a6270"));
				L.BeakCm = 1.1f; L.BeakDepth = 0.5f; L.TailFrac = 0.42f;
				Add(TEXT("GreatTit"), L);
			}
			{
				FLook L = Plain(P(TEXT("2f5a9c"), TEXT("6a8cc4")).Fr(0.08f), P(TEXT("3f6fb6")), P(TEXT("3f6fb6")), P(TEXT("e8d24a")), P(TEXT("8a9a4a")));
				L.GreaterCoverts = G(P(TEXT("3f6fb6"), TEXT("f4f4ee")).TipF(0.28f));
				L.Beak = L.BeakBase = C(TEXT("101012")); L.Legs = C(TEXT("5a6a88"));
				L.BeakCm = 0.8f; L.BeakDepth = 0.6f; L.TailFrac = 0.4f;
				Add(TEXT("BlueTit"), L);
			}
			// --- finches
			{
				FLook L = Plain(P(TEXT("141416"), TEXT("f2c81a")).RootF(0.42f).Sp(0.1f), P(TEXT("141416")), P(TEXT("141416")).Sp(0.15f), P(TEXT("e8dcc8")), P(TEXT("a07a52")));
				L.Secondaries = G(P(TEXT("141416"), TEXT("f2c81a")).RootF(0.5f).Sp(0.14f));
				L.GreaterCoverts = G(P(TEXT("f2c81a")));
				L.Beak = C(TEXT("e6d0c0")); L.BeakBase = C(TEXT("e6d0c0")); L.Legs = C(TEXT("d8b0a0"));
				L.BeakCm = 1.4f; L.BeakDepth = 0.62f; L.TailShape = ETail::Notch; L.TailFrac = 0.36f;
				Add(TEXT("Goldfinch"), L);
			}
			{
				FLook L = Plain(P(TEXT("1e1e20"), TEXT("d8d48a")).Fr(0.08f), P(TEXT("1e1e20"), TEXT("f2f2ec")).TipF(0.3f), P(TEXT("2a2a2c")), P(TEXT("c07d5a")), P(TEXT("7a5a3c")));
				L.LesserCoverts = G(P(TEXT("f2f2ec")));   // the white shoulder
				L.Tail = G(P(TEXT("2a2a2c")), 2, P(TEXT("f0f0ea")).RootF(0.f));
				L.Beak = L.BeakBase = C(TEXT("6a7a8c")); L.Legs = C(TEXT("8a6a5a"));
				L.BeakCm = 1.2f; L.BeakDepth = 0.55f; L.TailShape = ETail::Notch; L.TailFrac = 0.4f;
				Add(TEXT("Chaffinch"), L);
			}
			{
				const P Black = P(TEXT("0e0e12")).Gl(0.4f, 0.8f);
				FLook L = Plain(Black, Black, Black, P(TEXT("d6574a")), P(TEXT("7e7f88")));
				L.GreaterCoverts = G(P(TEXT("b8b8b8"), TEXT("0e0e12")).RootF(0.4f));
				L.Beak = L.BeakBase = C(TEXT("0e0e10")); L.Legs = C(TEXT("6a5a58"));
				L.BeakCm = 1.0f; L.BeakDepth = 0.75f; L.BeakHook = 0.2f; L.TailFrac = 0.4f;
				Add(TEXT("Bullfinch"), L);
			}
			// --- sparrows
			{
				FLook L = Plain(P(TEXT("5a4632"), TEXT("b8946a")).Fr(0.14f), P(TEXT("8a5a32"), TEXT("3a2a1a")).RootF(0.4f).Fr(0.1f), P(TEXT("6a5240"), TEXT("a08060")).Fr(0.08f), P(TEXT("a8a8a2")), P(TEXT("8a6a46"), TEXT("2a1e14")).Fr(0.2f));
				L.GreaterCoverts = G(P(TEXT("8a5a32"), TEXT("f0ece0")).TipF(0.25f));
				L.Beak = L.BeakBase = C(TEXT("3a3632")); L.Legs = C(TEXT("b0907a"));
				L.BeakCm = 1.2f; L.BeakDepth = 0.6f; L.TailShape = ETail::Notch; L.TailFrac = 0.38f;
				Add(TEXT("HouseSparrow"), L);
			}
			{
				FLook L = Plain(P(TEXT("5e4834"), TEXT("b8946a")).Fr(0.14f), P(TEXT("9a5e30"), TEXT("f0ece0")).TipF(0.25f), P(TEXT("6a5240")), P(TEXT("c8c0b0")), P(TEXT("8e6640"), TEXT("2a1e14")).Fr(0.2f));
				L.LesserCoverts = G(P(TEXT("9a5e30"), TEXT("f0ece0")).TipF(0.3f));
				L.Beak = L.BeakBase = C(TEXT("1a1816")); L.Legs = C(TEXT("b0907a"));
				L.BeakCm = 1.1f; L.BeakDepth = 0.6f; L.TailShape = ETail::Notch; L.TailFrac = 0.38f;
				Add(TEXT("TreeSparrow"), L);
			}
			// --- starling: black with a green and purple sheen and pale autumn spots
			{
				const P Body = P(TEXT("1a1c1e"), TEXT("c8ac78")).Sp(0.08f).Gl(0.8f, 0.4f);
				FLook L = Plain(P(TEXT("24262a"), TEXT("b89a6a")).Fr(0.1f).Gl(0.4f, 0.3f), Body, P(TEXT("26282c"), TEXT("b89a6a")).Fr(0.1f), Body, Body);
				L.Beak = L.BeakBase = C(TEXT("3a3530")); L.Legs = C(TEXT("b0706a"));
				L.BeakCm = 2.6f; L.BeakDepth = 0.32f; L.Wing = EWing::Pointed; L.TailFrac = 0.28f;
				Add(TEXT("Starling"), L);
			}
			// --- thrushes
			{
				FLook L = Plain(P(TEXT("8a6a48")), P(TEXT("8a6a48"), TEXT("d8b880")).Sp(0.1f), P(TEXT("8a6a48")), P(TEXT("e8dcc0"), TEXT("3a2a1a")).Sp(0.22f), P(TEXT("8a6a48")));
				L.Beak = C(TEXT("4a3a2a")); L.BeakBase = C(TEXT("c8a070")); L.Legs = C(TEXT("d8b0a0"));
				L.BeakCm = 1.8f; L.BeakDepth = 0.4f; L.TailFrac = 0.38f;
				Add(TEXT("SongThrush"), L);
			}
			{
				FLook L = Plain(P(TEXT("4a3426"), TEXT("8a6a50")).Fr(0.08f), P(TEXT("6a4a32")), P(TEXT("141414")), P(TEXT("d4a050"), TEXT("2a1e14")).Sp(0.22f), P(TEXT("7a4a2a")));
				L.Beak = C(TEXT("3a2a1a")); L.BeakBase = C(TEXT("e0b030")); L.Legs = C(TEXT("3a2a20"));
				L.BeakCm = 2.0f; L.BeakDepth = 0.4f; L.TailFrac = 0.4f;
				Add(TEXT("Fieldfare"), L);
			}
			{
				const P Black = P(TEXT("141414"), TEXT("222222"));
				FLook L = Plain(Black, Black, Black, Black, Black);
				L.Beak = L.BeakBase = C(TEXT("e8a020")); L.Legs = C(TEXT("3a2a20"));
				L.BeakCm = 2.2f; L.BeakDepth = 0.4f; L.TailFrac = 0.42f;
				Add(TEXT("Blackbird"), L);
			}
			{
				FLook L = Plain(P(TEXT("7a6a50"), TEXT("a08a68")).Fr(0.06f), P(TEXT("7a6a50"), TEXT("c8a878")).TipF(0.2f), P(TEXT("7a6a50")), P(TEXT("d8662e")), P(TEXT("7a6a50")));
				L.Beak = L.BeakBase = C(TEXT("1a1816")); L.Legs = C(TEXT("8a6a5a"));
				L.BeakCm = 1.2f; L.BeakDepth = 0.4f; L.TailFrac = 0.4f;
				Add(TEXT("Robin"), L);
			}
			{
				FLook L = Plain(P(TEXT("2a2a2c"), TEXT("e8e8e4")).Fr(0.14f), P(TEXT("2a2a2c"), TEXT("f0f0ec")).TipF(0.35f), P(TEXT("141414")), P(TEXT("f4f4f2")), P(TEXT("8a8d90")));
				L.Tail = G(P(TEXT("141414")), 2, P(TEXT("f4f4f2")));
				L.Beak = L.BeakBase = C(TEXT("101010")); L.Legs = C(TEXT("141414"));
				L.BeakCm = 1.3f; L.BeakDepth = 0.32f; L.TailShape = ETail::Long; L.TailFrac = 0.5f;
				Add(TEXT("WhiteWagtail"), L);
			}
			{
				FLook L = Plain(P(TEXT("2a2628"), TEXT("f0d040")).TipF(0.1f).Fr(0.06f), P(TEXT("6a6066")), P(TEXT("5a5458"), TEXT("f0d020")).TipF(0.16f), P(TEXT("b0907a")), P(TEXT("a08070")));
				L.Secondaries = G(P(TEXT("3a3438"), TEXT("d8281a")).TipF(0.1f).Sp(0.1f)); // the red wax tips
				L.Beak = L.BeakBase = C(TEXT("101010")); L.Legs = C(TEXT("141414"));
				L.BeakCm = 1.2f; L.BeakDepth = 0.5f; L.Wing = EWing::Pointed; L.TailFrac = 0.33f;
				Add(TEXT("Waxwing"), L);
			}
			// --- swift and the hirundines
			{
				const P Sooty = P(TEXT("2a2826"), TEXT("3c3936")).Fr(0.05f).Gl(0.15f, 0.2f);
				FLook L = Plain(Sooty, Sooty, Sooty, P(TEXT("2e2c2a")), Sooty);
				L.Beak = L.BeakBase = C(TEXT("101010")); L.Legs = C(TEXT("2a2420"));
				L.BeakCm = 0.6f; L.BeakDepth = 0.5f; L.BeakHook = 0.2f; L.Wing = EWing::Sickle; L.TailShape = ETail::Fork; L.TailFrac = 0.4f;
				Add(TEXT("CommonSwift"), L);
			}
			{
				const P Blue = P(TEXT("1a2a4a"), TEXT("0e1626")).Gl(0.7f, 0.75f);
				FLook L = Plain(P(TEXT("16203a")).Gl(0.5f, 0.75f), Blue, P(TEXT("16203a")).Gl(0.5f, 0.75f).Sp(0.12f), P(TEXT("eadccb")), Blue);
				L.Beak = L.BeakBase = C(TEXT("101010")); L.Legs = C(TEXT("141414"));
				L.BeakCm = 0.7f; L.BeakDepth = 0.4f; L.Wing = EWing::Sickle; L.TailShape = ETail::Streamers; L.TailFrac = 0.5f;
				Add(TEXT("BarnSwallow"), L);
			}
			{
				const P Brown = P(TEXT("6a5a48"));
				FLook L = Plain(P(TEXT("4e4234")), Brown, Brown, P(TEXT("f2f0ea")), Brown);
				L.Beak = L.BeakBase = C(TEXT("101010")); L.Legs = C(TEXT("3a3028"));
				L.BeakCm = 0.6f; L.BeakDepth = 0.4f; L.Wing = EWing::Sickle; L.TailShape = ETail::Fork; L.TailFrac = 0.34f;
				Add(TEXT("SandMartin"), L);
			}
			// --- cormorant and the raptors
			{
				FLook L = Plain(P(TEXT("1a1a18")).Gl(0.3f, 0.1f), P(TEXT("4a4030"), TEXT("101010")).Fr(0.2f).Gl(0.3f, 0.1f), P(TEXT("141412")), P(TEXT("1a1a18")).Gl(0.4f, 0.2f), P(TEXT("4a4030"), TEXT("101010")).Fr(0.2f));
				L.Beak = C(TEXT("5a5a52")); L.BeakBase = C(TEXT("d8c060")); L.Legs = C(TEXT("101010"));
				L.BeakCm = 7.f; L.BeakDepth = 0.2f; L.BeakHook = 0.35f; L.Wing = EWing::Broad; L.TailShape = ETail::Wedge; L.TailFrac = 0.22f;
				Add(TEXT("Cormorant"), L);
			}
			{
				FLook L = Plain(P(TEXT("5a6470"), TEXT("2a2e34")).Bar(5.f), P(TEXT("5a6470")), P(TEXT("6a7480"), TEXT("2a2e34")).Bar(4.f), P(TEXT("e8d8c8"), TEXT("c86a3a")).Bar(6.f), P(TEXT("5a6470")));
				L.Beak = C(TEXT("1a1a20")); L.BeakBase = C(TEXT("e8c840")); L.Legs = C(TEXT("e8c840"));
				L.BeakCm = 1.5f; L.BeakDepth = 0.6f; L.BeakHook = 0.9f; L.Wing = EWing::Round; L.TailShape = ETail::Square; L.TailFrac = 0.48f;
				Add(TEXT("Sparrowhawk"), L);
			}
			{
				FLook L = Plain(P(TEXT("3a404a"), TEXT("20242a")).Bar(6.f), P(TEXT("4a5260"), TEXT("2a2e36")).Bar(3.f), P(TEXT("4a5260"), TEXT("20242a")).Bar(6.f), P(TEXT("f0ece4"), TEXT("2a2e36")).Bar(7.f), P(TEXT("4a5260"), TEXT("2a2e36")).Bar(2.f));
				L.Beak = C(TEXT("2a2e36")); L.BeakBase = C(TEXT("e8c840")); L.Legs = C(TEXT("e8c840"));
				L.BeakCm = 2.f; L.BeakDepth = 0.6f; L.BeakHook = 1.f; L.Wing = EWing::Pointed; L.TailShape = ETail::Square; L.TailFrac = 0.35f;
				Add(TEXT("Peregrine"), L);
			}
			return M;
		}
	}

	const FLook& Get(FName SpeciesId)
	{
		static const TMap<FName, FLook> Looks = Build();
		static const FLook Fallback = Plain(P(TEXT("5a4632"), TEXT("b8946a")).Fr(0.1f), P(TEXT("7a5a3a")), P(TEXT("6a5240")), P(TEXT("b8b0a0")), P(TEXT("7a5a3a")));
		const FLook* L = Looks.Find(SpeciesId);
		return L ? *L : Fallback;
	}
}
