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

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // === Split Screen Setup ===
    UFUNCTION()
    void SetupOnlineSplitScreen();

    UFUNCTION()
    void CreateDummyLocalPlayer();

    // SetViewTarget 업데이트 함수
    UFUNCTION()
    void UpdateDummyViewTarget();

    // Deprecated functions (하위 호환성을 위해 유지)
    UFUNCTION()
    void AttachDummySpectatorToClient(APlayerController* RemoteClient);

    UFUNCTION()
    void SyncDummyRotationWithProxy();

protected:
    // === Configuration ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    bool bAutoEnableSplitScreen = true;

    // 더미 스펙테이터 폰 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Split Screen")
    TSubclassOf<ASSDummySpectatorPawn> DummySpectatorPawnClass;

    // === Runtime Data ===
    // 연결된 플레이어 목록
    UPROPERTY()
    TArray<APlayerController*> ConnectedPlayers;

    // 더미 플레이어 컨트롤러
    UPROPERTY()
    ASSPlayerController* DummyPlayerController = nullptr;

    // 더미 스펙테이터 폰 (선택사항)
    UPROPERTY()
    ASSDummySpectatorPawn* DummySpectatorPawn = nullptr;

    // 카메라 프록시 시스템
    UPROPERTY()
    TMap<APlayerController*, ASSCameraViewProxy*> ClientCamProxies;

    UPROPERTY()
    ASSCameraViewProxy* ServerCamProxy = nullptr;

    // 타이머 핸들
    FTimerHandle ViewTargetUpdateTimerHandle;

    // Deprecated timer handles (하위 호환성)
    FTimerHandle RotationSyncTimerHandle;

public:
    // === Getters ===
    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    ASSPlayerController* GetDummyPlayerController() const { return DummyPlayerController; }

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    ASSDummySpectatorPawn* GetDummySpectatorPawn() const { return DummySpectatorPawn; }

    UFUNCTION(BlueprintCallable, Category = "Split Screen")
    int32 GetConnectedPlayerCount() const { return ConnectedPlayers.Num(); }
};
