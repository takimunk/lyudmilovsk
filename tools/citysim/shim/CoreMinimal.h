// A tiny stand-in for the few Unreal core types the town's pure-math code uses (CityKit's mesh builders, the window
// schedules), so they compile with plain clang++ for tools/citysim. Only what that code calls; same conventions as
// Unreal (FRotator pitch/yaw/roll in degrees, row-vector rotation matrices, left-handed Z up).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using int32 = int32_t;
using uint32 = uint32_t;
using uint8 = uint8_t;
using TCHAR = char;
#define TEXT(x) x
#ifndef PI
#define PI (3.1415926535897932f)
#endif
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

template <typename T> void Swap(T& A, T& B) { std::swap(A, B); }

template <typename T>
struct TArray
{
	std::vector<T> V;
	TArray() = default;
	TArray(std::initializer_list<T> L) : V(L) {}
	int32 Add(const T& X) { V.push_back(X); return (int32)V.size() - 1; }
	void Append(std::initializer_list<T> L) { V.insert(V.end(), L.begin(), L.end()); }
	void Reserve(int32 N) { V.reserve(N); }
	int32 Num() const { return (int32)V.size(); }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	T& operator[](int32 I) { return V[I]; }
	const T& operator[](int32 I) const { return V[I]; }
	typename std::vector<T>::iterator begin() { return V.begin(); }
	typename std::vector<T>::iterator end() { return V.end(); }
	typename std::vector<T>::const_iterator begin() const { return V.begin(); }
	typename std::vector<T>::const_iterator end() const { return V.end(); }
};

template <typename F> using TFunctionRef = std::function<F>;

struct FString
{
	std::string S;
	FString() = default;
	FString(const char* C) : S(C) {}
	FString(std::string C) : S(std::move(C)) {}
	bool operator==(const char* C) const { return S == C; }
	bool operator==(const FString& O) const { return S == O.S; }
	bool IsEmpty() const { return S.empty(); }
};

struct FName { std::string S; };
class UObject;

template <typename T>
struct TSharedPtr : std::shared_ptr<T>
{
	using std::shared_ptr<T>::shared_ptr;
	TSharedPtr(std::shared_ptr<T> P) : std::shared_ptr<T>(std::move(P)) {}
	bool IsValid() const { return this->get() != nullptr; }
};

class FJsonValue
{
public:
	double N = 0.0;
	explicit FJsonValue(double In) : N(In) {}
	double AsNumber() const { return N; }
};

class FJsonObject
{
public:
	std::map<std::string, double> Numbers;
	std::map<std::string, TArray<TSharedPtr<FJsonValue>>> Arrays;
	bool HasField(const char* K) const { return Numbers.count(K) || Arrays.count(K); }
	double GetNumberField(const char* K) const { auto It = Numbers.find(K); return It == Numbers.end() ? 0.0 : It->second; }
	bool TryGetArrayField(const char* K, const TArray<TSharedPtr<FJsonValue>>*& Out) const
	{
		auto It = Arrays.find(K);
		if (It == Arrays.end()) return false;
		Out = &It->second;
		return true;
	}
};

struct FMath
{
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Clamp(T X, T A, T B) { return X < A ? A : (X > B ? B : X); }
	template <typename T> static T Abs(T X) { return X < 0 ? -X : X; }
	template <typename T> static T Square(T X) { return X * X; }
	template <typename T, typename U> static T Lerp(const T& A, const T& B, U Alpha) { return A + (B - A) * Alpha; }
	static float Cos(float X) { return std::cos(X); }
	static float Sin(float X) { return std::sin(X); }
	static float Sqrt(float X) { return std::sqrt(X); }
	static float Atan2(float Y, float X) { return std::atan2(Y, X); }
	static float Fmod(float X, float Y) { return std::fmod(X, Y); }
	static int32 RoundToInt(float X) { return (int32)std::floor(X + 0.5f); }
	static float RadiansToDegrees(float X) { return X * (180.f / PI); }
	static float DegreesToRadians(float X) { return X * (PI / 180.f); }
};

struct FVector3f;

struct FVector
{
	double X = 0, Y = 0, Z = 0;
	FVector() = default;
	FVector(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FVector(double S) : X(S), Y(S), Z(S) {}
	explicit FVector(const FVector3f& V);
	FVector operator+(const FVector& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
	FVector operator-(const FVector& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
	FVector operator-() const { return { -X, -Y, -Z }; }
	FVector operator*(double S) const { return { X * S, Y * S, Z * S }; }
	FVector operator/(double S) const { return { X / S, Y / S, Z / S }; }
	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	double SizeSquared() const { return X * X + Y * Y + Z * Z; }
	FVector GetSafeNormal() const { const double L = Size(); return L < 1e-8 ? FVector() : *this / L; }
	void Normalize() { const double L = Size(); if (L > 1e-8) { X /= L; Y /= L; Z /= L; } }
	static FVector CrossProduct(const FVector& A, const FVector& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static double DistSquared(const FVector& A, const FVector& B) { return (A - B).SizeSquared(); }
	static const FVector ZeroVector, UpVector, ForwardVector, RightVector;
};
inline FVector operator*(double S, const FVector& V) { return V * S; }
inline const FVector FVector::ZeroVector(0, 0, 0);
inline const FVector FVector::UpVector(0, 0, 1);
inline const FVector FVector::ForwardVector(1, 0, 0);
inline const FVector FVector::RightVector(0, 1, 0);

struct FVector3f
{
	float X = 0, Y = 0, Z = 0;
	FVector3f() = default;
	FVector3f(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit FVector3f(const FVector& V) : X((float)V.X), Y((float)V.Y), Z((float)V.Z) {}
	FVector3f operator+(const FVector3f& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
	FVector3f operator-(const FVector3f& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
	FVector3f operator*(float S) const { return { X * S, Y * S, Z * S }; }
	FVector3f operator/(float S) const { return { X / S, Y / S, Z / S }; }
	float Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	FVector3f GetSafeNormal() const { const float L = Size(); return L < 1e-8f ? FVector3f() : *this / L; }
	static FVector3f CrossProduct(const FVector3f& A, const FVector3f& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	static float DotProduct(const FVector3f& A, const FVector3f& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static const FVector3f UpVector, XAxisVector;
};
inline const FVector3f FVector3f::UpVector(0, 0, 1);
inline const FVector3f FVector3f::XAxisVector(1, 0, 0);
inline FVector::FVector(const FVector3f& V) : X(V.X), Y(V.Y), Z(V.Z) {}

struct FVector2f
{
	float X = 0, Y = 0;
	FVector2f() = default;
	FVector2f(float InX, float InY) : X(InX), Y(InY) {}
	static const FVector2f ZeroVector;
};
inline const FVector2f FVector2f::ZeroVector(0, 0);

struct FVector2D
{
	double X = 0, Y = 0;
	FVector2D() = default;
	FVector2D(double InX, double InY) : X(InX), Y(InY) {}
	FVector2D operator-(const FVector2D& O) const { return { X - O.X, Y - O.Y }; }
	bool Equals(const FVector2D& O, double Tol = 1e-4) const { return std::fabs(X - O.X) <= Tol && std::fabs(Y - O.Y) <= Tol; }
	FVector2D GetSafeNormal() const { const double L = std::sqrt(X * X + Y * Y); return L < 1e-8 ? FVector2D() : FVector2D(X / L, Y / L); }
};

struct FVector4f
{
	float X = 0, Y = 0, Z = 0, W = 0;
	FVector4f() = default;
	FVector4f(float InX, float InY, float InZ, float InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
};

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	FRotator() = default;
	FRotator(double P, double Y, double R) : Pitch(P), Yaw(Y), Roll(R) {}
	/** Unreal's FRotationMatrix rows (X, Y, Z axes); a vector turns as V.X * Row0 + V.Y * Row1 + V.Z * Row2. */
	void Axes(FVector& AX, FVector& AY, FVector& AZ) const
	{
		const double D = 3.14159265358979323846 / 180.0;
		const double SP = std::sin(Pitch * D), CP = std::cos(Pitch * D), SY = std::sin(Yaw * D), CY = std::cos(Yaw * D);
		const double SR = std::sin(Roll * D), CR = std::cos(Roll * D);
		AX = FVector(CP * CY, CP * SY, SP);
		AY = FVector(SR * SP * CY - CR * SY, SR * SP * SY + CR * CY, -SR * CP);
		AZ = FVector(-(CR * SP * CY + SR * SY), CY * SR - CR * SP * SY, CR * CP);
	}
	FVector RotateVector(const FVector& V) const
	{
		FVector AX, AY, AZ;
		Axes(AX, AY, AZ);
		return AX * V.X + AY * V.Y + AZ * V.Z;
	}
	static const FRotator ZeroRotator;
};
inline const FRotator FRotator::ZeroRotator(0, 0, 0);

struct FMatrix
{
	FVector AX, AY, AZ;
	FMatrix(const FVector& InX, const FVector& InY, const FVector& InZ, const FVector&) : AX(InX), AY(InY), AZ(InZ) {}
	/** Unreal's FMatrix::Rotator(). */
	FRotator Rotator() const
	{
		const double R2D = 180.0 / 3.14159265358979323846;
		FRotator R(std::atan2(AX.Z, std::sqrt(AX.X * AX.X + AX.Y * AX.Y)) * R2D, std::atan2(AX.Y, AX.X) * R2D, 0);
		FVector SX, SY, SZ;
		R.Axes(SX, SY, SZ);
		R.Roll = std::atan2(FVector::DotProduct(AZ, SY), FVector::DotProduct(AY, SY)) * R2D;
		return R;
	}
};

struct FColor
{
	uint8 R, G, B;
	FColor(uint8 InR, uint8 InG, uint8 InB) : R(InR), G(InG), B(InB) {}
};

struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1.f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor& operator*=(float S) { R *= S; G *= S; B *= S; A *= S; return *this; }
	static float Lin(uint8 C) { const float F = C / 255.f; return F <= 0.04045f ? F / 12.92f : std::pow((F + 0.055f) / 1.055f, 2.4f); }
	static FLinearColor FromSRGBColor(const FColor& C) { return FLinearColor(Lin(C.R), Lin(C.G), Lin(C.B)); }
};

/** Unreal's FRandomStream (same LCG). */
struct FRandomStream
{
	int32 Seed;
	explicit FRandomStream(int32 In) : Seed(In) {}
	void Mutate() { Seed = (int32)((uint32)Seed * 196314165u + 907633515u); }
	float GetFraction()
	{
		Mutate();
		uint32 Bits = 0x3F800000u | ((uint32)Seed >> 9);
		float F;
		std::memcpy(&F, &Bits, 4);
		return F - 1.f;
	}
	float FRandRange(float A, float B) { return A + (B - A) * GetFraction(); }
	int32 RandRange(int32 A, int32 B) { const int32 Range = (B - A) + 1; return A + std::min((int32)(GetFraction() * Range), Range - 1); }
};
