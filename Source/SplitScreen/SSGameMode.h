// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSDummySpectatorPawn.h"
#include "SSPlayerController.h"
#include "SSGameMode.generated.h"

struct FCameraPredictionData;

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSGameMode : public AGameModeBase        
{
	GENERATED_BODY()
	
public:
    ASSGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<APawn> DummySpectatorPawnClass;

    UPROPERTY() // GC 보호
        class ASSCameraViewProxy* ServerCamProxy = nullptr;

    UPROPERTY()
    TMap<APlayerController*, ASSCameraViewProxy*> ClientCamProxies;

    UPROPERTY()
    TMap<APlayerController*, ASSDummySpectatorPawn*> ClientSpectatorPawns;

protected:
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void SetupOnlineSplitScreen();

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    void UpdateSplitScreenLayout();

private:
    TArray<APlayerController*> ConnectedPlayers;
    class ASSDummySpectatorPawn* DummySpectatorPawn;
    class ASSPlayerController* DummyPlayerController;

    void CreateDummyLocalPlayer();
    void SyncDummyPlayerWithRemotePlayer();

    FTimerHandle SyncTimerHandle;

    // === 카메라 예측 시스템 (서버용) ===

    // 클라이언트로부터 받은 카메라 데이터 히스토리
    UPROPERTY()
    TArray<FCameraPredictionData> ClientCameraHistory;

    // 현재 예측된 카메라 상태
    UPROPERTY()
    FCameraPredictionData PredictedClientCamera;

    // 마지막으로 받은 클라이언트 데이터
    UPROPERTY()
    FCameraPredictionData LastClientCamera;

    // 예측 설정값들
    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float MaxPredictionTime = 0.03f; // 최대 예측 시간 (200ms)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float CorrectionSpeed = 30.f; // 클라이언트 데이터로 보정하는 속도

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    int32 MaxHistorySize = 10; // 저장할 히스토리 개수

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionLocationThreshold = 100.0f; // 즉시 보정할 위치 오차 임계값 (cm)

    UPROPERTY(EditAnywhere, Category = "Camera Prediction")
    float ImmediateCorrectionRotationThreshold = 10.0f; // 즉시 보정할 회전 오차 임계값 (도)

    // 카메라 예측 관련 함수들
    void UpdateClientCameraHistory(const struct FRepCamInfo& ClientCam);
    FCameraPredictionData PredictClientCameraMovement();
    FCameraPredictionData CorrectPredictionWithClientData(const FCameraPredictionData& Prediction, const struct FRepCamInfo& ClientData);
    void ApplyPredictedClientCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionData& CameraData);

    // 디버그용 함수
    UFUNCTION(BlueprintCallable, Category = "Debug")
    void DebugServerCameraPrediction();

};
