#include "ComputeShaderDeclaration.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"


#include "Modules/ModuleManager.h"

#define RT_SIZE 2048
#define NUM_THREADS_PER_GROUP_DIMENSION (RT_SIZE / 256)
#define PIXELS_PER_THREAD 8
#define NUM_THREADS_FOR_Z_DIMENSION 1


class FWhiteNoiseCS : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FWhiteNoiseCS);
	SHADER_USE_PARAMETER_STRUCT(FWhiteNoiseCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
		SHADER_PARAMETER(FVector2f, Dimensions)
		SHADER_PARAMETER(UINT, TimeStamp)
		SHADER_PARAMETER(UINT, NUM_BOIDS)
		SHADER_PARAMETER(float, perceptionRadius)
		SHADER_PARAMETER(float, maxSpeed)
		SHADER_PARAMETER(float, separationWeight)
		SHADER_PARAMETER(float, alignmentWeight)
		SHADER_PARAMETER(float, cohesionWeight)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_FOR_Z_DIMENSION);
		OutEnvironment.SetDefine(TEXT("PIXELS_PER_THREAD"), PIXELS_PER_THREAD);
	}
};

class FComputeShaderEntryPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeShaderEntryPS);
	SHADER_USE_PARAMETER_STRUCT(FComputeShaderEntryPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, VelocityOutputTexture)	// Create UAV
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, VelocityInput)			// Retrieve from the Cached Render Target
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, PositionOutputTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D<float4>, PositionInput)
		SHADER_PARAMETER(FVector2f, TextureSize)
		SHADER_PARAMETER(float, Range)
		SHADER_PARAMETER(float, AlignScaler)
		SHADER_PARAMETER(float, CohesionScaler)
		SHADER_PARAMETER(float, SeparationScaler)
		END_SHADER_PARAMETER_STRUCT()


		// Used to determine which shader permutations to compile
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	// Modifies the compilation environment - this allows us to added custom defines to the shader compilation
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// Preprocessor defines. 
		// This way there is no need to change both C++ and HLSL files if the value for NUM_THREADS_PER_GROUP_DIMENSION is changed
		// the define outlined in the Char argument is set to the value in the Value argument
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_FOR_Z_DIMENSION);
		OutEnvironment.SetDefine(TEXT("PIXELS_PER_THREAD"), PIXELS_PER_THREAD);
	}
};

// This will tell the engine to create the shader and where the shader entry point is.
//                        ShaderType              ShaderPath             Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FComputeShaderEntryPS, "/CustomShaders/ComputeShader.usf", "MainComputeShader", SF_Compute);

//Static members
FComputeShaderManager* FComputeShaderManager::instance = nullptr;

void FComputeShaderManager::BeginRendering(FBoidComputeShaderParameters& Parameters)
{
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	bCachedParamsAreValid = false;

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	if (RendererModule)
	{
		OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FComputeShaderManager::Execute_RenderThread);
	}

	UpdateParameters(Parameters);
}

void FComputeShaderManager::EndRendering()
{
	if (!OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	if (RendererModule)
	{
		RendererModule->GetResolvedSceneColorCallbacks().Remove(OnPostResolvedSceneColorHandle);
	}

	OnPostResolvedSceneColorHandle.Reset();
}

void FComputeShaderManager::UpdateParameters(FBoidComputeShaderParameters& Parameters)
{
	CachedComputeShaderParameters = Parameters;
	bCachedParamsAreValid = true;
}

void FComputeShaderManager::Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	// Create the VElocity output render target if it doesn't exist
	if (!VelocityOutputRenderTarget.IsValid())
	{
		FPooledRenderTargetDesc VelocityComputeShaderOutputDesc(
			FPooledRenderTargetDesc::Create2DDesc(
				CachedComputeShaderParameters.CachedRenderTargetSize,
				CachedComputeShaderParameters.VelocityRenderTarget->GetRenderTargetResource()->TextureRHI->GetFormat(),
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_ShaderResource | TexCreate_UAV,
				false
			)
		);

		VelocityComputeShaderOutputDesc.DebugName = TEXT("ShaderPlugin_VelocityOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, VelocityComputeShaderOutputDesc, VelocityOutputRenderTarget, TEXT("ShaderPlugin_VelocityOutput"));

		if (!VelocityOutputRenderTarget.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("VelocityOutputRenderTarget is not valid"));
			return;
		}
	}

	// Create the Position output render target if it doesn't exist
	if (!PositionOutputRenderTarget.IsValid())
	{
		FPooledRenderTargetDesc PositionComputeShaderOutputDesc(
			FPooledRenderTargetDesc::Create2DDesc(
				CachedComputeShaderParameters.CachedRenderTargetSize,
				CachedComputeShaderParameters.PositionRenderTarget->GetRenderTargetResource()->TextureRHI->GetFormat(),
				FClearValueBinding::None,
				TexCreate_None,
				TexCreate_ShaderResource | TexCreate_UAV,
				false
			)
		);

		PositionComputeShaderOutputDesc.DebugName = TEXT("ShaderPlugin_PositionOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, PositionComputeShaderOutputDesc, PositionOutputRenderTarget, TEXT("ShaderPlugin_PositionOutput"));

		if (!PositionOutputRenderTarget.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("PositionOutputRenderTarget is not valid"));
			return;
		}
	}

	if (!(bCachedParamsAreValid && CachedComputeShaderParameters.VelocityRenderTarget && CachedComputeShaderParameters.PositionRenderTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid cached parameters or render target."));
		return;
	}

	FShaderExecutionParameters ShaderExecutionParameters(RHICmdList, CachedComputeShaderParameters);
	ShaderExecutionParameters.VelocityOutputRenderTarget = VelocityOutputRenderTarget;
	ShaderExecutionParameters.PositionOutputRenderTarget = PositionOutputRenderTarget;

	Draw_RenderThread(ShaderExecutionParameters);
}

void FComputeShaderManager::Draw_RenderThread(const FShaderExecutionParameters& ExecutionParameters)
{

	// Unbind the render targets
	FRHITransitionInfo TransitionInfo;
	TransitionInfo.Resource = ExecutionParameters.RHICmdList.CreateUnorderedAccessView(ExecutionParameters.VelocityOutputRenderTarget->GetRHI());
	TransitionInfo.Type = FRHITransitionInfo::EType::UAV;
	TransitionInfo.AccessBefore = ERHIAccess::Unknown;
	TransitionInfo.AccessAfter = ERHIAccess::UAVCompute;
	ExecutionParameters.RHICmdList.Transition(TransitionInfo);

	// TransitionInfo.Resource = ExecutionParameters.RHICmdList.CreateUnorderedAccessView(ExecutionParameters.PositionOutputRenderTarget->GetRHI());
	// ExecutionParameters.RHICmdList.Transition(TransitionInfo);



	// Create pass parameters for the compute shader
	FComputeShaderEntryPS::FParameters PassParameters;
	PassParameters.VelocityOutputTexture = ExecutionParameters.RHICmdList.CreateUnorderedAccessView(ExecutionParameters.VelocityOutputRenderTarget->GetRHI());
	PassParameters.VelocityInput = ExecutionParameters.CachedComputeShaderParameters.VelocityRenderTarget->GetRenderTargetResource()->TextureRHI;
	// PassParameters.PositionOutputTexture = ExecutionParameters.RHICmdList.CreateUnorderedAccessView(ExecutionParameters.PositionOutputRenderTarget->GetRHI());
	// PassParameters.PositionInput = ExecutionParameters.CachedComputeShaderParameters.PositionRenderTarget->GetRenderTargetResource()->TextureRHI;

	// Check if any render targets are null
	if
	(
			PassParameters.VelocityOutputTexture == nullptr ||
			PassParameters.VelocityInput == nullptr
	)
	{
		UE_LOG(LogTemp, Warning, TEXT("FComputeShaderEntry::RunComputeShader_RenderThread - PassParameters Render Targets are null"));
		return;
	}

	// Complete the pass parameters
	PassParameters.TextureSize = FVector2f(
		ExecutionParameters.CachedComputeShaderParameters.CachedRenderTargetSize.X,
		ExecutionParameters.CachedComputeShaderParameters.CachedRenderTargetSize.Y
	);

	PassParameters.Range = ExecutionParameters.CachedComputeShaderParameters.Range;
	PassParameters.AlignScaler = ExecutionParameters.CachedComputeShaderParameters.AlignScaler;
	PassParameters.CohesionScaler = ExecutionParameters.CachedComputeShaderParameters.CohesionScaler;
	PassParameters.SeparationScaler = ExecutionParameters.CachedComputeShaderParameters.SeparationScaler;



	// Dispatch the compute shader
	TShaderMapRef<FComputeShaderEntryPS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FComputeShaderUtils::Dispatch(
		ExecutionParameters.RHICmdList,
		ComputeShader,
		PassParameters,
		FIntVector(
			FMath::DivideAndRoundUp(
				ExecutionParameters.CachedComputeShaderParameters.CachedRenderTargetSize.X,
				NUM_THREADS_PER_GROUP_DIMENSION * PIXELS_PER_THREAD
			),
			FMath::DivideAndRoundUp(
				ExecutionParameters.CachedComputeShaderParameters.CachedRenderTargetSize.Y,
				NUM_THREADS_PER_GROUP_DIMENSION * PIXELS_PER_THREAD
			),
			NUM_THREADS_FOR_Z_DIMENSION
		)
	);



	// Copy the textures from the compute shader to the render targets
	ExecutionParameters.RHICmdList.CopyTexture(
		ExecutionParameters.VelocityOutputRenderTarget->GetRHI(),
		ExecutionParameters.CachedComputeShaderParameters.VelocityRenderTarget->GetRenderTargetResource()->TextureRHI,
		FRHICopyTextureInfo()
	);

	return;

	ExecutionParameters.RHICmdList.CopyTexture(
		ExecutionParameters.PositionOutputRenderTarget->GetRHI(),
		ExecutionParameters.CachedComputeShaderParameters.PositionRenderTarget->GetRenderTargetResource()->TextureRHI,
		FRHICopyTextureInfo()
	);
}