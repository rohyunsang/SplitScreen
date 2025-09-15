// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SSGameMode.generated.h"

struct FRepCamInfo;
class ASSDummySpectatorPawn;
class ASSPlayerController;
class ASSCameraViewProxy;
class ACharacter;

// 카메라 예측을 위한 데이터 구조체
USTRUCT(BlueprintType)
struct FCameraPredictionDataGM
{
	GENERATED_BODY()

	UPROPERTY() FVector  Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float    FOV = 90.0f;
	float    SpringArmLength = 0.f;

	// 예측용
	UPROPERTY() FVector  Velocity = FVector::ZeroVector;
	UPROPERTY() FVector  AngularVelocity = FVector::ZeroVector;
	UPROPERTY() float    Timestamp = 0.0f;
};


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
	virtual void Tick(float DeltaTime) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
	bool bAutoEnableSplitScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
	TSubclassOf<APawn> DummySpectatorPawnClass;

protected:
	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void SetupOnlineSplitScreen();

	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void UpdateSplitScreenLayout();

private:
	// 접속한 실제 네트워크 플레이어들(더미 제외)
	UPROPERTY(Transient)
	TArray<APlayerController*> ConnectedPlayers;

	// 서버 2P 화면용 더미
	UPROPERTY(Transient)
	ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;

	UPROPERTY(Transient)
	ASSPlayerController* DummyPlayerController = nullptr;

	// 카메라 프록시 (GC 보호)
public:
	UPROPERTY(Transient)
	ASSCameraViewProxy* ServerCamProxy = nullptr;

	UPROPERTY(Transient)
	ASSCameraViewProxy* ClientCamProxy = nullptr;

private:
	// 동기화/프록시/탐색
	void CreateDummyLocalPlayer();
	void SetupCameraProxies();
	void SyncDummyPlayerWithProxy();
	ACharacter* FindRemoteCharacter() const;

	// 적용 (예측 버전)
	void ApplyPredictedCamera(ASSDummySpectatorPawn* DummyPawn, const FCameraPredictionDataGM& CameraData);

	// 호환용: 헤더에 있던 선언을 실제 구현으로 채워둠(예측 파이프라인 래퍼)
	void ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData);

	FTimerHandle SyncTimerHandle;

	// 캐시
	TWeakObjectPtr<ASSCameraViewProxy> CachedClientProxy;
	TWeakObjectPtr<ACharacter>         CachedRemoteCharacter;
	TWeakObjectPtr<APlayerController>  CachedRemotePC;
	TWeakObjectPtr<APlayerController>  CachedHostPC;

	// 튜닝 파라미터
	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerSnapDistance = 250.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerLocationInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerRotationInterpSpeed = 28.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float PivotZOffset = 60.f;

	// ───────── 예측용 상태/튜닝 ─────────
	// 히스토리
	UPROPERTY(Transient)
	TArray<FCameraPredictionDataGM> CameraHistory;

	// 마지막으로 수신한 클라 카메라(원천)
	UPROPERTY(Transient)
	FCameraPredictionDataGM LastClientCamera;

	// 최신 예측 결과
	UPROPERTY(Transient)
	FCameraPredictionDataGM PredictedCamera;

	// 예측 튜닝
	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	int32 MaxHistorySize = 10;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	float MaxPredictionTime = 0.06f;   // 초 (네트 지연 보정용)

	UPROPERTY(EditAnywhere, Category = "Split Screen|Prediction")
	float CorrectionSpeed = 20.f;      // 보정 보간 속도

	// 예측 로직
	void UpdateCameraHistory(const FRepCamInfo& ClientCam);
	FCameraPredictionDataGM PredictCameraMovement();
	FCameraPredictionDataGM CorrectPredictionWithServerData(const FCameraPredictionDataGM& Prediction,
		const FRepCamInfo& ClientCam);
	
};
