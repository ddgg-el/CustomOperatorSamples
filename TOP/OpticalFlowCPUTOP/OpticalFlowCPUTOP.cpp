/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "OpticalFlowCPUTOP.h"
#include "Parameters.h"

#include <cassert>
#include <opencv2/core.hpp>
#include <opencv2/video/tracking.hpp>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
void
FillTOPPluginInfo(TD::TOP_PluginInfo *info)
{
	// This must always be set to this constant
	info->apiVersion = TD::TOPCPlusPlusAPIVersion;

	// Change this to change the executeMode behavior of this plugin.
	info->executeMode = TD::TOP_ExecuteMode::CPUMem;

	// For more information on OP_CustomOPInfo see CPlusPlus_Common.h
	TD::OP_CustomOPInfo& customInfo = info->customOPInfo;

	// Unique name of the node which starts with an upper case letter, followed by lower case letters or numbers
	customInfo.opType->setString("Opticalflowcpu");
	// English readable name
	customInfo.opLabel->setString("Optical Flow CPU");
	// Information of the author of the node
	customInfo.authorName->setString("Author Name");
	customInfo.authorEmail->setString("email@email.ca");

	// This TOP takes one input
	customInfo.minInputs = 1;
	customInfo.maxInputs = 1;
}

DLLEXPORT
TD::TOP_CPlusPlusBase*
CreateTOPInstance(const TD::OP_NodeInfo* info, TD::TOP_Context* context)
{
	// Return a new instance of your class every time this is called.
	// It will be called once per TOP that is using the .dll
	return new OpticalFlowCPUTOP(info, context);
}

DLLEXPORT
void
DestroyTOPInstance(TD::TOP_CPlusPlusBase* instance, TD::TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (OpticalFlowCPUTOP*)instance;
}

};


OpticalFlowCPUTOP::OpticalFlowCPUTOP(const TD::OP_NodeInfo*, TD::TOP_Context *context) :
	myFrame{ new cv::Mat() }, myPrev{ new cv::Mat() }, myFlow{ new cv::Mat() },
	myContext(context), myExecuteCount(0), myPrevDownRes(nullptr)
{
}

OpticalFlowCPUTOP::~OpticalFlowCPUTOP()
{
	delete myFrame;
	delete myPrev;
	delete myFlow;
}

void
OpticalFlowCPUTOP::getGeneralInfo(TD::TOP_GeneralInfo* ginfo, const TD::OP_Inputs*, void*)
{
	ginfo->cookEveryFrameIfAsked = false;
	ginfo->inputSizeIndex = 0;
}

void
OpticalFlowCPUTOP::execute(TD::TOP_Output* output, const TD::OP_Inputs* inputs, void*)
{
	myExecuteCount++;

	using namespace cv;

	inputToMat(inputs);
	if (myFrame->empty())
		return;

	if (!myPrevDownRes)
		return;

	TD::TOP_UploadInfo info;
	info.textureDesc.width = myPrevDownRes->textureDesc.width;
	info.textureDesc.height = myPrevDownRes->textureDesc.height;
	info.textureDesc.texDim = TD::OP_TexDim::e2D;
	info.textureDesc.pixelFormat = TD::OP_PixelFormat::RG32Float;
	info.colorBufferIndex = 0;

	Size outSize = Size(info.textureDesc.width, info.textureDesc.height);
	resize(*myFrame, *myFrame, outSize);
	
	if (myPrev->empty() || myPrev->size() != outSize)
	{
		*myPrev = std::move(*myFrame);
		return;
	}

	bool usegaussianfilter = myParms.evalUsegaussianfilter(inputs);
	bool usepreviousflow = myParms.evalUsepreviousflow(inputs);
	int myFlags = usegaussianfilter ? cv::OPTFLOW_FARNEBACK_GAUSSIAN : 0;
	myFlags |= usepreviousflow ? cv::OPTFLOW_USE_INITIAL_FLOW : 0;

	if (myFlow->empty() || myFlow->size() != outSize)
	{
		*myFlow = Mat(outSize, CV_32FC2);
		myFlags &= ~OPTFLOW_USE_INITIAL_FLOW;
	}

	calcOpticalFlowFarneback(
		*myPrev, *myFrame, *myFlow, myParms.evalPyramidscale(inputs), myParms.evalNumlevels(inputs), myParms.evalWindowsize(inputs),
		myParms.evalIterations(inputs), myParms.evalPolyn(inputs), myParms.evalPolysigma(inputs), myFlags
	);

	*myPrev = std::move(*myFrame);

	cvMatToOutput(*myFlow, output, info);
}

void
OpticalFlowCPUTOP::setupParameters(TD::OP_ParameterManager* manager, void*)
{
	myParms.setup(manager);
}

void 
OpticalFlowCPUTOP::cvMatToOutput(const cv::Mat& M, TD::TOP_Output* out, TD::TOP_UploadInfo info) const
{
	size_t	height = info.textureDesc.height;
	size_t	width = info.textureDesc.width;
	size_t imgsize = 2 * height * width * sizeof(float);

	TD::OP_SmartRef<TD::TOP_Buffer> buf = myContext->createOutputBuffer(imgsize, TD::TOP_BufferFlags::None, nullptr);

	float*	pixel = static_cast<float*>(buf->data);
	
	cv::flip(M, M, 0);
	float*	data = static_cast<float*>(static_cast<void*>(M.data));

	memcpy(pixel, data, 2 * height * width * sizeof(float));

	out->uploadBuffer(&buf, info, nullptr);
}

void 
OpticalFlowCPUTOP::inputToMat(const TD::OP_Inputs* in)
{
	const TD::OP_TOPInput*	top = in->getInputTOP(0);
	if (!top)
        {
                *myFrame = cv::Mat();
		return;
        }

	TD::OP_TOPInputDownloadOptions	opts;
	opts.verticalFlip = true;
	opts.pixelFormat = TD::OP_PixelFormat::BGRA8Fixed;
	TD::OP_SmartRef<TD::OP_TOPDownloadResult> downRes = top->downloadTexture(opts, nullptr);

	if (!downRes)
	{
		*myFrame = cv::Mat();
		return;
	}

	if (myPrevDownRes)
	{
		uint8_t* pixel = (uint8_t*)myPrevDownRes->getData();


		if (!pixel)
		{
			*myFrame = cv::Mat();
			return;
		}

		int height = top->textureDesc.height;
		int width = top->textureDesc.width;


		*myFrame = cv::Mat(height, width, CV_8UC1);
		uint8_t* data = (uint8_t*)myFrame->data;
		for (int i = 0; i < height; i += 1) {
			for (int j = 0; j < width; j += 1) {
				int pixelN = i * width + j;
				int index = 4 * pixelN + static_cast<int>(myParms.evalChannel(in));
				data[pixelN] = pixel[index];
			}
		}
	}
	myPrevDownRes = std::move(downRes);

	
}
