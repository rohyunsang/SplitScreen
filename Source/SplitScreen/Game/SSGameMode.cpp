// SSGameMode.cpp

#include "Game/SSGameMode.h"
#include "Player/SSPlayerController.h"
#include "Pawn/PartnerSpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerState.h"

ASSGameMode::ASSGameMode()
{
    PlayerControllerClass = ASSPlayerController::StaticClass();
    SpectatorClass = APartnerSpectatorPawn::StaticClass();

    bUseSeamlessTravel = false;
    bPauseable = true;
    bStartPlayersAsSpectators = false;
}

void ASSGameMode::BeginPlay()
{
    Super::BeginPlay();

    ConnectedPlayers.Empty();

    // �������� ȣ��Ʈ�� ��Ͽ� ���� �߰�
    if (APlayerController* PC0 = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ASSPlayerController* SS0 = Cast<ASSPlayerController>(PC0))
        {
            ConnectedPlayers.AddUnique(SS0);
        }
    }

    TryAutoPair();
}

void ASSGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (ASSPlayerController* SS = Cast<ASSPlayerController>(NewPlayer))
    {
        ConnectedPlayers.AddUnique(SS);
        TryAutoPair();
    }
}

void ASSGameMode::Logout(AController* Exiting)
{
    if (ASSPlayerController* SS = Cast<ASSPlayerController>(Exiting))
    {
        // ������ �� ����
        ClearPartner(SS);

        // ���� ���� �ִ� ��뵵 ����
        for (ASSPlayerController* Other : ConnectedPlayers)
        {
            if (Other && Other->GetPartner() == SS)
            {
                ClearPartner(Other);
            }
        }

        ConnectedPlayers.Remove(SS);
    }

    Super::Logout(Exiting);

    // ���� �ο��� ���� ���Ī
    TryAutoPair();
}

bool ASSGameMode::IsPaired(const ASSPlayerController* PC)
{
    return (PC && PC->GetPartner() != nullptr);
}

void ASSGameMode::ClearPartner(ASSPlayerController* PC)
{
    if (!PC) return;
    PC->SetPartner(nullptr);          // ���������� ȣ��(������)
    PC->ClientDisablePartnerView();   // ���� PIP �ݱ�
}

void ASSGameMode::TryAutoPair()
{
    // ��� �ƴ� �� �� �̱�
    ASSPlayerController* A = nullptr;
    ASSPlayerController* B = nullptr;

    for (ASSPlayerController* PC : ConnectedPlayers)
    {
        if (PC && !IsPaired(PC))
        {
            if (!A) A = PC;
            else if (!B) { B = PC; break; }
        }
    }

    if (A && B)
    {
        // ���� ��Ʈ�� ����(Replicated)
        A->SetPartner(B);
        B->SetPartner(A);

        // ���� ���ÿ��� PIP ����(Ŭ��/ȣ��Ʈ ���)
        A->ClientEnablePartnerView();
        B->ClientEnablePartnerView();

        if (GEngine)
        {
            const FString AName = (A->PlayerState ? A->PlayerState->GetPlayerName() : TEXT("A"));
            const FString BName = (B->PlayerState ? B->PlayerState->GetPlayerName() : TEXT("B"));
            GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
                FString::Printf(TEXT("Auto-paired: %s <-> %s"), *AName, *BName));
        }
    }
}
