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

    UPROPERTY()
    float SpringArmLength = 400.0f;
};

UCLASS()
class SPLITSCREEN_API ASSCameraViewProxy : public AActor
{
	GENERATED_BODY()
	
public:
    ASSCameraViewProxy();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** �������� ī�޶� �ҽ��� �� PlayerController ���� */
    UFUNCTION(BlueprintCallable, Category = "SS|CameraProxy")
    void SetSourcePC(APlayerController* InPC);

    /** ���� �Լ�: �������� �÷��̾� �ε����� �ҽ� ���� (��: 0 = �������� ����) */
    UFUNCTION(BlueprintCallable, Category = "SS|CameraProxy")
    void SetSourceFromPlayerIndex(int32 PlayerIndex = 0);

    /** Ŭ�󿡼� ���� �� �ִ� ������ ī�޶� ���� */
    UFUNCTION(BlueprintPure, Category = "SS|CameraProxy")
    const FRepCamInfo& GetReplicatedCamera() const { return RepCam; }

protected:
    /** �����Ǵ� ī�޶� ����(������ ä��� Ŭ�� ����) */
    UPROPERTY(Replicated)
    FRepCamInfo RepCam;

    /** ���������� �ǹ� �ִ� �ҽ� PC (Ŭ�� �������� ����) */
    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> SourcePC;

};
