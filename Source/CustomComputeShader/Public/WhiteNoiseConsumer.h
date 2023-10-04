#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WhiteNoiseConsumer.generated.h"

UCLASS()
class CUSTOMCOMPUTESHADER_API AWhiteNoiseConsumer : public APawn
{
	GENERATED_BODY()

public:

	AWhiteNoiseConsumer();

	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



	UPROPERTY()
	USceneComponent* Root;

	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* static_mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo")
	class UTextureRenderTarget2D* VelocityRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo")
	class UTextureRenderTarget2D* PositionRenderTarget;

	float Range{ 1.0f };
	float AlignScaler{ 1.0f };
	float CohesionScaler{ 1.0f };
	float SeparationScaler{ 1.0f };

protected:

	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;
};