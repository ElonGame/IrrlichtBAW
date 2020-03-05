#define _IRR_STATIC_LIB_
#include <irrlicht.h>


#include "../../ext/OptiX/OptiXManager.h"

/**
This example just shows a screen which clears to red,
nothing fancy, just to show that Irrlicht links fine
**/
using namespace irr;


/*
The start of the main function starts like in most other example. We ask the
user for the desired renderer and start it up.
*/
int main()
{
	// create device with full flexibility over creation parameters
	// you can add more parameters if desired, check irr::SIrrlichtCreationParameters
	irr::SIrrlichtCreationParameters params;
	params.Bits = 24; //may have to set to 32bit for some platforms
	params.ZBufferBits = 24; //we'd like 32bit here
	params.DriverType = video::EDT_OPENGL; //! Only Well functioning driver, software renderer left for sake of 2D image drawing
	params.WindowSize = core::dimension2d<uint32_t>(1280, 720);
	params.Fullscreen = false;
	params.Vsync = false;
	params.Doublebuffer = true;
	params.Stencilbuffer = false; //! This will not even be a choice soon
	params.AuxGLContexts = 16;
	IrrlichtDevice* device = createDeviceEx(params);

	if (device == 0)
		return 1; // could not create selected driver.

	video::IVideoDriver* driver = device->getVideoDriver();
	auto optixmgr = irr::ext::OptiX::Manager::create(driver);
	if (!optixmgr)
		return 2;
#if 0
	// Specify options for the build. We use default options for simplicity.
	OptixAccelBuildOptions accel_options = {};
	accel_options.buildFlags = OPTIX_BUILD_FLAG_NONE;
	accel_options.operation = OPTIX_BUILD_OPERATION_BUILD;

	// Triangle build input: simple list of three vertices
	const float vertices_data[3][3] =
	{
		{ -0.5f, -0.5f, 0.0f },
		{  0.5f, -0.5f, 0.0f },
		{  0.0f,  0.5f, 0.0f }
	};

	// Allocate and copy device memory for our input triangle vertices
	const uint32_t numVertices = 3u;
	const size_t vertices_size = sizeof(float)*numVertices*3ull;
	cuda::CCUDAHandler::GraphicsAPIObjLink<video::IGPUBuffer> vertices_link;
	vertices_link.obj = core::smart_refctd_ptr<video::IGPUBuffer>(driver->createFilledDeviceLocalGPUBufferOnDedMem(vertices_size,vertices_data), core::dont_grab);
	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::registerBuffer(&vertices_link,CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY)))
		return 3;

	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsMapResources(1u, &vertices_link.cudaHandle, optixmgr->stream[0])))
		return 4;

	CUdeviceptr d_vertices = 0;
	{
		size_t tmp = vertices_size;
		if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsResourceGetMappedPointer_v2(&d_vertices, &tmp, vertices_link.cudaHandle)))
			return 5;
	}

	// Populate the build input struct with our triangle data as well as
	// information about the sizes and types of our data
	const uint32_t triangle_input_flags[1] = { OPTIX_GEOMETRY_FLAG_NONE };
	OptixBuildInput triangle_input = {};
	triangle_input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
	triangle_input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
	triangle_input.triangleArray.numVertices = numVertices;
	triangle_input.triangleArray.vertexBuffers = &d_vertices;
	triangle_input.triangleArray.flags = triangle_input_flags;
	triangle_input.triangleArray.numSbtRecords = 1;

	// Query OptiX for the memory requirements for our GAS 
	OptixAccelBufferSizes gas_buffer_sizes;
	optixAccelComputeMemoryUsage(
		optixmgr->optixContext[0],         // The device context we are using
		&accel_options,
		&triangle_input, // Describes our geometry
		1,               // Number of build inputs, could have multiple
		&gas_buffer_sizes);

	// Allocate device memory for the scratch space buffer as well
	// as the GAS itself
	CUdeviceptr d_temp_buffer_gas, d_gas_output_buffer;
	cuda::CCUDAHandler::cuda.pcuMemAlloc_v2(&d_temp_buffer_gas,gas_buffer_sizes.tempSizeInBytes);
	cuda::CCUDAHandler::cuda.pcuMemAlloc_v2(&d_gas_output_buffer,gas_buffer_sizes.outputSizeInBytes);

	// Now build the GAS
	OptixTraversableHandle gas_handle = 0u;
	optixAccelBuild(
		optixmgr->optixContext[0],         // The device context we are using
		optixmgr->stream[0],           // CUDA stream
		&accel_options,
		&triangle_input,
		1,           // num build inputs
		d_temp_buffer_gas,
		gas_buffer_sizes.tempSizeInBytes,
		d_gas_output_buffer,
		gas_buffer_sizes.outputSizeInBytes,
		&gas_handle, // Output handle to the struct
		nullptr,     // emitted property list
		0);         // num emitted properties

	if (!cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::cuda.pcuGraphicsUnmapResources(1, &vertices_link.cudaHandle, optixmgr->stream[0])))
		return 6;

	// We can now free scratch space used during the build
	cuda::CCUDAHandler::cuda.pcuMemFree_v2(d_temp_buffer_gas);
#endif
	// Pipeline options must be consistent for all modules used in a
	// single pipeline
	OptixPipelineCompileOptions pipeline_compile_options = {};
	pipeline_compile_options.usesMotionBlur = false;
	pipeline_compile_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;  // TODO: should be OPTIX_EXCEPTION_FLAG_STACK_OVERFLOW;

	// This option is important to ensure we compile code which is optimal
	// for our scene hierarchy. We use a single GAS � no instancing or
	// multi-level hierarchies
	pipeline_compile_options.traversableGraphFlags =
		OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_GAS | OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
	
	/*
	// Our device code uses 3 payload registers (r,g,b output value)
	pipeline_compile_options.numPayloadValues = 3;
	*/
	pipeline_compile_options.numPayloadValues = 2;
	pipeline_compile_options.numAttributeValues = 2;

	// This is the name of the param struct variable in our device code
	pipeline_compile_options.pipelineLaunchParamsVariableName = "params";

	std::string log;
	std::string ptx;
	{
		auto file = device->getFileSystem()->createAndOpenFile("../optixTriangle.cu");
		bool ok = cuda::CCUDAHandler::defaultHandleResult(cuda::CCUDAHandler::compileDirectlyToPTX<io::IReadFile*>(ptx, file, nullptr, nullptr, {"--std=c++14",cuda::CCUDAHandler::getCommonVirtualCUDAArchitecture(),"-dc","-use_fast_math","-ewp"}, &log));
		printf("NVRTC LOG:\n%s\n", log.c_str());
		if (!ok)
			return 7;
		file->drop();
	}
	size_t sizeof_log = 1024u * 256u;
	log.resize(sizeof_log);


	OptixModule module = nullptr; // The output module
	{
		// Default options for our module.
		OptixModuleCompileOptions module_compile_options = {};
		module_compile_options.maxRegisterCount = OPTIX_COMPILE_DEFAULT_MAX_REGISTER_COUNT;
		module_compile_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
		module_compile_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_LINEINFO;
		optixModuleCreateFromPTX(
			optixmgr->optixContext[0],         // The device context we are using
			&module_compile_options,
			&pipeline_compile_options,
			ptx.c_str(),
			ptx.size(),
			log.data(),
			&sizeof_log,
			&module);
	}
	
    //
    // Create program groups, including NULL miss and hitgroups
    //
    OptixProgramGroup raygen_prog_group   = nullptr;
    OptixProgramGroup miss_prog_group     = nullptr;
    OptixProgramGroup hitgroup_prog_group = nullptr;
    {
        OptixProgramGroupOptions program_group_options   = {}; // Initialize to zeros

        OptixProgramGroupDesc raygen_prog_group_desc  = {}; //
        raygen_prog_group_desc.kind                     = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        raygen_prog_group_desc.raygen.module            = module;
        raygen_prog_group_desc.raygen.entryFunctionName = "__raygen__draw_solid_color";
        size_t sizeof_log = sizeof( log );
        optixProgramGroupCreate(
                    optixmgr->optixContext[0],
                    &raygen_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    log.data(),
                    &sizeof_log,
                    &raygen_prog_group
                    );

        // Leave miss group's module and entryfunc name null
        OptixProgramGroupDesc miss_prog_group_desc = {};
        miss_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
        sizeof_log = sizeof( log );
        optixProgramGroupCreate(
                    optixmgr->optixContext[0],
                    &miss_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    log.data(),
                    &sizeof_log,
                    &miss_prog_group
                    );

        // Leave hit group's module and entryfunc name null
        OptixProgramGroupDesc hitgroup_prog_group_desc = {};
        hitgroup_prog_group_desc.kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        sizeof_log = sizeof( log );
        optixProgramGroupCreate(
                    optixmgr->optixContext[0],
                    &hitgroup_prog_group_desc,
                    1,   // num program groups
                    &program_group_options,
                    log.data(),
                    &sizeof_log,
                    &hitgroup_prog_group
                    );
    }

    //
    // Link pipeline
    //
    OptixPipeline pipeline = nullptr;
    {
        OptixProgramGroup program_groups[] = { raygen_prog_group };

        OptixPipelineLinkOptions pipeline_link_options = {};
        pipeline_link_options.maxTraceDepth          = 5;
        pipeline_link_options.debugLevel             = OPTIX_COMPILE_DEBUG_LEVEL_FULL;
        pipeline_link_options.overrideUsesMotionBlur = false;
        size_t sizeof_log = sizeof( log );
        optixPipelineCreate(
                    optixmgr->optixContext[0],
                    &pipeline_compile_options,
                    &pipeline_link_options,
                    program_groups,
                    sizeof( program_groups ) / sizeof( program_groups[0] ),
                    log.data(),
                    &sizeof_log,
                    &pipeline
                    );
    }

	// release all resources
	/// optixAccelDestroy?
	//cuda::CCUDAHandler::cuda.pcuMemFree_v2(d_gas_output_buffer);

	device->drop();

	return 0;
}