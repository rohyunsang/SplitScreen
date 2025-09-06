// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class USSGameInstance;
class ASSGameMode;

// ���Ͻð� �������ִ� ���� ī�޶� ������(����: ���� ���Ǵ� SSCameraViewProxy.h)
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

    // �����/������
    UPROPERTY()
    FVector  Velocity = FVector::ZeroVector;        // ���ӵ� (cm/s)

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
    // ==== Ŭ�� ���ø� ��ũ�� / ���� ���� ====
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);

    // �� ƽ ����ȭ(����/���� ����)
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // ���� ������ -> �����丮 ����
    void UpdateCameraHistory(const FRepCamInfo& ServerCam);

    // ������ ���� + ������ �ʰ�����(ȸ���� ���ʹϾ� Slerp)
    FCameraPredictionData PredictCameraMovement();

    // �Ӱ谨���� ���� ����(��ú��� ����)
    FCameraPredictionData CorrectPredictionWithServerData(const FCameraPredictionData& Prediction,
        const FRepCamInfo& ServerData);

    // ����� ���� ���� 1ȸ�� ���� (��Ʈ�ѷ� ȸ�� X)
    void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // �����
    void DebugCameraPrediction();

    // ==== RPCs ====
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);
    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

private:
    // === ���� �÷��� ===
    UPROPERTY(VisibleAnywhere, Category = "SS|State")
    bool bClientSplitScreenSetupComplete = false;

    UPROPERTY(VisibleAnywhere, Category = "SS|State")
    bool bIsDummyController = false;

    // === ���� �� / Ÿ�̸� ===
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn = nullptr;

    FTimerHandle ClientSyncTimerHandle;

    // === ���Ͻ� ĳ�� ===
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

    // === ī�޶� �����丮/���� ===
    UPROPERTY()
    TArray<FCameraPredictionData> CameraHistory;

    UPROPERTY()
    FCameraPredictionData PredictedCamera;

    UPROPERTY()
    FCameraPredictionData LastServerCamera;

    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "2", ClampMax = "64"))
    int32 MaxHistorySize = 16;

    // === ����/���� �Ķ���� ===
    // ���� ���� �ð� = Now - InterpDelaySec
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.01", ClampMax = "0.25"))
    float InterpDelaySec = 0.12f;

    // ���� ���� �� �ִ� �ʰ����� ���
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float MaxExtrapolateSec = 0.03f;

    // ���ӵ� ���� (deg/s)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "30.0", ClampMax = "720.0"))
    float MaxAngularSpeedDeg = 180.f;

    // ���� �Ǵ� ���ӵ�(cm/s)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float LinearDeadzoneCmPerS = 1.f;

    // ������ �Ǵ�(������ deg/s^2)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "0.0", ClampMax = "45.0"))
    float AngAccelDeadzoneDegS2 = 5.f;

    // ���� ���� �Ӱ谨�� ����(���� -> ������ ����)
    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float PosErrorGain = 8.f;

    UPROPERTY(EditAnywhere, Category = "SS|Prediction", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float RotErrorGain = 8.f;

    // === ��Ʈ ��ġ ��ε�ĳ��Ʈ ���� ===
    UPROPERTY(EditAnywhere, Category = "SS|Net", meta = (ClampMin = "0.01", ClampMax = "0.2"))
    float LocationUpdateInterval = 0.05f;

    float TimeSinceLastUpdate = 0.f;
};
