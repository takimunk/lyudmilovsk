// The street's things in the public spaces' kit (docs/street_life.md): the kvass barrel and the ice-cream cart of a hot
// day, the yards' washing lines and what hangs on them, the domino table, the whitewashed bands round the trees' trunks
// and the autumn's leaf piles. Built like the rest of the kit (CityKit.h: metres, front +X, along +Y, up +Z, the origin on
// the ground, colours in the vertex colour); placed and shown by ULyudStreetLife (Content/City/street_props.json).
#include "CityKit.h"

namespace CityKit
{
	namespace
	{
		// the stall's frame (tools/town/street_life.py stall()): the vendor stands 1.45 m out along +X from the middle, the
		// counter point (the mugs' table, the cart) is 1.9 m out, the first in the line 2.35 m out
		constexpr float CounterX = 1.9f;

		void KvassBarrel(FMesh& M)
		{
			const FSkin Tank = Paint(226, 184, 40, 0.45f), Band = Paint(150, 36, 28, 0.5f), Fe = Iron(),
				Tyre = Paint(30, 30, 32, 0.9f), Wood = Timber(), Glass = Paint(200, 140, 60, 0.15f);
			// the tank on its axle: a yellow cylinder along X, a red band round its middle, domed ends
			const float R = 0.55f, Z = 1.0f, L = 1.05f;
			M.Tube(FVector(-L, 0, Z), FVector(L, 0, Z), R, R, 20, Tank, true, true);
			M.Tube(FVector(-0.12f, 0, Z), FVector(0.12f, 0, Z), R + 0.012f, R + 0.012f, 20, Band, false, false);
			for (float S : { -1.f, 1.f })
			{
				M.Tube(FVector(S * L, 0, Z), FVector(S * (L + 0.08f), 0, Z), R, R * 0.82f, 20, Tank, false, true);
				M.Tube(FVector(S * (L - 0.25f), 0, Z), FVector(S * (L - 0.2f), 0, Z), R + 0.015f, R + 0.015f, 20, Fe, false, false);
			}
			// the hatch on top, the tap at the front end
			M.Tube(FVector(0.35f, 0, Z + R - 0.02f), FVector(0.35f, 0, Z + R + 0.08f), 0.16f, 0.16f, 12, Tank, false, true);
			M.Bar(FVector(L + 0.08f, 0, Z - 0.25f), FVector(L + 0.24f, 0, Z - 0.25f), 0.04f, 0.04f, Fe);
			M.Bar(FVector(L + 0.24f, 0, Z - 0.25f), FVector(L + 0.24f, 0, Z - 0.36f), 0.03f, 0.03f, Fe);
			// the frame, the two wheels and the tow bar resting on its leg
			M.Bar(FVector(-L, -0.4f, Z - R - 0.02f), FVector(L, -0.4f, Z - R - 0.02f), 0.06f, 0.06f, Fe);
			M.Bar(FVector(-L, 0.4f, Z - R - 0.02f), FVector(L, 0.4f, Z - R - 0.02f), 0.06f, 0.06f, Fe);
			for (float Y : { -0.66f, 0.66f })
			{
				M.Tube(FVector(0, Y - 0.07f, 0.42f), FVector(0, Y + 0.07f, 0.42f), 0.42f, 0.42f, 16, Tyre, true, true);
				M.Tube(FVector(0, Y - 0.08f, 0.42f), FVector(0, Y + 0.08f, 0.42f), 0.16f, 0.16f, 10, Fe, true, true);
			}
			M.Bar(FVector(-L, -0.4f, Z - R), FVector(-L - 0.9f, 0, 0.55f), 0.06f, 0.06f, Fe);
			M.Bar(FVector(-L, 0.4f, Z - R), FVector(-L - 0.9f, 0, 0.55f), 0.06f, 0.06f, Fe);
			M.Bar(FVector(-L - 0.9f, 0, 0.55f), FVector(-L - 0.9f, 0, 0.f), 0.05f, 0.05f, Fe);
			// the mugs' table in front of the tap, a tray of heavy glass mugs, the rinser
			const float Tx = CounterX;
			M.Box(FVector(Tx, 0, 0.82f), FVector(0.26f, 0.4f, 0.02f), Wood, FRotator::ZeroRotator, true);
			for (float X : { -0.2f, 0.2f })
				for (float Y : { -0.34f, 0.34f })
					M.Bar(FVector(Tx + X, Y, 0.f), FVector(Tx + X, Y, 0.8f), 0.035f, 0.035f, Fe);
			for (int32 I = 0; I < 6; I++)
			{
				const FVector P(Tx - 0.1f + (I % 2) * 0.13f, -0.3f + (I / 2) * 0.12f, 0.84f);
				M.Tube(P, P + FVector(0, 0, 0.13f), 0.045f, 0.05f, 8, Glass, true, false);
			}
			M.Tube(FVector(Tx + 0.05f, 0.25f, 0.84f), FVector(Tx + 0.05f, 0.25f, 0.92f), 0.09f, 0.09f, 10, Fe, true, false);
			M.Collide(FVector(0, 0, 0.8f), FVector(L + 0.1f, 0.75f, 0.8f));
			M.Collide(FVector(Tx, 0, 0.43f), FVector(0.27f, 0.41f, 0.43f));
		}

		void IcecreamCart(FMesh& M)
		{
			const FSkin Body = Paint(232, 236, 240, 0.4f), Stripe = Paint(52, 110, 178, 0.45f), Fe = Iron(),
				Tyre = Paint(30, 30, 32, 0.9f), Lid = Paint(190, 200, 210, 0.3f), Red = Paint(196, 48, 40, 0.6f);
			// the chest across the line's head (its long side along Y), on four small wheels
			const FVector C(CounterX, 0, 0.62f);
			M.Box(C, FVector(0.28f, 0.6f, 0.32f), Body, FRotator::ZeroRotator, true);
			M.Box(C + FVector(0.282f, 0, 0.08f), FVector(0.005f, 0.6f, 0.06f), Stripe, FRotator::ZeroRotator, false);
			M.Box(C + FVector(0.282f, 0, -0.14f), FVector(0.005f, 0.6f, 0.025f), Stripe, FRotator::ZeroRotator, false);
			for (float Y : { -0.3f, 0.3f })
				M.Box(C + FVector(0, Y, 0.34f), FVector(0.27f, 0.28f, 0.02f), Lid, FRotator::ZeroRotator, true);
			for (float X : { -0.2f, 0.2f })
				for (float Y : { -0.62f, 0.62f })
					M.Tube(FVector(CounterX + X, Y - 0.03f * FMath::Sign(Y), 0.12f), FVector(CounterX + X, Y + 0.03f * FMath::Sign(Y), 0.12f),
						0.12f, 0.12f, 12, Tyre, true, true);
			// the push handle on the vendor's side
			M.Bar(FVector(CounterX - 0.28f, -0.5f, 0.8f), FVector(CounterX - 0.55f, -0.5f, 0.95f), 0.03f, 0.03f, Fe);
			M.Bar(FVector(CounterX - 0.28f, 0.5f, 0.8f), FVector(CounterX - 0.55f, 0.5f, 0.95f), 0.03f, 0.03f, Fe);
			M.Bar(FVector(CounterX - 0.55f, -0.5f, 0.95f), FVector(CounterX - 0.55f, 0.5f, 0.95f), 0.035f, 0.035f, Fe);
			// the parasol: a pole and a striped cone of eight panels
			const FVector Top(CounterX - 0.1f, 0.45f, 2.25f);
			M.Bar(FVector(CounterX - 0.1f, 0.45f, 0.94f), Top, 0.035f, 0.035f, Fe);
			for (int32 I = 0; I < 8; I++)
			{
				const float A0 = 2.f * PI * I / 8, A1 = 2.f * PI * (I + 1) / 8, Rr = 0.95f;
				const FVector P0 = Top + FVector(FMath::Cos(A0) * Rr, FMath::Sin(A0) * Rr, -0.35f);
				const FVector P1 = Top + FVector(FMath::Cos(A1) * Rr, FMath::Sin(A1) * Rr, -0.35f);
				const FSkin& S = (I % 2) ? Body : Stripe;
				const int32 Ia = M.Vert(Top + FVector(0, 0, 0.08f), FVector::UpVector, S), Ib = M.Vert(P0, FVector::UpVector, S), Ic = M.Vert(P1, FVector::UpVector, S);
				M.Tri(Ia, Ib, Ic, FVector::UpVector);
				const int32 Ja = M.Vert(Top + FVector(0, 0, 0.07f), -FVector::UpVector, S), Jb = M.Vert(P0, -FVector::UpVector, S), Jc = M.Vert(P1, -FVector::UpVector, S);
				M.Tri(Ja, Jb, Jc, -FVector::UpVector);
			}
			// a sign board on the front: a red disc on a little post
			M.Disc(FVector(CounterX + 0.29f, 0.f, 1.12f), 0.f, 0.14f, 14, Red, -1.f);
			M.Collide(C, FVector(0.3f, 0.66f, 0.62f));
		}

		/** The washing line: two T posts 5 m apart along X, two wires between their arms. */
		void LaundryPosts(FMesh& M)
		{
			const FSkin Fe = Paint(92, 112, 96, 0.6f), Wire = Paint(170, 170, 165, 0.4f);
			for (float X : { -2.5f, 2.5f })
			{
				M.Bar(FVector(X, 0, 0.f), FVector(X, 0, 2.05f), 0.06f, 0.06f, Fe);
				M.Bar(FVector(X, -0.38f, 2.0f), FVector(X, 0.38f, 2.0f), 0.05f, 0.05f, Fe);
			}
			for (float Y : { -0.3f, 0.3f }) M.Bar(FVector(-2.5f, Y, 2.0f), FVector(2.5f, Y, 1.96f), 0.008f, 0.008f, Wire);
			M.Collide(FVector(-2.5f, 0, 1.f), FVector(0.06f, 0.06f, 1.f));
			M.Collide(FVector(2.5f, 0, 1.f), FVector(0.06f, 0.06f, 1.f));
		}

		/** What hangs on the line today: sheets, shirts, towels, a dress, socks with their pegs (Variant: three washes). */
		void LaundryWashing(FMesh& M, int32 V)
		{
			static const uint8 Cols[8][3] = { { 240, 240, 236 }, { 236, 238, 244 }, { 118, 156, 206 }, { 206, 70, 64 },
											  { 236, 196, 88 }, { 120, 170, 120 }, { 214, 140, 170 }, { 160, 120, 90 } };
			struct FPiece { float W, H; int32 Col; };
			static const FPiece Sets[3][7] = {
				{ { 1.4f, 1.1f, 0 }, { 0.5f, 0.7f, 2 }, { 0.5f, 0.7f, 2 }, { 0.6f, 0.45f, 4 }, { 1.2f, 1.0f, 1 }, { 0.2f, 0.3f, 3 }, { 0.2f, 0.3f, 3 } },
				{ { 0.6f, 0.9f, 6 }, { 1.3f, 1.05f, 0 }, { 0.45f, 0.4f, 5 }, { 0.45f, 0.4f, 4 }, { 0.5f, 0.65f, 7 }, { 0.5f, 0.65f, 2 }, { 0.2f, 0.25f, 0 } },
				{ { 1.5f, 1.1f, 1 }, { 0.55f, 0.5f, 3 }, { 0.55f, 0.5f, 3 }, { 1.2f, 0.95f, 0 }, { 0.4f, 0.6f, 5 }, { 0.2f, 0.3f, 7 }, { 0.6f, 0.8f, 6 } } };
			const int32 K = ((V % 3) + 3) % 3;
			const FSkin Peg = Paint(196, 160, 110, 0.7f);
			// the front wire the long things, the back wire the small ones, side by side from one post to the other
			float X[2] = { -2.25f, -2.25f };
			for (int32 I = 0; I < 7; I++)
			{
				const FPiece& P = Sets[K][I];
				const int32 Wi = P.H > 0.8f || P.W > 1.0f ? 0 : 1;
				const float Y = Wi == 0 ? -0.3f : 0.3f;
				if (X[Wi] + P.W > 2.3f) continue;
				const FSkin S = Paint(Cols[P.Col][0], Cols[P.Col][1], Cols[P.Col][2], 0.85f);
				const float Cx = X[Wi] + P.W * 0.5f, Sag = 0.03f * FMath::Sin(PI * (Cx + 2.5f) / 5.f);
				// a slight sway of each piece, so the line doesn't read as one board
				M.Box(FVector(Cx, Y, 1.98f - Sag - P.H * 0.5f), FVector(P.W * 0.5f, 0.006f, P.H * 0.5f), S, FRotator(0.f, 0.f, (I % 3 - 1) * 4.f), true);
				for (float Px : { X[Wi] + 0.04f, X[Wi] + P.W - 0.04f }) M.Box(FVector(Px, Y, 1.99f - Sag), FVector(0.01f, 0.012f, 0.035f), Peg);
				X[Wi] += P.W + 0.12f;
			}
		}

		/** A square plywood table on one post and four stools round it at the corners (45, 135, 225, 315 degrees). */
		void DominoTable(FMesh& M)
		{
			const FSkin Top = Paint(184, 150, 104, 0.7f), Fe = Paint(64, 104, 72, 0.55f), Seat = Paint(150, 96, 60, 0.65f),
				Tile = Paint(236, 232, 220, 0.4f), Dot = Paint(30, 30, 30, 0.6f);
			M.Box(FVector(0, 0, 0.74f), FVector(0.45f, 0.45f, 0.02f), Top, FRotator::ZeroRotator, true);
			M.Bar(FVector(0, 0, 0.f), FVector(0, 0, 0.72f), 0.08f, 0.08f, Fe);
			M.Box(FVector(0, 0, 0.02f), FVector(0.3f, 0.3f, 0.02f), Fe);
			// a few tiles left on the table: a crooked chain
			for (int32 I = 0; I < 5; I++)
			{
				const FVector P(-0.2f + I * 0.09f, 0.02f * (I % 2), 0.765f);
				M.Box(P, FVector(0.04f, 0.02f, 0.006f), Tile, FRotator(0.f, (I % 2) * 90.f, 0.f), false);
				M.Box(P + FVector(0, 0, 0.0065f), FVector(0.006f, 0.006f, 0.001f), Dot, FRotator::ZeroRotator, false);
			}
			for (int32 K = 0; K < 4; K++)
			{
				const float A = FMath::DegreesToRadians(45.f + 90.f * K);
				const FVector C(FMath::Cos(A) * 0.75f, FMath::Sin(A) * 0.75f, 0.f);
				M.Tube(C + FVector(0, 0, 0.42f), C + FVector(0, 0, 0.45f), 0.17f, 0.17f, 12, Seat, false, true);
				M.Bar(C, C + FVector(0, 0, 0.42f), 0.05f, 0.05f, Fe);
			}
			M.Collide(FVector(0, 0, 0.38f), FVector(0.46f, 0.46f, 0.38f));
		}

		/** The lime band of a whitewashed trunk: a white sleeve from the ground to the knee. */
		void TrunkWhite(FMesh& M)
		{
			const FSkin Lime = Paint(238, 238, 232, 0.95f);
			M.Tube(FVector(0, 0, 0.f), FVector(0, 0, 1.0f), 0.16f, 0.135f, 12, Lime, false, false);
		}

		/** A raked pile of leaves: a low lumpy mound in the colours of the season. */
		void LeafPile(FMesh& M, int32 V)
		{
			static const uint8 Cols[4][3] = { { 190, 104, 34 }, { 156, 66, 30 }, { 206, 160, 52 }, { 120, 84, 44 } };
			TArray<FVector2D> Pr;
			Pr.Add(FVector2D(0.95f, 0.f));
			Pr.Add(FVector2D(0.82f, 0.12f));
			Pr.Add(FVector2D(0.62f, 0.28f));
			Pr.Add(FVector2D(0.36f, 0.4f));
			Pr.Add(FVector2D(0.f, 0.46f));
			M.Lathe(FVector::ZeroVector, Pr, 14, [V](int32 Span, int32 Side)
			{
				const int32 K = (Span * 7 + Side * 3 + V) % 4;
				return Paint(Cols[K][0], Cols[K][1], Cols[K][2], 0.9f);
			});
			// a few leaves strayed onto the ground round it
			for (int32 I = 0; I < 7; I++)
			{
				const float A = I * 2.39996f, R = 1.05f + 0.25f * (I % 3);
				const uint8* C = Cols[(I + V) % 4];
				M.Box(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.006f), FVector(0.05f, 0.035f, 0.004f), Paint(C[0], C[1], C[2], 0.9f),
					FRotator(0.f, I * 47.f, 0.f), false);
			}
		}
	}

	bool BuildStreetProp(FMesh& M, const FString& K, int32 V)
	{
		if (K == TEXT("kvass_barrel")) KvassBarrel(M);
		else if (K == TEXT("icecream_cart")) IcecreamCart(M);
		else if (K == TEXT("laundry_line_posts")) LaundryPosts(M);
		else if (K == TEXT("laundry_washing")) LaundryWashing(M, V);
		else if (K == TEXT("domino_table")) DominoTable(M);
		else if (K == TEXT("trunk_white")) TrunkWhite(M);
		else if (K == TEXT("leaf_pile")) LeafPile(M, V);
		else return false;
		return true;
	}

	bool StreetPropCollides(const FString& K)
	{
		return K == TEXT("kvass_barrel") || K == TEXT("icecream_cart") || K == TEXT("laundry_line_posts") || K == TEXT("domino_table");
	}
}
