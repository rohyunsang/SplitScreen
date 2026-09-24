// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicSplitScreenCameraProxy.generated.h"

class APlayerController;

/** Camera state replicated for the secondary split-screen view. */
USTRUCT(BlueprintType)
struct DYNAMICSPLITSCREEN_API FDynamicSplitScreenCameraInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	float FOV = 90.f;

	/** Sender's SpringArm TargetArmLength. The receiver rebuilds the camera from anchor + rotation + arm length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	float ArmLength = 0.f;

	/**
	 * True while the sender views through its own pawn's camera (no other view target, no view target blend).
	 * False when a level camera is in use (e.g. Fixed / Follow camera volumes): the receiver then uses Location
	 * directly instead of rebuilding the camera from the pawn's spring arm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamic Split Screen")
	bool bUsesPawnCamera = true;
};

/**
 * Replicates one player's camera (location / rotation / FOV / arm length) to the other machine.
 *
 * Spawned by ADynamicSplitScreenGameMode:
 *   - Server proxy (no owner): the listen server fills it from the host's camera every tick.
 *   - Client proxy (owned by a remote PlayerController): the owning client sends its camera via RPC at SendRate.
 *
 * UDynamicSplitScreenSubsystem reads the proxy of the *other* player to drive the secondary view.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class DYNAMICSPLITSCREEN_API ADynamicSplitScreenCameraProxy : public AActor
{
	GENERATED_BODY()

public:
	ADynamicSplitScreenCameraProxy();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: the PlayerController whose camera feeds a server proxy. */
	void SetSourcePC(APlayerController* InPC);

	UFUNCTION(BlueprintPure, Category = "Dynamic Split Screen")
	const FDynamicSplitScreenCameraInfo& GetReplicatedCamera() const { return RepCam; }

	bool IsServerProxy() const { return bIsServerProxy; }
	void SetIsServerProxy(bool bInServerProxy) { bIsServerProxy = bInServerProxy; }

	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerUpdateClientCamera(const FDynamicSplitScreenCameraInfo& NewCam);

	/** How often (Hz) the owning client sends its camera to the server. */
	UPROPERTY(EditDefaultsOnly, Category = "Dynamic Split Screen", meta = (ClampMin = "10.0", ClampMax = "120.0"))
	float SendRate = 60.f;

protected:
	UPROPERTY(Replicated)
	FDynamicSplitScreenCameraInfo RepCam;

	UPROPERTY(Replicated)
	bool bIsServerProxy = false;

	/** Server only, not replicated */
	TWeakObjectPtr<APlayerController> SourcePC;

private:
	static FDynamicSplitScreenCameraInfo CaptureCamera(const APlayerController* PC);

	float ClientSendTimer = 0.f;
};
