// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SSPlayerController.generated.h"

// 파트너 뷰 데이터
USTRUCT(BlueprintType)
struct FPartnerViewData
{
    GENERATED_BODY()

    UPROPERTY() FVector  CameraLocation = FVector::ZeroVector;
    UPROPERTY() FRotator CameraRotation = FRotator::ZeroRotator;
    UPROPERTY() float    CameraArmLength = 300.f;
    UPROPERTY() float    CameraFOV = 90.f;
    UPROPERTY() bool     bIsValid = false;
    UPROPERTY() int32    PlayerID = -1;
};

/**
 * 
 */
UCLASS()
class SPLITSCREEN_API ASSPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
    ASSPlayerController();

    // 파트너 API
    UFUNCTION(BlueprintCallable, Category = "Partner System")
    void SetPartner(ASSPlayerController* NewPartner);

    UFUNCTION(BlueprintPure, Category = "Partner System")
    FORCEINLINE ASSPlayerController* GetPartner() const { return PartnerController; }

    UFUNCTION(BlueprintPure, Category = "Partner System")
    FORCEINLINE bool HasPartner() const { return PartnerController != nullptr; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ======= Replicated Vars =======
    UPROPERTY(ReplicatedUsing = OnRep_PartnerController, BlueprintReadOnly, Category = "Partner System")
    TObjectPtr<class ASSPlayerController> PartnerController;

    UPROPERTY(ReplicatedUsing = OnRep_PartnerPawn, BlueprintReadOnly, Category = "Partner System")
    TObjectPtr<class APawn> PartnerPawn;

    UFUNCTION() void OnRep_PartnerController();
    UFUNCTION() void OnRep_PartnerPawn();

    // ======= Partner View (Local Only) =======
    UPROPERTY() TObjectPtr<class APartnerSpectatorPawn> PartnerViewPawn;
    UPROPERTY() TObjectPtr<class ULocalPlayer> SecondaryLocalPlayer;

    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings") bool  bAutoCreatePartnerView = true;
    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings") float ViewUpdateRate = 0.033f; // 30fps
    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings") bool  bSmoothPartnerView = true;

    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings|PIP") float PIPSizeX = 0.3f;
    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings|PIP") float PIPSizeY = 0.3f;
    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings|PIP") float PIPOriginX = 0.68f;
    UPROPERTY(EditDefaultsOnly, Category = "Partner View Settings|PIP") float PIPOriginY = 0.02f;

    UPROPERTY(BlueprintReadOnly, Category = "Partner View") bool bShowPartnerView = false;
    UPROPERTY(BlueprintReadOnly, Category = "Partner View") bool bPartnerViewReady = false;

private:
    float ViewUpdateTimer = 0.f;
    bool  bIsConnectedToPartner = false;
    bool  bShowDebugInfo = false;

    // 내부 로컬 처리
    void SetupSecondaryViewport();
    void UpdatePartnerView();
    FPartnerViewData GetLocalCameraData() const;
    void ShowDebugInfo();

    // 입력 핸들러
    void OnTogglePartnerView();
    void OnToggleDebugInfo();

public:
    // 파트너 뷰 제어
    UFUNCTION(BlueprintCallable, Category = "Partner View") void CreatePartnerView();
    UFUNCTION(BlueprintCallable, Category = "Partner View") void DestroyPartnerView();
    UFUNCTION(BlueprintCallable, Category = "Partner View") void TogglePartnerView();
    UFUNCTION(BlueprintCallable, Category = "Partner View") void SetPartnerViewVisible(bool bVisible);
    UFUNCTION(BlueprintCallable, Category = "Partner View") void SetPIPPosition(float OriginX, float OriginY);
    UFUNCTION(BlueprintCallable, Category = "Partner View") void SetPIPSize(float SizeX, float SizeY);

    // ======= Networking =======
    UFUNCTION(Server, Unreliable) void ServerUpdateCameraView(const FPartnerViewData& ViewData);
    UFUNCTION(Server, Reliable)   void ServerRequestPartner();

    UFUNCTION(Client, Unreliable) void ClientReceivePartnerView(const FPartnerViewData& ViewData);
    UFUNCTION(Client, Reliable)   void ClientEnablePartnerView();
    UFUNCTION(Client, Reliable)   void ClientDisablePartnerView();
    UFUNCTION(Client, Reliable)   void ClientPartnerConnected(const FString& PartnerName);
    UFUNCTION(Client, Reliable)   void ClientPartnerDisconnected();
};
