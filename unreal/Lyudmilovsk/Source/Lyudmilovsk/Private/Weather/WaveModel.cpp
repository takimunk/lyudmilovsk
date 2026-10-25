#include "WaveModel.h"

#include <algorithm>
#include <cmath>

namespace LyudWeather
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;
		constexpr double G = 9.81;

		double Clamp01(double V) { return std::min(std::max(V, 0.0), 1.0); }

		double HashPhase(uint32_t I, uint32_t Seed)
		{
			uint32_t H = I * 0x9e3779b1u ^ Seed * 0x85ebca6bu;
			H = (H ^ (H >> 16)) * 0x7feb352du;
			H = (H ^ (H >> 15)) * 0x846ca68bu;
			H ^= H >> 16;
			return double(H >> 8) / 16777216.0;
		}

		/** Fills a set: log-spaced wavenumbers, golden-angle directions, hashed phases. */
		void Lattice(std::vector<FWaveTrain>& Out, int N, double LambdaMin, double LambdaMax, double Loop, uint32_t Seed, double Turn)
		{
			Out.assign(std::max(N, 0), FWaveTrain());
			const double LnKMin = std::log(2.0 * Pi / LambdaMax), LnKMax = std::log(2.0 * Pi / LambdaMin);
			const double W0 = 2.0 * Pi / Loop;
			for (int I = 0; I < N; I++)
			{
				FWaveTrain& T = Out[I];
				const double Jit = HashPhase(I, Seed + 1) - 0.5;
				const double K = std::exp(LnKMin + (LnKMax - LnKMin) * (I + 0.5 + 0.6 * Jit) / N);
				const double A = 2.0 * Pi * std::fmod(I * 0.6180339887498949 + Turn, 1.0);
				T.DirX = float(std::cos(A));
				T.DirY = float(std::sin(A));
				T.K = float(K);
				T.Lambda = float(2.0 * Pi / K);
				T.Omega = float(std::max(1.0, std::round(DispersionOmega(K) / W0)) * W0);
				T.Phase = float(2.0 * Pi * HashPhase(I, Seed));
			}
		}

		/** JONSWAP wavenumber spectrum S(k) for a peak omega and alpha, m^3. */
		double SpectrumK(double K, double Wp, double Alpha, double Gamma)
		{
			const double W = DispersionOmega(K);
			const double Sigma = W <= Wp ? 0.07 : 0.09;
			const double R = std::exp(-(W - Wp) * (W - Wp) / (2.0 * Sigma * Sigma * Wp * Wp));
			const double Sw = Alpha * G * G / std::pow(W, 5.0) * std::exp(-1.25 * std::pow(Wp / W, 4.0)) * std::pow(Gamma, R);
			// dω/dk from the dispersion relation (numerically: it includes surface tension)
			const double Dk = K * 1e-3;
			const double DwDk = (DispersionOmega(K + Dk) - DispersionOmega(K - Dk)) / (2.0 * Dk);
			return Sw * DwDk;
		}

		void Amplitudes(std::vector<FWaveTrain>& Set, double LambdaMin, double LambdaMax, double Wp, double Alpha, double Gamma,
			double Spread, double DirX, double DirY)
		{
			if (Set.empty())
			{
				return;
			}
			// each train stands for an equal cell of (log k, angle): a^2/2 = S(k) k dlnk dθ D(θ)
			const double Cell = (std::log(LambdaMax) - std::log(LambdaMin)) * 2.0 * Pi / Set.size();
			const double Norm = std::tgamma(Spread + 1.0) / (2.0 * std::sqrt(Pi) * std::tgamma(Spread + 0.5));
			for (FWaveTrain& T : Set)
			{
				const double CosA = std::min(std::max(T.DirX * DirX + T.DirY * DirY, -1.0), 1.0);
				// cos^2s of half the angle = ((1 + cos) / 2)^s
				const double D = Norm * std::pow((1.0 + CosA) * 0.5, Spread);
				const double E = SpectrumK(T.K, Wp, Alpha, Gamma) * T.K * Cell * D;
				T.Amp = float(std::sqrt(std::max(2.0 * E, 0.0)));
			}
		}
	}

	double DispersionOmega(double K)
	{
		return std::sqrt(G * K + 7.4e-5 * K * K * K);
	}

	void FWaveField::Init(const FWaveParams& P)
	{
		Params = P;
		Lattice(Geo, P.GeoCount, P.GeoLambdaMin, P.GeoLambdaMax, P.LoopSeconds, P.Seed, 0.13);
		Lattice(Detail, P.DetailCount, P.DetailLambdaMin, P.DetailLambdaMax, P.LoopSeconds, P.Seed + 101, 0.71);
		// longest first
		auto ByK = [](const FWaveTrain& A, const FWaveTrain& B) { return A.K < B.K; };
		std::sort(Geo.begin(), Geo.end(), ByK);
		std::sort(Detail.begin(), Detail.end(), ByK);
		SetWind(0.f, 1.f, 0.f);
	}

	void FWaveField::SetWind(float Speed, float DirX, float DirY)
	{
		WindSpeed = Speed;
		WindDirX = DirX;
		WindDirY = DirY;
		const double U = std::max(double(Speed), 0.4);
		const double F = Params.FetchKm * 1000.0;
		const double Chi = G * F / (U * U);
		// fetch-limited peak, never below the fully developed (Pierson-Moskowitz) one
		double Wp = 22.0 * std::cbrt(G * G / (U * F));
		double Alpha = 0.076 * std::pow(Chi, -0.22);
		double Gamma = 3.3;
		const double WpPM = 0.855 * G / U;
		if (Wp < WpPM)
		{
			Wp = WpPM;
			Alpha = 0.0081;
			Gamma = 1.0;
		}
		Alpha = std::max(Alpha, 0.0081);
		PeakLambda = float(2.0 * Pi * G / (Wp * Wp));
		const double Dn = std::sqrt(double(DirX) * DirX + double(DirY) * DirY);
		const double DX = Dn > 1e-6 ? DirX / Dn : 1.0, DY = Dn > 1e-6 ? DirY / Dn : 0.0;
		Amplitudes(Geo, Params.GeoLambdaMin, Params.GeoLambdaMax, Wp, Alpha, Gamma, Params.Spread, DX, DY);
		// the ripples answer the wind faster and wider than the swell: broader spread
		Amplitudes(Detail, Params.DetailLambdaMin, Params.DetailLambdaMax, Wp, Alpha, Gamma, std::max(1.0, Params.Spread * 0.5), DX, DY);
		// below ~1 m/s the water goes glassy: no new ripples
		const double Ripple = Clamp01((U - 0.7) / 1.8);
		for (FWaveTrain& T : Detail)
		{
			T.Amp *= float(Ripple);
		}
		double Var = 0.0, Slope = 0.0;
		for (const FWaveTrain& T : Geo)
		{
			Var += 0.5 * T.Amp * T.Amp;
			Slope += T.K * T.Amp;
		}
		Hs = float(4.0 * std::sqrt(Var));
		Chop = float(Slope > 1e-6 ? std::min(Params.Choppiness, 0.85 / Slope) : Params.Choppiness);
	}

	float FWaveField::DepthAtten(float K, float DepthM)
	{
		return float(std::sqrt(Clamp01(K * DepthM * 0.5)));
	}

	float FWaveField::BreakCap(float DepthM) const
	{
		return float(Clamp01(0.4 * std::max(DepthM, 0.f) / std::max(0.5 * Hs, 1e-3)));
	}

	double FWaveField::Wrap(double Seconds) const
	{
		const double L = Params.LoopSeconds;
		const double W = std::fmod(Seconds, L);
		return W < 0.0 ? W + L : W;
	}

	void FWaveField::Displace(double X0, double Y0, double T, float DepthM, float Out[3]) const
	{
		const double Cap = BreakCap(DepthM);
		double DX = 0.0, DY = 0.0, DZ = 0.0;
		for (const FWaveTrain& W : Geo)
		{
			const double A = W.Amp * DepthAtten(W.K, DepthM) * Cap;
			if (A <= 0.0)
			{
				continue;
			}
			const double Ph = W.K * (W.DirX * X0 + W.DirY * Y0) - W.Omega * T + W.Phase;
			const double S = std::sin(Ph), C = std::cos(Ph);
			DX -= Chop * A * W.DirX * S;
			DY -= Chop * A * W.DirY * S;
			DZ += A * C;
		}
		Out[0] = float(DX);
		Out[1] = float(DY);
		Out[2] = float(DZ);
	}

	FSeaSurface FWaveField::SurfaceAt(double X, double Y, double T, float DepthM) const
	{
		// find the particle that sits over (X, Y): undo the horizontal displacement a few times
		double X0 = X, Y0 = Y;
		float D[3];
		for (int I = 0; I < 5; I++)
		{
			Displace(X0, Y0, T, DepthM, D);
			X0 = X - D[0];
			Y0 = Y - D[1];
		}
		const double Cap = BreakCap(DepthM);
		double H = 0.0, VX = 0.0, VY = 0.0, VZ = 0.0;
		double Txx = 1.0, Txy = 0.0, Txz = 0.0, Tyy = 1.0, Tyz = 0.0;
		for (const FWaveTrain& W : Geo)
		{
			const double A = W.Amp * DepthAtten(W.K, DepthM) * Cap;
			if (A <= 0.0)
			{
				continue;
			}
			const double Ph = W.K * (W.DirX * X0 + W.DirY * Y0) - W.Omega * T + W.Phase;
			const double S = std::sin(Ph), C = std::cos(Ph);
			H += A * C;
			VX += Chop * A * W.Omega * W.DirX * C;
			VY += Chop * A * W.Omega * W.DirY * C;
			VZ += A * W.Omega * S;
			const double KA = W.K * A;
			// d(position)/d(x0) and d/d(y0)
			Txx -= Chop * KA * W.DirX * W.DirX * C;
			Txy -= Chop * KA * W.DirX * W.DirY * C;
			Tyy -= Chop * KA * W.DirY * W.DirY * C;
			Txz -= KA * W.DirX * S;
			Tyz -= KA * W.DirY * S;
		}
		// normal = d/dx0 x d/dy0, with d/dx0 = (Txx, Txy, Txz) and d/dy0 = (Txy, Tyy, Tyz)
		double NX = Txy * Tyz - Txz * Tyy;
		double NY = Txz * Txy - Txx * Tyz;
		double NZ = Txx * Tyy - Txy * Txy;
		const double L = std::sqrt(NX * NX + NY * NY + NZ * NZ);
		FSeaSurface S;
		S.Height = float(H);
		if (L > 1e-9)
		{
			S.Normal[0] = float(NX / L);
			S.Normal[1] = float(NY / L);
			S.Normal[2] = float(NZ / L);
		}
		S.Velocity[0] = float(VX);
		S.Velocity[1] = float(VY);
		S.Velocity[2] = float(VZ);
		return S;
	}

	float FWaveField::Jacobian(double X0, double Y0, double T) const
	{
		double Txx = 1.0, Txy = 0.0, Tyy = 1.0;
		for (const FWaveTrain& W : Geo)
		{
			const double Ph = W.K * (W.DirX * X0 + W.DirY * Y0) - W.Omega * T + W.Phase;
			const double C = std::cos(Ph) * Chop * W.K * W.Amp;
			Txx -= C * W.DirX * W.DirX;
			Txy -= C * W.DirX * W.DirY;
			Tyy -= C * W.DirY * W.DirY;
		}
		return float(Txx * Tyy - Txy * Txy);
	}
}
