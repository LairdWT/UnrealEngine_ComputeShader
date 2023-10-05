#include "WhiteNoiseConsumer.h"

#include "Kismet/GameplayStatics.h"
#include "CustomShadersDeclarations/Private/ComputeShaderDeclaration.h"


AComputeShaderAgent::AComputeShaderAgent()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Static Mesh"));
}

void AComputeShaderAgent::BeginPlay()
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

void AComputeShaderAgent::BeginDestroy()
{
	FComputeShaderManager::Get()->EndRendering();
	Super::BeginDestroy();

}

void AComputeShaderAgent::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Range = Range + DeltaTime;

	FBoidComputeShaderParameters Parameters(VelocityRenderTarget, PositionRenderTarget);
	Parameters.Range = Range;
	Parameters.AlignScaler = AlignScaler;
	Parameters.CohesionScaler = CohesionScaler;
	Parameters.SeparationScaler = SeparationScaler;

	FComputeShaderManager::Get()->UpdateParameters(Parameters);
}

void AComputeShaderAgent::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}