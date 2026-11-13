// A tiny stand-in for the parts of Unreal's CoreMinimal.h the plant model uses (tools/plantsim/run.sh): enough to compile
// PlantModel.cpp, the species presets and the herb builder with clang++ on any machine. Not Unreal: only what those files touch.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef uint8_t uint8;
typedef char TCHAR;

#define TEXT(x) x
#define FORCEINLINE inline
#define LYUDMILOVSK_API
#define UENUM(...)
#define USTRUCT(...)
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define GENERATED_BODY()
#define INDEX_NONE (-1)
#define UE_SMALL_NUMBER (1.e-8f)
#define PI (3.1415926535897932f)
#define ECVF_Default 0
#define ECVF_Scalability 0

template <typename T> struct TNumericLimits
{
	static constexpr T Max() { return std::numeric_limits<T>::max(); }
	static constexpr T Lowest() { return std::numeric_limits<T>::lowest(); }
};

template <typename T>
class TArray
{
public:
	std::vector<T> V;
	TArray() = default;
	TArray(std::initializer_list<T> L) : V(L) {}
	int32 Num() const { return (int32)V.size(); }
	bool IsEmpty() const { return V.empty(); }
	int32 Add(const T& X) { V.push_back(X); return Num() - 1; }
	int32 Add(T&& X) { V.push_back(std::move(X)); return Num() - 1; }
	int32 AddUnique(const T& X) { for (int32 I = 0; I < Num(); I++) if (V[I] == X) return I; return Add(X); }
	template <typename... A> int32 Emplace(A&&... Args) { V.emplace_back(std::forward<A>(Args)...); return Num() - 1; }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	void Reset() { V.clear(); }
	void Empty() { V.clear(); }
	void Reserve(int32 N) { V.reserve(N); }
	void Init(const T& X, int32 N) { V.assign(N, X); }
	void SetNum(int32 N) { V.resize(N); }
	void SetNumZeroed(int32 N) { V.assign(N, T()); }
	void SetNumUninitialized(int32 N) { V.resize(N); }
	T& Last() { return V.back(); }
	const T& Last() const { return V.back(); }
	void Append(const TArray& O) { V.insert(V.end(), O.V.begin(), O.V.end()); }
	void Append(TArray&& O) { for (T& X : O.V) V.push_back(std::move(X)); }
	void Append(const T* P, int32 N) { V.insert(V.end(), P, P + N); }
	T* GetData() { return V.data(); }
	const T* GetData() const { return V.data(); }
	T& operator[](int32 I) { return V[I]; }
	const T& operator[](int32 I) const { return V[I]; }
	typename std::vector<T>::iterator begin() { return V.begin(); }
	typename std::vector<T>::iterator end() { return V.end(); }
	typename std::vector<T>::const_iterator begin() const { return V.begin(); }
	typename std::vector<T>::const_iterator end() const { return V.end(); }
	template <typename P> T* FindByPredicate(P Pred) { for (T& X : V) if (Pred(X)) return &X; return nullptr; }
	template <typename P> const T* FindByPredicate(P Pred) const { for (const T& X : V) if (Pred(X)) return &X; return nullptr; }
	bool Contains(const T& X) const { return std::find(V.begin(), V.end(), X) != V.end(); }
};

class UObject;
class UStaticMesh;
class UTexture2D;

template <typename T> T&& MoveTemp(T& X) { return static_cast<T&&>(X); }

struct FMemory
{
	static void* Memcpy(void* D, const void* S, size_t N) { return std::memcpy(D, S, N); }
};

class FString
{
public:
	std::string S;
	FString() = default;
	FString(const char* C) : S(C ? C : "") {}
	FString(std::string X) : S(std::move(X)) {}
	const char* operator*() const { return S.c_str(); }
	static FString Printf(const char* Fmt, ...)
	{
		char Buf[2048];
		va_list A;
		va_start(A, Fmt);
		vsnprintf(Buf, sizeof(Buf), Fmt, A);
		va_end(A);
		return FString(Buf);
	}
	FString ToLower() const { std::string R = S; for (char& C : R) C = (char)std::tolower((unsigned char)C); return FString(R); }
	bool Contains(const char* X) const { return S.find(X) != std::string::npos; }
	bool StartsWith(const char* X) const { return S.rfind(X, 0) == 0; }
	bool IsEmpty() const { return S.empty(); }
	int32 Len() const { return (int32)S.size(); }
	bool operator==(const char* X) const { return S == X; }
	bool operator==(const FString& X) const { return S == X.S; }
	bool operator!=(const char* X) const { return S != X; }
};

class FName
{
public:
	std::string S;
	FName() = default;
	FName(const char* C) : S(C ? C : "") {}
	FString ToString() const { return FString(S); }
	bool IsNone() const { return S.empty(); }
	bool operator==(const FName& O) const { return S == O.S; }
	bool operator!=(const FName& O) const { return S != O.S; }
	bool operator==(const char* O) const { return S == O; }
};

struct FText
{
	FString S;
};

struct FMath
{
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Clamp(T X, T A, T B) { return X < A ? A : X > B ? B : X; }
	template <typename T> static T Abs(T X) { return X < 0 ? -X : X; }
	template <typename T> static T Square(T X) { return X * X; }
	template <typename T, typename U> static T Lerp(const T& A, const T& B, U Alpha) { return A + (B - A) * Alpha; }
	static float Sin(float X) { return std::sin(X); }
	static float Cos(float X) { return std::cos(X); }
	static float Atan2(float Y, float X) { return std::atan2(Y, X); }
	static float Pow(float A, float B) { return std::pow(A, B); }
	static float Exp(float X) { return std::exp(X); }
	static float Loge(float X) { return std::log(X); }
	static float Sqrt(float X) { return std::sqrt(X); }
	static float Frac(float X) { return X - std::floor(X); }
	static int32 FloorToInt(float X) { return (int32)std::floor(X); }
	static int32 CeilToInt(float X) { return (int32)std::ceil(X); }
	static int32 RoundToInt(float X) { return (int32)std::floor(X + 0.5f); }
	static float RoundToFloat(float X) { return std::floor(X + 0.5f); }
	static float DegreesToRadians(float X) { return X * (PI / 180.f); }
	static float RadiansToDegrees(float X) { return X * (180.f / PI); }
};

template <typename T>
struct TVec3
{
	T X = 0, Y = 0, Z = 0;
	TVec3() = default;
	TVec3(T InX, T InY, T InZ) : X(InX), Y(InY), Z(InZ) {}
	explicit TVec3(T S) : X(S), Y(S), Z(S) {}
	template <typename U> explicit TVec3(const TVec3<U>& O) : X((T)O.X), Y((T)O.Y), Z((T)O.Z) {}
	TVec3 operator+(const TVec3& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
	TVec3 operator-(const TVec3& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
	TVec3 operator-() const { return { -X, -Y, -Z }; }
	TVec3 operator*(T S) const { return { X * S, Y * S, Z * S }; }
	TVec3 operator*(const TVec3& O) const { return { X * O.X, Y * O.Y, Z * O.Z }; }
	TVec3 operator/(T S) const { return { X / S, Y / S, Z / S }; }
	TVec3& operator+=(const TVec3& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	TVec3& operator-=(const TVec3& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
	TVec3& operator*=(T S) { X *= S; Y *= S; Z *= S; return *this; }
	T Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	T SizeSquared() const { return X * X + Y * Y + Z * Z; }
	T Dot(const TVec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
	bool IsNearlyZero() const { return SizeSquared() < (T)1e-8; }
	bool ContainsNaN() const { return std::isnan(X) || std::isnan(Y) || std::isnan(Z); }
	TVec3 GetSafeNormal(T Tol = (T)UE_SMALL_NUMBER, const TVec3& Fallback = TVec3()) const
	{
		const T S2 = SizeSquared();
		if (S2 <= Tol) return Fallback;
		return *this / std::sqrt(S2);
	}
	static T DotProduct(const TVec3& A, const TVec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static TVec3 CrossProduct(const TVec3& A, const TVec3& B) { return { A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X }; }
	static T Dist(const TVec3& A, const TVec3& B) { return (A - B).Size(); }
	static const TVec3 ZeroVector, UpVector, XAxisVector, OneVector;
};
template <typename T> const TVec3<T> TVec3<T>::ZeroVector(0, 0, 0);
template <typename T> const TVec3<T> TVec3<T>::UpVector(0, 0, 1);
template <typename T> const TVec3<T> TVec3<T>::XAxisVector(1, 0, 0);
template <typename T> const TVec3<T> TVec3<T>::OneVector(1, 1, 1);
template <typename T> TVec3<T> operator*(T S, const TVec3<T>& V) { return V * S; }
typedef TVec3<float> FVector3f;
typedef TVec3<double> FVector;

template <typename T>
struct TVec2
{
	T X = 0, Y = 0;
	TVec2() = default;
	TVec2(T InX, T InY) : X(InX), Y(InY) {}
	TVec2 operator+(const TVec2& O) const { return { X + O.X, Y + O.Y }; }
	TVec2 operator-(const TVec2& O) const { return { X - O.X, Y - O.Y }; }
	TVec2 operator*(T S) const { return { X * S, Y * S }; }
	TVec2& operator+=(const TVec2& O) { X += O.X; Y += O.Y; return *this; }
	T Size() const { return std::sqrt(X * X + Y * Y); }
	static const TVec2 ZeroVector;
};
template <typename T> const TVec2<T> TVec2<T>::ZeroVector(0, 0);
typedef TVec2<float> FVector2f;
typedef TVec2<double> FVector2D;

struct FIntVector3
{
	int32 X = 0, Y = 0, Z = 0;
	FIntVector3() = default;
	FIntVector3(int32 A, int32 B, int32 C) : X(A), Y(B), Z(C) {}
};
struct FIntPoint
{
	int32 X = 0, Y = 0;
};

struct FColor
{
	uint8 R = 0, G = 0, B = 0, A = 255;
	FColor() = default;
	FColor(uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255) : R(InR), G(InG), B(InB), A(InA) {}
};

struct FLinearColor
{
	float R = 0, G = 0, B = 0, A = 1;
	FLinearColor() = default;
	FLinearColor(float InR, float InG, float InB, float InA = 1.f) : R(InR), G(InG), B(InB), A(InA) {}
	FLinearColor operator+(const FLinearColor& O) const { return { R + O.R, G + O.G, B + O.B, A + O.A }; }
	FLinearColor operator-(const FLinearColor& O) const { return { R - O.R, G - O.G, B - O.B, A - O.A }; }
	FLinearColor operator*(const FLinearColor& O) const { return { R * O.R, G * O.G, B * O.B, A * O.A }; }
	FLinearColor operator*(float S) const { return { R * S, G * S, B * S, A * S }; }
	FLinearColor operator/(float S) const { return { R / S, G / S, B / S, A / S }; }
	FLinearColor& operator+=(const FLinearColor& O) { R += O.R; G += O.G; B += O.B; A += O.A; return *this; }
	FColor ToFColor(bool) const
	{
		auto E = [](float C) { C = std::clamp(C, 0.f, 1.f); return (uint8)std::lround(255.f * (C <= 0.0031308f ? C * 12.92f : 1.055f * std::pow(C, 1.f / 2.4f) - 0.055f)); };
		return FColor(E(R), E(G), E(B), (uint8)std::lround(255.f * std::clamp(A, 0.f, 1.f)));
	}
	static const FLinearColor White, Black, Green;
};
inline const FLinearColor FLinearColor::White(1, 1, 1, 1);
inline const FLinearColor FLinearColor::Black(0, 0, 0, 1);
inline const FLinearColor FLinearColor::Green(0, 1, 0, 1);

struct FQuat
{
	double X = 0, Y = 0, Z = 0, W = 1;
	FVector RotateVector(const FVector& V) const
	{
		const FVector Q(X, Y, Z);
		const FVector T = FVector::CrossProduct(Q, V) * 2.0;
		return V + T * W + FVector::CrossProduct(Q, T);
	}
};

struct FQuat4f
{
	float X = 0, Y = 0, Z = 0, W = 1;
	FQuat4f() = default;
	FQuat4f(const FVector3f& Axis, float Angle)
	{
		const float S = std::sin(0.5f * Angle);
		X = Axis.X * S; Y = Axis.Y * S; Z = Axis.Z * S; W = std::cos(0.5f * Angle);
	}
	FVector3f RotateVector(const FVector3f& V) const
	{
		const FVector3f Q(X, Y, Z);
		const FVector3f T = FVector3f::CrossProduct(Q, V) * 2.f;
		return V + T * W + FVector3f::CrossProduct(Q, T);
	}
};

template <typename F> using TFunction = std::function<F>;

struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	FRotator() = default;
	FRotator(double P, double Y, double R) : Pitch(P), Yaw(Y), Roll(R) {}
	FQuat Quaternion() const
	{
		const double DR = PI / 360.0;
		const double SP = std::sin(Pitch * DR), CP = std::cos(Pitch * DR), SY = std::sin(Yaw * DR), CY = std::cos(Yaw * DR);
		const double SR = std::sin(Roll * DR), CR = std::cos(Roll * DR);
		FQuat Q;
		Q.X = CR * SP * SY - SR * CP * CY;
		Q.Y = -CR * SP * CY - SR * CP * SY;
		Q.Z = CR * CP * SY - SR * SP * CY;
		Q.W = CR * CP * CY + SR * SP * SY;
		return Q;
	}
};

struct FTransform
{
	FQuat Rot;
	FVector Loc;
	FVector Scale = FVector(1, 1, 1);
	FTransform() = default;
	FTransform(const FQuat& Q, const FVector& L, const FVector& S = FVector(1, 1, 1)) : Rot(Q), Loc(L), Scale(S) {}
	FTransform(const FRotator& R, const FVector& L, const FVector& S = FVector(1, 1, 1)) : Rot(R.Quaternion()), Loc(L), Scale(S) {}
	FVector GetLocation() const { return Loc; }
	FVector GetScale3D() const { return Scale; }
	void SetScale3D(const FVector& S) { Scale = S; }
	FVector TransformPosition(const FVector& P) const { return Rot.RotateVector(P * Scale) + Loc; }
};

inline uint32 HashCombineFast(uint32 A, uint32 B) { return A ^ (B + 0x9e3779b9u + (A << 6) + (A >> 2)); }

template <typename T>
class TAutoConsoleVariable
{
public:
	T Value;
	TAutoConsoleVariable(const char*, T Default, const char*, int) : Value(Default) {}
	T GetValueOnAnyThread() const { return Value; }
	T GetValueOnGameThread() const { return Value; }
};
