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

protected:
	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void SetupOnlineSplitScreen();

	UFUNCTION(BlueprintCallable, Category = "Split Screen")
	void UpdateSplitScreenLayout();

private:
	// 접속한 실제 네트워크 플레이어들(더미 제외)
	UPROPERTY(Transient)
	TArray<APlayerController*> ConnectedPlayers;

	// 서버 뷰포트의 2P 역할
	UPROPERTY(Transient)
	ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;

	UPROPERTY(Transient)
	ASSPlayerController* DummyPlayerController = nullptr;

	// 카메라 동기화
	void CreateDummyLocalPlayer();
	void SyncDummyPlayerWithProxy();
	void ApplyProxyCamera(ASSDummySpectatorPawn* DummyPawn, const FRepCamInfo& CamData);
	void SetupCameraProxies();

	FTimerHandle SyncTimerHandle;

public:
	// 카메라 프록시 (GC 보호)
	UPROPERTY(Transient)
	ASSCameraViewProxy* ServerCamProxy = nullptr;

	UPROPERTY(Transient)
	ASSCameraViewProxy* ClientCamProxy = nullptr;

private:
	// 캐시
	TWeakObjectPtr<ASSCameraViewProxy>   CachedClientProxy;
	TWeakObjectPtr<ACharacter>           CachedRemoteCharacter;

	// **핵심: 원격/호스트 PC 캐시**
	TWeakObjectPtr<APlayerController>    CachedRemotePC;
	TWeakObjectPtr<APlayerController>    CachedHostPC;

	// 튜닝 파라미터
	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerSnapDistance = 250.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerLocationInterpSpeed = 18.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float ServerRotationInterpSpeed = 28.f;

	UPROPERTY(EditAnywhere, Category = "Split Screen|Sync")
	float PivotZOffset = 60.f;

private:
	// **원격 캐릭터 탐색 (가장 신뢰도 높은 경로부터)**
	ACharacter* FindRemoteCharacter() const;
};
