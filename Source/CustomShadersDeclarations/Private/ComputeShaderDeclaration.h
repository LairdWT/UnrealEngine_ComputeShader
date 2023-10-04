#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

//This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.
struct  FWhiteNoiseCSParameters
{
	UTextureRenderTarget2D* RenderTarget{};


	FIntPoint GetRenderTargetSize() const
	{
		return CachedRenderTargetSize;
	}

	FWhiteNoiseCSParameters() { }
	FWhiteNoiseCSParameters(UTextureRenderTarget2D* IORenderTarget)
		: RenderTarget(IORenderTarget)
	{
		CachedRenderTargetSize = RenderTarget ? FIntPoint(RenderTarget->SizeX, RenderTarget->SizeY) : FIntPoint::ZeroValue;
	}

private:
	FIntPoint CachedRenderTargetSize{};
public:
	uint32 TimeStamp{};
};

struct FBoidComputeShaderParameters
{
	// Shader struct Parameters
	UTextureRenderTarget2D* VelocityRenderTarget{ nullptr };
	UTextureRenderTarget2D* PositionRenderTarget{ nullptr };
	FIntPoint CachedRenderTargetSize{};
	float Range{ 0.0f };
	float AlignScaler{ 0.0f };
	float CohesionScaler{ 0.0f };
	float SeparationScaler{ 0.0f };

	FBoidComputeShaderParameters() { }
	FBoidComputeShaderParameters(UTextureRenderTarget2D* InRenderTarget, UTextureRenderTarget2D* InRenderTarget2)
		: VelocityRenderTarget(InRenderTarget), PositionRenderTarget(InRenderTarget2)
	{
		CachedRenderTargetSize = VelocityRenderTarget ? FIntPoint(VelocityRenderTarget->SizeX, VelocityRenderTarget->SizeY) : FIntPoint::ZeroValue;
	}
};


// Parameters passed to the shader entry point
struct FShaderExecutionParameters
{
	FRHICommandListImmediate& RHICmdList;
	FBoidComputeShaderParameters& CachedComputeShaderParameters;
	TRefCountPtr<IPooledRenderTarget> VelocityOutputRenderTarget{};
	TRefCountPtr<IPooledRenderTarget> PositionOutputRenderTarget{};

	FShaderExecutionParameters(FRHICommandListImmediate& InRHICmdList, FBoidComputeShaderParameters& InCachedComputeShaderParameters)
		: RHICmdList(InRHICmdList)
		, CachedComputeShaderParameters(InCachedComputeShaderParameters)
	{
		// Render Targets added after initialization
	}
};


/// <summary>
/// A singleton Shader Manager for our Shader Type
/// </summary>
class CUSTOMSHADERSDECLARATIONS_API FComputeShaderManager
{
public:
	//Get the instance
	static FComputeShaderManager* Get()
	{
		if (!instance)
			instance = new FComputeShaderManager();
		return instance;
	};

	// Call this when you want to hook onto the renderer and start executing the compute shader. The shader will be dispatched once per frame.
	void BeginRendering(FBoidComputeShaderParameters& Parameters);

	// Stops compute shader execution
	void EndRendering();

	// Call this whenever you have new parameters to share.
	void UpdateParameters(FBoidComputeShaderParameters& Parameters);

	void Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures);
	void Draw_RenderThread(const FShaderExecutionParameters& ExecutionParameters);
	
private:

	FComputeShaderManager() = default;

	//The singleton instance
	static FComputeShaderManager* instance;

	// The function that will be executed each frame by the renderer
	FDelegateHandle OnPostResolvedSceneColorHandle{};

	//Cached Shader Parameters
	FBoidComputeShaderParameters CachedComputeShaderParameters{};

	//Whether we have cached parameters to pass to the shader or not
	volatile bool bCachedParamsAreValid{false};

	// Render Target outputs
	TRefCountPtr<IPooledRenderTarget> VelocityOutputRenderTarget{ nullptr };
	TRefCountPtr<IPooledRenderTarget> PositionOutputRenderTarget{ nullptr };
};
