#include "WhiteNoiseConsumer.h"

#include "Kismet/GameplayStatics.h"
#include "CustomShadersDeclarations/Private/ComputeShaderDeclaration.h"


AWhiteNoiseConsumer::AWhiteNoiseConsumer()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Static Mesh"));
}

void AWhiteNoiseConsumer::BeginPlay()
{
	Super::BeginPlay();

	FBoidComputeShaderParameters Parameters(VelocityRenderTarget, PositionRenderTarget);
	Parameters.Range = Range;
	Parameters.AlignScaler = AlignScaler;
	Parameters.CohesionScaler = CohesionScaler;
	Parameters.SeparationScaler = SeparationScaler;

	FComputeShaderManager::Get()->BeginRendering(Parameters);


	// Assuming that the static mesh is already using the material that we're targeting, we create an instance and assign it to it
	// UMaterialInstanceDynamic* MID = static_mesh->CreateAndSetMaterialInstanceDynamic(0);
	// MID->SetTextureParameterValue("VelocityRenderTarget", (UTexture*)VelocityRenderTarget);
	// MID->SetTextureParameterValue("PositionRenderTarget", (UTexture*)PositionRenderTarget);
}

void AWhiteNoiseConsumer::BeginDestroy()
{
	FComputeShaderManager::Get()->EndRendering();
	Super::BeginDestroy();

}

void AWhiteNoiseConsumer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FBoidComputeShaderParameters Parameters(VelocityRenderTarget, PositionRenderTarget);
	Parameters.Range = Range;
	Parameters.AlignScaler = AlignScaler;
	Parameters.CohesionScaler = CohesionScaler;
	Parameters.SeparationScaler = SeparationScaler;

	FComputeShaderManager::Get()->UpdateParameters(Parameters);
}

void AWhiteNoiseConsumer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}