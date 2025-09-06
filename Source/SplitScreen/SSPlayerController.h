// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Components/InputComponent.h"
#include "SSPlayerController.generated.h"

class ASSDummySpectatorPawn;
class ASSCameraViewProxy;
class UCameraComponent;

// 카메라 예측을 위한 데이터 구조체
USTRUCT()
struct FCameraPredictionData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    float FOV = 90.0f;

    UPROPERTY()
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY()
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY()
    float Timestamp = 0.0f;

    FCameraPredictionData()
    {
        Location = FVector::ZeroVector;
        Rotation = FRotator::ZeroRotator;
        FOV = 90.0f;
        Velocity = FVector::ZeroVector;
        AngularVelocity = FVector::ZeroVector;
        Timestamp = 0.0f;
    }
};

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // 더미 컨트롤러 플래그
    UPROPERTY(BlueprintReadOnly, Category = "Split Screen")
    bool bIsDummyController = false;

    // 더미 컨트롤러로 설정
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetAsDummyController(bool bDummy = true);

    // 캐시된 프록시 참조
    UPROPERTY()
    TWeakObjectPtr<ASSCameraViewProxy> CachedProxy;

protected:
    // 네트워크 RPC 함수들
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerUpdatePlayerLocation(FVector Location, FRotator Rotation);

    UFUNCTION(Client, Reliable)
    void ClientReceiveRemotePlayerLocation(FVector Location, FRotator Rotation);

    // 입력 설정
    virtual void SetupInputComponent() override;

    // 카메라 회전 관련 
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MinimumMovementThreshold = 5.0f; // 최소 움직임 임계값 (cm)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MinimumRotationThreshold = 2.0f; // 최소 회전 임계값 (도)

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float StationaryPredictionReduction = 0.3f; // 정지 상태일 때 예측 강도 감소

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera Prediction")
    float MaxAngularVelocityMagnitude = 180.0f; // 최대 각속도 제한 (도/초)

private:
    // 네트워크 업데이트 관련
    float LocationUpdateInterval = 0.0167f; // 60fps
    float TimeSinceLastUpdate = 0.0f;

    // 클라이언트용 함수들
    void SetupClientSplitScreen();
    void CreateClientDummyPawn();
    void StartClientDummySync(ASSDummySpectatorPawn* DummyPawn);
    void SyncClientDummyWithRemotePlayer(ASSDummySpectatorPawn* DummyPawn);

    // 클라이언트 더미 관련 변수들
    UPROPERTY()
    ASSDummySpectatorPawn* ClientDummyPawn;

    FTimerHandle ClientSyncTimerHandle;

    UPROPERTY()
    bool bClientSplitScreenSetupComplete = false;

    // === 카메라 예측 시스템 ===

    // 서버로부터 받은 카메라 데이터 히스토리
    UPROPERTY()
    TArray<FCameraPredictionData> CameraHistory;

    // 현재 예측된 카메라 상태
    UPROPERTY()
    FCameraPredictionData PredictedCamera;

    // 마지막으로 받은 서버 데이터
    UPROPERTY()
    FCameraPredictionData LastServerCamera;

    // 예측 설정값들
    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float MaxPredictionTime = 0.2f; // 최대 예측 시간 (200ms)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float CorrectionSpeed = 10.0f; // 서버 데이터로 보정하는 속도

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    int32 MaxHistorySize = 10; // 저장할 히스토리 개수

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionLocationThreshold = 100.0f; // 즉시 보정할 위치 오차 임계값 (cm)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionRotationThreshold = 10.0f; // 즉시 보정할 회전 오차 임계값 (도)

    // 카메라 예측 관련 함수들
    void UpdateCameraHistory(const struct FRepCamInfo& ServerCam);
    FCameraPredictionData PredictCameraMovement();
    FCameraPredictionData CorrectPredictionWithServerData(const FCameraPredictionData& Prediction, const struct FRepCamInfo& ServerData);
    void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // 디버그용 함수
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugCameraPrediction();



    // 현재 움직임 상태 추적
    bool bIsMoving = false;
    bool bIsRotating = false;
    float StationaryTime = 0.0f;
};
