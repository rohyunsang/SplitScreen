// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class USSGameInstance;
class ASSGameMode;

// 프록시가 복제해주는 서버 카메라 데이터(선언만: 실제 정의는 SSCameraViewProxy.h)
struct FRepCamInfo;

USTRUCT(BlueprintType)
struct FCameraPredictionData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector  Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    float    FOV = 90.f;

    UPROPERTY()
    float    Timestamp = 0.f;

    // 디버그/추정용
    UPROPERTY()
    FVector  Velocity = FVector::ZeroVector;        // 선속도 (cm/s)

    UPROPERTY()
    FVector  AngularVelocity = FVector::ZeroVector; // (deg/s) Pitch, Yaw, Roll
};

UCLASS()
class SPLITSCREEN_API ASSPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASSPlayerController();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable)
    void SetAsDummyController(bool bDummy);

protected:
    // ==== 클라 스플릿 스크린 / 더미 세팅 ====
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);

    // 매 틱 동기화(예측/보정 포함)
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // 서버 스냅샷 -> 히스토리 축적
    void UpdateCameraHistory(const FRepCamInfo& ServerCam);

    // 스냅샷 보간 + 제한적 초과보간(회전은 쿼터니언 Slerp)
    FCameraPredictionData PredictCameraMovement();

    // 임계감쇠형 에러 보정(즉시보정 없음)
    FCameraPredictionData CorrectPredictionWithServerData(const FCameraPredictionData& Prediction,
        const FRepCamInfo& ServerData);

    // 결과를 더미 폰에 1회만 적용 (컨트롤러 회전 X)
    void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // 디버그
    void DebugCameraPrediction();

    // ==== RPCs ====
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);
    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

private:
    // === 상태 플래그 ===
    UPROPERTY(VisibleAnywhere, Category = "SS|State")
    bool bClientSplitScreenSetupComplete = false;

    UPROPERTY(VisibleAnywhere, Category = "SS|State")
    bool bIsDummyController = false;

    // === 더미 폰 / 타이머 ===
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn = nullptr;

    FTimerHandle ClientSyncTimerHandle;

    // === 프록시 캐시 ===
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

    // === 카메라 히스토리/예측 ===
    UPROPERTY()
    TArray<FCameraPredictionData> CameraHistory;

    UPROPERTY()
    FCameraPredictionData PredictedCamera;

    UPROPERTY()
    FCameraPredictionData LastServerCamera;

    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "2", ClampMax = "64"))
    int32 MaxHistorySize = 16;

    // === 보간/예측 파라미터 ===
    // 렌더 기준 시간 = Now - InterpDelaySec
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.01", ClampMax = "0.25"))
    float InterpDelaySec = 0.12f;

    // 샘플 부족 시 최대 초과보간 허용
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float MaxExtrapolateSec = 0.03f;

    // 각속도 상한 (deg/s)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "30.0", ClampMax = "720.0"))
    float MaxAngularSpeedDeg = 180.f;

    // 정지 판단 선속도(cm/s)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float LinearDeadzoneCmPerS = 1.f;

    // 저가속 판단(각가속 deg/s^2)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float AngAccelDeadzoneDegS2 = 5.f;

    // 에러 보정 임계감쇠 게인(값↑ -> 빠르게 붙음)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float PosErrorGain = 8.f;

    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float RotErrorGain = 8.f;

    // === 네트 위치 브로드캐스트 간격 ===
    UPROPERTY(EditAnywhere, Category = "SS|Net", meta = (ClampMin = "0.01", ClampMax = "0.2"))
    float LocationUpdateInterval = 0.05f;

    float TimeSinceLastUpdate = 0.f;
};
