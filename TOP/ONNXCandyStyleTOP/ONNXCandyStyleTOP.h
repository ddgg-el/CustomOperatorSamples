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

#include "TOP_CPlusPlusBase.h"
#include "cuda_runtime.h"
#include <array>
#include <string>
#include <onnxruntime_c_api.h>

using namespace TD;

class ONNXCandyStyleTOP : public TOP_CPlusPlusBase
{
public:
	ONNXCandyStyleTOP(const OP_NodeInfo *info, TOP_Context *context);
	virtual ~ONNXCandyStyleTOP();

	virtual void		getGeneralInfo(TOP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
	//virtual bool		getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void* reserved) override;


	virtual void		execute(TOP_Output*, const OP_Inputs*, void* reserved) override;

	virtual int32_t		getNumInfoCHOPChans(void* reserved) override;
	virtual void		getInfoCHOPChan(int32_t index,
										OP_InfoCHOPChan *chan,
										void* reserved) override;

	virtual bool		getInfoDATSize(OP_InfoDATSize *infoSize, void* reserved) override;
	virtual void		getInfoDATEntries(int32_t index,
											int32_t nEntries,
											OP_InfoDATEntries *entries,
											void* reserved) override;

	virtual void		getErrorString(OP_String *error, void* reserved) override;

	virtual void		setupParameters(OP_ParameterManager *manager, void* reserved) override;
	virtual void		pulsePressed(const char *name, void* reserved) override;

private:
	bool				checkORTStatus(OrtStatus* status, std::string* err);
	bool				allocCUDAAndValue(void** cudaMemory, OrtValue** value, size_t size, const std::array<int64_t, 4>& shape);

	// We don't need to store this pointer, but we do for the example.
	// The OP_NodeInfo class store information about the node that's using
	// this instance of the class (like its name).
	const OP_NodeInfo*	myNodeInfo;

	cudaSurfaceObject_t	myInputSurface = 0;
	cudaSurfaceObject_t	myOutputSurface = 0;
	cudaStream_t		myStream = 0;

	TOP_Context*		myContext = nullptr;
	std::string			myError;
	const OrtApi*		myORT = nullptr;
	OrtEnv*				myEnv = nullptr;
	OrtSessionOptions*	mySessionOptions = nullptr;
	OrtSession*			mySession = nullptr;
	OrtRunOptions*		myRunOptions = nullptr;
	void*				mySourceData = nullptr;
	OrtValue*			mySourceTensor = nullptr;
	void*				myDestData = nullptr;
	OrtValue*			myDestTensor = nullptr;
	size_t				myInputCount = 0;
	size_t				myOutputCount = 0;

};
