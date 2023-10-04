#include "ComputeShaderDeclaration.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"


#include "Modules/ModuleManager.h"

#define NUM_THREADS_PER_GROUP_DIMENSION 32


// Internal class thet holds the parameters and connects the HLSL Shader to the engine

class FWhiteNoiseCS : public FGlobalShader
{
public:

	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FWhiteNoiseCS);

	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FWhiteNoiseCS, FGlobalShader);


	/// DECLARATION OF THE PARAMETER STRUCTURE
	/// The parameters must match the parameters in the HLSL code
	/// For each parameter, provide the C++ type, and the name (Same name used in HLSL code)

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

	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	//Modifies the compilations environment of the shader
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};

// This will tell the engine to create the shader and where the shader entry point is.
//                        ShaderType              ShaderPath             Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FWhiteNoiseCS, "/CustomShaders/WhiteNoiseCS.usf", "MainComputeShader", SF_Compute);

//Static members
FComputeShaderManager* FComputeShaderManager::instance = nullptr;

//Begin the execution of the compute shader each frame
void FComputeShaderManager::BeginRendering()
{
	// Check for an already initialized and valid handle
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	bCachedParamsAreValid = false;

	// Get the Renderer Module
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	// Check if the RendererModule pointer is valid
	if (RendererModule)
	{
		// Safely add our callback to the module
		OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FComputeShaderManager::Execute_RenderThread);
	}
}

//Stop the compute shader execution
void FComputeShaderManager::EndRendering()
{
	// Check for a valid handle
	if (!OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	// Get the Renderer Module
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	// Check if the RendererModule pointer is valid
	if (RendererModule)
	{
		// Safely remove our callback from the module
		RendererModule->GetResolvedSceneColorCallbacks().Remove(OnPostResolvedSceneColorHandle);
	}

	OnPostResolvedSceneColorHandle.Reset();
}


//Update the parameters by a providing an instance of the Parameters structure used by the shader manager
void FComputeShaderManager::UpdateParameters(FWhiteNoiseCSParameters& params)
{
	cachedParams = params;
	bCachedParamsAreValid = true;
}



/// Creates an instance of the shader type parameters structure and fills it using the cached shader manager parameter structure
/// Gets a reference to the shader type from the global shaders map
/// Dispatches the shader using the parameter structure instance

void FComputeShaderManager::Execute_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	// Basic check for cachedParams validity and the RenderTarget
	if (!(bCachedParamsAreValid && cachedParams.RenderTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid cached parameters or render target."));
		return;
	}

	// Render Thread Assertion
	check(IsInRenderingThread());

	// Check and initialize the ComputeShaderOutput
	if (!ComputeShaderOutput.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ComputeShaderOutput is not valid. Initializing..."));
		FPooledRenderTargetDesc ComputeShaderOutputDesc(FPooledRenderTargetDesc::Create2DDesc(cachedParams.GetRenderTargetSize(), cachedParams.RenderTarget->GetRenderTargetResource()->TextureRHI->GetFormat(), FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		ComputeShaderOutputDesc.DebugName = TEXT("WhiteNoiseCS_Output_RenderTarget");
		GRenderTargetPool.FindFreeElement(RHICmdList, ComputeShaderOutputDesc, ComputeShaderOutput, TEXT("WhiteNoiseCS_Output_RenderTarget"));

		// Double check after initialization
		if (!ComputeShaderOutput.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to initialize ComputeShaderOutput."));
			return;
		}
	}

	// Specify the resource transition
	FRHITransitionInfo TransitionInfo;
	TransitionInfo.Resource = RHICmdList.CreateUnorderedAccessView(ComputeShaderOutput->GetRHI());
	TransitionInfo.Type = FRHITransitionInfo::EType::UAV;
	TransitionInfo.AccessBefore = ERHIAccess::Unknown;
	TransitionInfo.AccessAfter = ERHIAccess::UAVCompute;

	RHICmdList.Transition(TransitionInfo);

	// Fill the shader parameters structure
	FWhiteNoiseCS::FParameters PassParameters;
	PassParameters.OutputTexture = RHICmdList.CreateUnorderedAccessView(ComputeShaderOutput->GetRHI());
	if (!PassParameters.OutputTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create UAV for ComputeShaderOutput."));
		return;
	}

	PassParameters.Dimensions = FVector2f(cachedParams.GetRenderTargetSize().X, cachedParams.GetRenderTargetSize().Y);
	PassParameters.TimeStamp = cachedParams.TimeStamp;

	// Get a reference to our shader type
	TShaderMapRef<FWhiteNoiseCS> whiteNoiseCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	if (!whiteNoiseCS.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get valid shader reference."));
		return;
	}

	// Dispatch the compute shader
	FComputeShaderUtils::Dispatch(RHICmdList, whiteNoiseCS, PassParameters,
		FIntVector(FMath::DivideAndRoundUp(cachedParams.GetRenderTargetSize().X, NUM_THREADS_PER_GROUP_DIMENSION),
			FMath::DivideAndRoundUp(cachedParams.GetRenderTargetSize().Y, NUM_THREADS_PER_GROUP_DIMENSION), 1));

	// Check validity before copying texture
	if (!ComputeShaderOutput->GetRHI() || !cachedParams.RenderTarget->GetRenderTargetResource() || !cachedParams.RenderTarget->GetRenderTargetResource()->TextureRHI)
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid resources for texture copy."));
		return;
	}

	// Copy shader's output to the render target provided by the client
	RHICmdList.CopyTexture(ComputeShaderOutput->GetRHI(), cachedParams.RenderTarget->GetRenderTargetResource()->TextureRHI, FRHICopyTextureInfo());
}