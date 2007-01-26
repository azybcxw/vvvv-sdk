//////project name
//ARTK+Tracker

//////description
//freeframe plugin.
//implemenation of the ARToolkitPlus library
//for tracking of AR markers

//////licence
//GNU Lesser General Public License (LGPL)
//english: http://www.gnu.org/licenses/lgpl.html
//german: http://www.gnu.de/lgpl-ger.html

//////language/ide
//c++/codeblocks

//////dependencies
//ARToolkitPlus library:
//http://studierstube.icg.tu-graz.ac.at/handheld_ar/artoolkitplus.php

//////initial author
//joreg -> joreg@gmx.at

//includes
#include "ARTK+Tracker.h"

//ARToolkits
#include "ARToolKitPlus/TrackerSingleMarkerImpl.h"



#include <string>
using std::string;

// Plugin Globals
PlugInfoStruct GPlugInfo;
PlugExtendedInfoStruct GPlugExtInfo;
ParamConstsStruct GParamConstants[NUM_PARAMS];
OutputConstsStruct GOutputConstants[NUM_OUTPUTS];

ARToolKitPlus::TrackerSingleMarker* FTracker;
string FCameraFile;

LPVOID instantiate(VideoInfoStruct* pVideoInfo)
{

	// Create local pointer to plugObject
	plugClass *pPlugObj;
	// create new instance of plugClass
	pPlugObj = new plugClass;

	// make a copy of the VideoInfoStruct
	pPlugObj->FVideoInfo.frameWidth = pVideoInfo->frameWidth;
	pPlugObj->FVideoInfo.frameHeight = pVideoInfo->frameHeight;
	pPlugObj->FVideoInfo.bitDepth = pVideoInfo->bitDepth;

	// this shouldn't happen if the host is checking the capabilities properly
	pPlugObj->FVideoMode = pPlugObj->FVideoInfo.bitDepth;
	if (pPlugObj->FVideoMode >2 || pPlugObj->FVideoMode < 0) {
	  return (LPVOID) FF_FAIL;
	}

	pPlugObj->init();

	// Russell - return pointer to the plugin instance object we have created

	// return pointer to object cast as LPVOID
	return (LPVOID) pPlugObj;
}

DWORD deInstantiate(LPVOID instanceID)
{
	// declare pPlugObj - pointer to this instance
	plugClass *pPlugObj;

	// typecast LPVOID into pointer to a plugClass
	pPlugObj = (plugClass*) instanceID;

	delete pPlugObj; // todo: ? success / fail?

	return FF_SUCCESS;
}

DWORD initialise()
{
    // populate the parameters constants structs
    GParamConstants[0].Type = 0;
    GParamConstants[1].Type = 0;
    GParamConstants[2].Type = 10;
    GParamConstants[3].Type = 100;
    GParamConstants[4].Type = 22;

	GParamConstants[0].Default = 1.0f;
	GParamConstants[1].Default = 1.0f;
	GParamConstants[2].Default = 0.8f;

    string camerafilemask = "Camera Files (*.dat, *.cal)|*.dat;*.cal";
    int i = (int)&camerafilemask[0];
    float* fp = (float*)&i;
 	GParamConstants[3].Default = *fp;

 	string markerfilemask = "Pattern Files (*.pat)|*.pat";
 	i = (int)&markerfilemask[0];
    fp = (float*)&i;
 	GParamConstants[4].Default = *fp;

	char tempName0[17] = "Use BCH Marker";
	char tempName1[17] = "Thin Border";
	char tempName2[17] = "Marker Width";
	char tempName3[17] = "Camera File";
	char tempName4[17] = "Marker Files";

	memcpy(GParamConstants[0].Name, tempName0, 16);
	memcpy(GParamConstants[1].Name, tempName1, 16);
	memcpy(GParamConstants[2].Name, tempName2, 16);
	memcpy(GParamConstants[3].Name, tempName3, 16);
	memcpy(GParamConstants[4].Name, tempName4, 16);

    // populate the output structs
    GOutputConstants[0].Type = 0;
    GOutputConstants[1].Type = 10;
    GOutputConstants[2].Type = 10;

	char outName0[17] = "ID";
	char outName1[17] = "ModelView";
	char outName2[17] = "Projection";

	memcpy(GOutputConstants[0].Name, outName0, 16);
	memcpy(GOutputConstants[1].Name, outName1, 16);
	memcpy(GOutputConstants[2].Name, outName2, 16);


	return FF_SUCCESS;
}

DWORD deInitialise()
{
	return FF_SUCCESS;
}

DWORD getNumParameters()
{
	return NUM_PARAMS;
}

DWORD getNumOutputs()
{
	return NUM_OUTPUTS;
}

char* getParameterName(DWORD index)
{
	return GParamConstants[index].Name;
}

float getParameterDefault(DWORD index)
{
	return GParamConstants[index].Default;
}

unsigned int getParameterType(DWORD index)
{
	return GParamConstants[index].Type;
}

unsigned int getOutputType(DWORD index)
{
	return GOutputConstants[index].Type;
}

char* getOutputName(DWORD index)
{
	return GOutputConstants[index].Name;
}

plugClass::plugClass()
{
    for (register int i=0; i<NUM_OUTPUTS; i++)
    {
        FOutputs[i].SliceCount = 0;
        FOutputs[i].Spread = (float*) calloc(1, 0);
    }

    FWidth = FVideoInfo.frameWidth;
    FHeight = FVideoInfo.frameHeight;

    FGrayImage = (unsigned char*) calloc(FWidth*FHeight, sizeof(unsigned char));

    InitializeCriticalSection(&CriticalSection);


    // create a tracker that does:
    //  - 6x6 sized marker images
    //  - samples at a maximum of 6x6
    //  - works with luminance (gray) images
    //  - can load a maximum of 1 pattern
    //  - can detect a maximum of 8 patterns in one image
    FTracker = new ARToolKitPlus::TrackerSingleMarkerImpl<6,6,6, 1, 8>(FWidth, FHeight);
    const char* description = FTracker->getDescription();
	//printf("ARToolKitPlus compile-time information:\n%s\n\n", description);
    sprintf(FDebugBuffer, "ARToolKitPlus compile-time information:\n%s\n\n", description);
    OutputDebugString(FDebugBuffer);

    //set pixelformat (we do the grayscale conversion manually)
	FTracker->setPixelFormat(ARToolKitPlus::PIXEL_FORMAT_LUM);

	//use UndistLUT
	FTracker->setLoadUndistLUT(true);

    //set a threshold. alternatively we could also activate automatic thresholding
    FTracker->activateAutoThreshold(true);

    //let's use lookup-table undistortion for high-speed
    //note: LUT only works with images up to 1024x1024
    FTracker->setUndistortionMode(ARToolKitPlus::UNDIST_LUT);

    //RPP is more robust than ARToolKit's standard pose estimator
    FTracker->setPoseEstimator(ARToolKitPlus::POSE_ESTIMATOR_RPP);

    FInitialized = false;
}

void plugClass::init()
{
    // -> setting defaults for input values //
    for (int in=0; in<NUM_PARAMS; in++) FParams[in].Value=GParamConstants[in].Default;
}


plugClass::~plugClass()
{
    for (int i=0; i<NUM_OUTPUTS; i++) free(FOutputs[i].Spread);

    delete FTracker;

    DeleteCriticalSection(&CriticalSection);
}

char* plugClass::getParameterDisplay(DWORD index)
{
	// fill the array with spaces first
	for (int n=0; n<16; n++)
    {
		FParams[index].DisplayValue[n] = ' ';
	}
	sprintf(FParams[index].DisplayValue, "%f",FParams[index].Value);
	return FParams[index].DisplayValue;
}



DWORD plugClass::setParameter(SetParameterStruct* pParam)
{
	FParams[pParam->index].Value = pParam->value;

	if (pParam->index == 3) 	//camera file
	{
        float f = pParam->value;
	    int* ip = (int*)&f;
	    char* cp = (char*)*ip;
        int length = strlen(cp);
        memcpy(&FCameraFile[0], cp, length);

        //for some reaseon ARTK+ init() needs / instead of \ in the path
        for (int i=0; i<length; i++)
        {
            if (FCameraFile[i] == '\\')
              FCameraFile[i] = '/';
        }

        FNewCameraFile = true;
	}

	return FF_SUCCESS;
}

void plugClass::loadCameraFile()
{
    // load a camera file. two types of camera files are supported:
    //  - Std. ARToolKit
    //  - MATLAB Camera Calibration Toolbox
    if(!FTracker->init(&FCameraFile[0],  1.0f, 1000.0f))
	{
	    FInitialized = false;
		sprintf(FDebugBuffer, "ERROR: init() failed!");
        OutputDebugString(FDebugBuffer);
	}
	else
	  FInitialized = true;

    FNewCameraFile = false;
}


// -> Function is called when spread input values (types 20, 21 or 22) are modified //
DWORD plugClass::setInput(InputStruct* pParam)
{
 	return FF_SUCCESS;
}


float plugClass::getParameter(DWORD index)
{
	return FParams[index].Value;
}

DWORD plugClass::getOutputSliceCount(DWORD index)
{
	return FOutputs[index].SliceCount;
}

DWORD plugClass::setThreadLock(DWORD Enter)
{
	if (*(bool*) Enter)
	  EnterCriticalSection(&CriticalSection);
    else
      LeaveCriticalSection(&CriticalSection);
}

float* plugClass::getOutput(DWORD index)
{
   return FOutputs[index].Spread;
}

DWORD plugClass::processFrame(LPVOID pFrame)
{
	switch (FVideoInfo.bitDepth) {
		case 1:
			return processFrame24Bit(pFrame);
		case 2:
			return processFrame32Bit(pFrame);
		default:
			return FF_FAIL;
	}
}

#define hibyte(x) (unsigned char)((x)>>8)
DWORD plugClass::processFrame24Bit(LPVOID pFrame)
{
    EnterCriticalSection(&CriticalSection);

    if (FNewCameraFile)
        loadCameraFile();

    if (!FInitialized)
    {
        LeaveCriticalSection(&CriticalSection);
        return FF_SUCCESS;
    }

    //convert camera image to grayscale and flip Y
    unsigned char r, g, b;
    unsigned char* src = (unsigned char*) pFrame;
    unsigned char* dest = FGrayImage;
    dest+=FWidth*FHeight-FWidth;

    for (int y=0; y<FHeight; y++)
    {
        for(int x=0; x<FWidth; x++)
        {
            //fastest and best
            r = *src++;
            g = *src++;
            b = *src++;
            *dest++ = hibyte(r * 77 + g * 151 + b * 28);
        }
        dest -= FWidth*2;
    }

    //check input pins
    bool useBCH = (FParams[0].Value == 1); //use bch-marker
    FTracker->setMarkerMode(useBCH ? ARToolKitPlus::MARKER_ID_BCH : ARToolKitPlus::MARKER_ID_SIMPLE);

    bool useThin (FParams[1].Value == 1); //thin border
    FTracker->setBorderWidth(useThin ? 0.125f : 0.250f);

    ARToolKitPlus::ARMarkerInfo* mi;
    int markercount;

    //detect all the markers
    FTracker->arDetectMarker(FGrayImage, FTracker->getThreshold(), &mi, &markercount);

    //make the outputs fit
    if (FOutputs[0].SliceCount != markercount)//ids
    {
        FOutputs[0].SliceCount = markercount;
        FOutputs[0].Spread = (float*) realloc(FOutputs[0].Spread, sizeof(float) * markercount);
    }
    if (FOutputs[2].SliceCount != markercount)//projection
    {
        FOutputs[2].SliceCount = markercount;
        FOutputs[2].Spread = (float*) realloc(FOutputs[2].Spread, sizeof(float) * markercount);
    }
    if (FOutputs[1].SliceCount != markercount*16)//modelviews
    {
        FOutputs[1].SliceCount = markercount*16;
        FOutputs[1].Spread     = (float*) realloc(FOutputs[1].Spread, sizeof(float) * markercount*16);
    }

    ARFloat center[2] = {0.0, 0.0};
    ARFloat width = FParams[2].Value * 100;
    ARFloat conv[3][4];

    //get modelview matrices for detected markers and put them into the outputs
    for (int i=0; i<markercount; i++)
    {
        FOutputs[0].Spread[i] = mi[0].id;
        FTracker->arGetTransMat(mi, center, width, conv);

        //flip back Y (see above: image was flipped to be trackable)
        conv[1][0] *= -1;
        conv[1][1] *= -1;
        conv[1][2] *= -1;
        conv[1][3] *= -1;

        for(int y = 0; y < 3; y++ )
        {
            for(int x = 0; x < 4; x++ )
            {
                FOutputs[1].Spread[i*16 + x*4+y] = conv[y][x];
            }
        }
        FOutputs[1].Spread[i*16 + 0*4+3] = FOutputs[1].Spread[i*16 + 1*4+3] = FOutputs[1].Spread[i*16 + 2*4+3] = 0.0;
        FOutputs[1].Spread[i*16 + 3*4+3] = 1.0;

        mi++;
    }

    //get projection matrix
    FOutputs[2].SliceCount = 16;
    FOutputs[2].Spread     = (float*) realloc(FOutputs[2].Spread, sizeof(float) * 16);
    for(int i=0; i<16; i++)
        FOutputs[2].Spread[i] = FTracker->getProjectionMatrix()[i];

    //flip back Y (see above: image was flipped to be trackable)
    FOutputs[2].Spread[5] *= -1;


    LeaveCriticalSection(&CriticalSection);

    return FF_SUCCESS;
}



DWORD plugClass::processFrame32Bit(LPVOID pFrame)
{
	return FF_FAIL;
}

DWORD plugClass::processFrameCopy(ProcessFrameCopyStruct* pFrameData)
{
    return FF_FAIL;
}

DWORD plugClass::processFrameCopy24Bit(ProcessFrameCopyStruct* pFrameData)
{
	return FF_FAIL;
}

DWORD plugClass::processFrameCopy32Bit(ProcessFrameCopyStruct* pFrameData)
{
	return FF_FAIL;
}

DWORD getPluginCaps(DWORD index)
{
	switch (index) {

	case FF_CAP_16BITVIDEO:
		return FF_FALSE;

	case FF_CAP_24BITVIDEO:
		return FF_TRUE;

	case FF_CAP_32BITVIDEO:
		return FF_FALSE;

	case FF_CAP_PROCESSFRAMECOPY:
		return FF_FALSE;

	case FF_CAP_MINIMUMINPUTFRAMES:
		return NUM_INPUTS;

	case FF_CAP_MAXIMUMINPUTFRAMES:
		return NUM_INPUTS;

	case FF_CAP_COPYORINPLACE:
		return FF_FALSE;

	default:
		return FF_FALSE;
	}
}

PlugInfoStruct* getInfo()
{
	GPlugInfo.APIMajorVersion = 2;		// number before decimal point in version nums
	GPlugInfo.APIMinorVersion = 000;		// this is the number after the decimal point
										// so version 0.511 has major num 0, minor num 501
	char ID[5] = "JFAT";		     // this *must* be unique to your plugin
                                    // see www.freeframe.org for a list of ID's already taken
	char name[17] = "ARTK+Tracker";

	memcpy(GPlugInfo.uniqueID, ID, 4);
	memcpy(GPlugInfo.pluginName, name, 16);
	GPlugInfo.pluginType = FF_EFFECT;

	return &GPlugInfo;
}

LPVOID getExtendedInfo()
{

	GPlugExtInfo.PluginMajorVersion = 1;
	GPlugExtInfo.PluginMinorVersion = 10;

	// I'm just passing null for description etc for now
	// todo: send through description and about
	GPlugExtInfo.Description = NULL;
	GPlugExtInfo.About = NULL;

	// FF extended data block is not in use by the API yet
	// we will define this later if we want to
	GPlugExtInfo.FreeFrameExtendedDataSize = 0;
	GPlugExtInfo.FreeFrameExtendedDataBlock = NULL;

	return (LPVOID) &GPlugExtInfo;
}


