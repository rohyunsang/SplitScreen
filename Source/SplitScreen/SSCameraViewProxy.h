// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SSCameraViewProxy.generated.h"

USTRUCT(BlueprintType)
struct FRepCamInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FOV = 90.f;
};

UCLASS()
class SPLITSCREEN_API ASSCameraViewProxy : public AActor
{
	GENERATED_BODY()
	
public:
    ASSCameraViewProxy();

protected:
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void UpdateServerCamera();
    void ApplyReplicatedCamera();

public:
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SplitScreen|Camera")
    void SetSourcePC(APlayerController* InPC);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "SplitScreen|Camera")
    void SetSourceFromPlayerIndex(int32 PlayerIndex = 0);

    UFUNCTION()
    void OnRep_RepCam();

protected:
    // RepCam이 변경될 때 OnRep_RepCam 호출하도록 연결
    UPROPERTY(ReplicatedUsing = OnRep_RepCam, BlueprintReadOnly)
    FRepCamInfo RepCam;


private:
    UPROPERTY()
    TWeakObjectPtr<APlayerController> SourcePC;

};
