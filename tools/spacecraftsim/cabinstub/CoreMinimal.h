#pragma once
// A rough stand-in for the engine, only so that clang can PARSE Private/Spacecraft/LyudSpacecraftCabin.cpp in the cloud
// (tools/spacecraftsim/cabincheck.py: syntax, types, const-correctness, shadowed names, unused things). Every signature
// here is what the cabin ASSUMES of Unreal, copied from call sites in this repo: it proves nothing about the engine.
// Nothing here is ever compiled into the game.
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
typedef int32_t int32; typedef uint8_t uint8; typedef uint32_t uint32; typedef uint64_t uint64; typedef int64_t int64; typedef wchar_t TCHAR;
#define TEXT(x) L##x
#define PI (3.1415926535897932f)
#define MIN_int32 ((int32)0x80000000)
#define FORCEINLINE inline
#define LYUDMILOVSK_API
#define UCLASS(...)
#define UPROPERTY(...)
#define UFUNCTION(...)
#define GENERATED_BODY(...)
#define UE_LOG(Cat, Verb, Fmt, ...) StubLog(Fmt, ##__VA_ARGS__)
template <typename... A> void StubLog(const TCHAR*, A&&...) {}
#define UE_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define RETURN_QUICK_DECLARE_CYCLE_STAT(a, b) return TStatId()
enum { INDEX_NONE = -1 };
enum EObjectFlags { RF_Transient = 1 };
struct TStatId {};
template <typename T> struct TArray
{
	std::vector<T> V;
	int32 Num() const { return (int32)V.size(); }
	T& operator[](int32 I) { return V[(size_t)I]; }
	const T& operator[](int32 I) const { return V[(size_t)I]; }
	bool IsValidIndex(int32 I) const { return I >= 0 && I < Num(); }
	template <typename U> int32 Add(U&& X) { V.emplace_back(std::forward<U>(X)); return Num() - 1; }
	void Reset() { V.clear(); }
	void SetNum(int32 N) { V.resize((size_t)N); }
	T& AddDefaulted_GetRef() { V.emplace_back(); return V.back(); }
	auto begin() { return V.begin(); } auto end() { return V.end(); }
	auto begin() const { return V.begin(); } auto end() const { return V.end(); }
};
struct FString
{
	std::wstring S;
	FString() {}
	FString(const TCHAR* In) : S(In) {}
	const TCHAR* operator*() const { return S.c_str(); }
	FString operator+(const FString& O) const { FString R; R.S = S + O.S; return R; }
	FString operator+(const TCHAR* O) const { FString R; R.S = S + O; return R; }
	FString operator/(const TCHAR* O) const { FString R; R.S = S + L"/" + O; return R; }
	bool operator==(const TCHAR* O) const { return S == O; }
	bool operator!=(const TCHAR* O) const { return S != O; }
	bool operator==(const FString& O) const { return S == O.S; }
	int32 ParseIntoArray(TArray<FString>& Out, const TCHAR*, bool = true) const { Out.Reset(); return 0; }
	bool IsEmpty() const { return S.empty(); }
};
struct FName
{
	int32 I = 0;
	FName() {}
	FName(const TCHAR*) {}
	bool operator==(const FName& O) const { return I == O.I; }
	bool operator!=(const FName& O) const { return I != O.I; }
	bool IsNone() const { return I == 0; }
};
template <typename T> struct TSharedPtr
{
	std::shared_ptr<T> P;
	TSharedPtr() {}
	bool IsValid() const { return (bool)P; }
	void Reset() { P.reset(); }
	T* operator->() const { return P.get(); }
	T& operator*() const { return *P; }
	explicit operator bool() const { return (bool)P; }
};
template <typename T> struct TObjectPtr
{
	T* P = nullptr;
	TObjectPtr() {}
	TObjectPtr(T* In) : P(In) {}
	template <typename U, typename = std::enable_if_t<std::is_convertible<U*, T*>::value>> TObjectPtr(U* In) : P(In) {}
	TObjectPtr(std::nullptr_t) {}
	T* Get() const { return P; }
	operator T*() const { return P; }
	T* operator->() const { return P; }
	explicit operator bool() const { return P != nullptr; }
	bool operator!() const { return P == nullptr; }
};
template <typename T> struct TWeakObjectPtr { T* P = nullptr; };
struct FRotator; struct FQuat;
namespace EAxis { enum Type { None, X, Y, Z }; }
struct FVector
{
	double X, Y, Z;
	FVector() {}
	explicit FVector(double A) : X(A), Y(A), Z(A) {}
	FVector(double A, double B, double C) : X(A), Y(B), Z(C) {}
	FVector operator+(const FVector& O) const { return FVector(X + O.X, Y + O.Y, Z + O.Z); }
	FVector operator-(const FVector& O) const { return FVector(X - O.X, Y - O.Y, Z - O.Z); }
	FVector operator-() const { return FVector(-X, -Y, -Z); }
	FVector operator*(double S) const { return FVector(X * S, Y * S, Z * S); }
	FVector operator/(double S) const { return FVector(X / S, Y / S, Z / S); }
	FVector& operator+=(const FVector& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	FVector GetSafeNormal() const { return *this; }
	double Size() const { return std::sqrt(X * X + Y * Y + Z * Z); }
	bool Equals(const FVector& O, double Tol = 1e-4) const { return std::abs(X - O.X) < Tol; }
	FRotator Rotation() const;
	static double DotProduct(const FVector& A, const FVector& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	static const FVector ZeroVector, UpVector, XAxisVector, ZAxisVector;
};
inline FVector operator*(double S, const FVector& V) { return V * S; }
struct FVector2D
{
	double X, Y;
	FVector2D() {}
	FVector2D(double A, double B) : X(A), Y(B) {}
	explicit FVector2D(const FVector& V) : X(V.X), Y(V.Y) {}
	double SizeSquared() const { return X * X + Y * Y; }
	bool Equals(const FVector2D& O, double Tol = 1e-4) const { return std::abs(X - O.X) < Tol; }
	static const FVector2D ZeroVector;
};
struct FQuat
{
	double X = 0, Y = 0, Z = 0, W = 1;
	FQuat() {}
	FQuat(const FVector&, double) {}
	FQuat operator*(const FQuat&) const { return *this; }
	FQuat Inverse() const { return *this; }
	FQuat GetNormalized() const { return *this; }
	static FQuat Slerp(const FQuat& A, const FQuat&, double) { return A; }
	static const FQuat Identity;
};
struct FRotator
{
	double Pitch = 0, Yaw = 0, Roll = 0;
	FRotator() {}
	FRotator(double P, double Y, double R) : Pitch(P), Yaw(Y), Roll(R) {}
	FQuat Quaternion() const { return FQuat(); }
	static double NormalizeAxis(double A) { return A; }
	static const FRotator ZeroRotator;
};
inline FRotator FVector::Rotation() const { return FRotator(); }
struct FMatrix { FQuat ToQuat() const { return FQuat(); } };
struct FRotationMatrix { static FMatrix MakeFromZY(const FVector&, const FVector&) { return FMatrix(); } };
struct FTransform
{
	FQuat Q; FVector T = FVector(0, 0, 0);
	FTransform() {}
	explicit FTransform(const FVector& InT) : T(InT) {}
	FTransform(const FQuat& InQ, const FVector& InT) : Q(InQ), T(InT) {}
	FTransform(const FRotator&, const FVector& InT) : T(InT) {}
	FVector GetLocation() const { return T; }
	FQuat GetRotation() const { return Q; }
	FRotator Rotator() const { return FRotator(); }
	void SetRotation(const FQuat& In) { Q = In; }
	void SetScale3D(const FVector&) {}
	FVector TransformPosition(const FVector& V) const { return V; }
	FVector InverseTransformPosition(const FVector& V) const { return V; }
	FVector InverseTransformVectorNoScale(const FVector& V) const { return V; }
	FVector GetUnitAxis(EAxis::Type) const { return T; }
	FTransform GetRelativeTransform(const FTransform&) const { return *this; }
	FTransform operator*(const FTransform&) const { return *this; }
	static const FTransform Identity;
};
struct FLinearColor { float R, G, B, A; FLinearColor() {} FLinearColor(float r, float g, float b, float a = 1.f) : R(r), G(g), B(b), A(a) {} };
struct FMath
{
	template <typename T> static T Clamp(T X, T A, T B) { return X < A ? A : (X > B ? B : X); }
	template <typename T> static T Max(T A, T B) { return A > B ? A : B; }
	template <typename T> static T Min(T A, T B) { return A < B ? A : B; }
	template <typename T> static T Abs(T A) { return A < 0 ? -A : A; }
	template <typename T> static T Square(T A) { return A * A; }
	template <typename T, typename U> static T Lerp(const T& A, const T& B, const U& Alpha) { return (T)(A + Alpha * (B - A)); }
	static float FInterpConstantTo(float C, float, float, float) { return C; }
	static float FindDeltaAngleDegrees(float A, float B) { return B - A; }
	static float RoundToFloat(float F) { return F; } static double RoundToFloat(double F) { return F; }
	static int32 RoundToInt(float F) { return (int32)F; } static int64 RoundToInt(double F) { return (int64)F; } // UE5: the double one is int64
	static float FloorToFloat(float F) { return F; }
	static float Frac(float F) { return F; }
	static float Fmod(float A, float) { return A; } static double Fmod(double A, double) { return A; }
	static float Sin(float A) { return A; } static double Sin(double A) { return A; }
	static float Cos(float A) { return A; } static double Cos(double A) { return A; }
	static float Tan(float A) { return A; }
	static float Atan(float A) { return A; }
	static float Atan2(float A, float) { return A; } static double Atan2(double A, double) { return A; }
	static float Acos(float A) { return A; }
	static double Asin(double A) { return A; }
	template <typename T> static T DegreesToRadians(T A) { return A; }
	template <typename T> static T RadiansToDegrees(T A) { return A; }
};
// ---- objects
struct UClass {};
struct UObject { FString GetName() const { return FString(); } };
template <typename T> T* NewObject(UObject*, FName = FName(), EObjectFlags = RF_Transient) { return new T(); }
template <typename T> T* LoadObject(UObject*, const TCHAR*) { return nullptr; }
inline FName MakeUniqueObjectName(UObject*, UClass*, FName N) { return N; }
inline bool IsValid(const UObject* O) { return O != nullptr; }
template <typename To, typename From> To* Cast(From* F) { return static_cast<To*>(F); }
namespace EComponentMobility { enum Type { Static, Movable }; }
namespace ECollisionEnabled { enum Type { NoCollision, QueryOnly }; }
enum class ELightUnits { Candelas, Lumens };
enum class ETeleportType { None };
struct FHitResult;
struct UTexture2D : UObject {};
struct UMaterialInterface : UObject {};
struct UMaterialInstanceDynamic : UMaterialInterface
{
	TObjectPtr<UMaterialInterface> Parent;
	static UMaterialInstanceDynamic* Create(UMaterialInterface*, UObject*) { return nullptr; }
	void SetScalarParameterValue(FName, float) {}
	void SetVectorParameterValue(FName, FLinearColor) {}
	void SetTextureParameterValue(FName, UTexture2D*) {}
};
struct UStaticMesh : UObject {};
struct USceneComponent : UObject
{
	static UClass* StaticClass() { return nullptr; }
	void SetupAttachment(USceneComponent*) {}
	void SetMobility(EComponentMobility::Type) {}
	void RegisterComponent() {}
	void SetRelativeLocation(FVector) {}
	void SetRelativeLocationAndRotation(FVector, FRotator, bool = false, FHitResult* = nullptr, ETeleportType = ETeleportType::None) {}
	void SetRelativeLocationAndRotation(FVector, const FQuat&, bool = false, FHitResult* = nullptr, ETeleportType = ETeleportType::None) {}
	void SetRelativeRotation(const FQuat&) {}
	void SetRelativeRotation(FRotator) {}
	void SetRelativeTransform(const FTransform&) {}
	void SetRelativeScale3D(FVector) {}
	bool IsVisible() const { return true; }
	void SetVisibility(bool, bool = false) {}
	void DestroyComponent(bool = false) {}
	bool IsBeingDestroyed() const { return false; }
};
struct UPrimitiveComponent : USceneComponent
{
	uint8 bAffectDistanceFieldLighting : 1; uint8 bAffectDynamicIndirectLighting : 1;
	void SetCollisionEnabled(ECollisionEnabled::Type) {}
	void SetGenerateOverlapEvents(bool) {}
	void SetCanEverAffectNavigation(bool) {}
	void SetCastShadow(bool) {}
	void SetReceivesDecals(bool) {}
	void SetLightingChannels(bool, bool, bool) {}
	int32 GetMaterialIndex(FName) const { return 0; }
	UMaterialInterface* GetMaterial(int32) const { return nullptr; }
	void SetMaterial(int32, UMaterialInterface*) {}
	void SetCollisionProfileName(FName) {}
};
struct UStaticMeshComponent : UPrimitiveComponent { void SetStaticMesh(UStaticMesh*) {} TObjectPtr<UStaticMesh> GetStaticMesh() const { return nullptr; } };
struct UBoxComponent : UPrimitiveComponent { void SetBoxExtent(FVector, bool = true) {} };
struct UPointLightComponent : USceneComponent
{
	void SetIntensityUnits(ELightUnits) {} void SetIntensity(float) {} void SetAttenuationRadius(float) {} void SetLightColor(FLinearColor, bool = true) {}
	void SetSourceRadius(float) {} void SetCastShadows(bool) {}
};
struct UCollisionProfile { static FName BlockAll_ProfileName; };
enum ETickingGroup { TG_PostPhysics };
struct FActorTick { bool bCanEverTick = false, bStartWithTickEnabled = true; ETickingGroup TickGroup = TG_PostPhysics; };
namespace EEndPlayReason { enum Type { Destroyed }; }
struct AActor : UObject
{
	FActorTick PrimaryActorTick;
	TObjectPtr<USceneComponent> RootComponent;
	virtual ~AActor() {}
	USceneComponent* GetRootComponent() const { return RootComponent; }
	const FTransform& GetActorTransform() const { static FTransform T; return T; }
	void SetActorTickEnabled(bool) {}
	virtual void Tick(float) {}
	virtual void EndPlay(const EEndPlayReason::Type) {}
	virtual void BeginDestroy() {}
	template <typename T> T* CreateDefaultSubobject(const TCHAR*) { return new T(); }
};
struct ACharacter; struct APlayerController; struct UWorld;
struct UCameraModifier : UObject { virtual ~UCameraModifier() {} virtual bool ModifyCamera(float, struct FMinimalViewInfo&) { return false; } };
namespace EWorldType { enum Type { Game }; }
struct UTickableWorldSubsystem : UObject
{
	virtual ~UTickableWorldSubsystem() {}
	virtual void OnWorldBeginPlay(UWorld&) {} virtual void Deinitialize() {} virtual void Tick(float) {} virtual TStatId GetStatId() const { return TStatId(); }
	virtual bool DoesSupportWorldType(const EWorldType::Type) const { return true; }
};
struct ILyudSaveable { virtual ~ILyudSaveable() {} virtual void OnGameSaving_Implementation() {} virtual void OnGameLoaded_Implementation() {} };
// ---- json, files, console
enum class EJson { None, Null, String, Number, Boolean, Array, Object };
struct FJsonObject;
struct FJsonValue
{
	EJson Type = EJson::None;
	double AsNumber() const { return 0; }
	FString AsString() const { return FString(); }
	const TArray<TSharedPtr<FJsonValue>>& AsArray() const { static TArray<TSharedPtr<FJsonValue>> A; return A; }
	const TSharedPtr<FJsonObject>& AsObject() const { static TSharedPtr<FJsonObject> O; return O; }
};
struct FJsonObject
{
	TSharedPtr<FJsonValue> TryGetField(const FString&) const { return TSharedPtr<FJsonValue>(); }
	bool TryGetArrayField(const FString&, const TArray<TSharedPtr<FJsonValue>>*&) const { return false; }
};
struct FFileHelper { static bool LoadFileToString(FString&, const TCHAR*) { return false; } };
struct FPaths { static FString ProjectContentDir() { return FString(); } };
struct FJsonReaderStub {};
template <typename C = TCHAR> struct TJsonReaderFactory { static FJsonReaderStub Create(const FString&) { return FJsonReaderStub(); } };
struct FJsonSerializer { static bool Deserialize(FJsonReaderStub, TSharedPtr<FJsonObject>&) { return false; } };
struct FDelegateHandle { bool IsValid() const { return false; } void Reset() {} };
struct IConsoleVariable;
struct FConsoleVariableMulticastDelegate
{
	template <typename T> FDelegateHandle AddUObject(T*, void (T::*)(IConsoleVariable*)) { return FDelegateHandle(); }
	void Remove(FDelegateHandle) {}
};
struct IConsoleVariable { FConsoleVariableMulticastDelegate& OnChangedDelegate() { static FConsoleVariableMulticastDelegate D; return D; } };
struct IConsoleManager { static IConsoleManager& Get() { static IConsoleManager M; return M; } IConsoleVariable* FindConsoleVariable(const TCHAR*) { return nullptr; } };
namespace LyudText { inline UTexture2D* LoadTexture(const FString&, const FString&) { return nullptr; } }
