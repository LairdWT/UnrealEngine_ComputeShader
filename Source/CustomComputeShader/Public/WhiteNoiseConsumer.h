#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WhiteNoiseConsumer.generated.h"

UCLASS()
class CUSTOMCOMPUTESHADER_API AComputeShaderAgent : public APawn
{
	GENERATED_BODY()

public:

	AComputeShaderAgent();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo | Flock")
	float Range{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo | Flock")
	float AlignScaler{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo | Flock")
	float CohesionScaler{ 1.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ShaderDemo | Flock")
	float SeparationScaler{ 1.0f };

protected:

	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;
};