#include <PvDeviceFinderWnd.h>
#include <PvDevice.h>
#include <PvPipeline.h>
#include <PvBuffer.h>
#include <PvStream.h>
#include <PvStreamRaw.h>
#include <PvDisplayWnd.h>
#include <conio.h>
#include "HostParameter.h"
#include "resource.h"

unsigned char *UserBuffer;

void Configure(PvGenParameterArray *lDeviceParams)
{
	int AcqType = SendMessage(hPF_Type, CB_GETCURSEL, 0, 0);
	char *Type;
	if (AcqType == 1)
		Type = _T("Continous");
	else if (AcqType == 2)
		Type = _T("SingleFrame");
	else if (AcqType == 3)
		Type = _T("MultiFrame");
	else if (AcqType == 4)
		Type = _T("ContinousRecording");
	else if (AcqType == 5)
		Type = _T("ContinousReadout");
	else if (AcqType == 6)
		Type = _T("SingleFrameRecording");
	else if (AcqType == 7)
		Type = _T("SingleFrameReadout");



	TCHAR txt_ExpTime[10];
	GetDlgItemText(hWnd_Setting, IDC_PFEXP_EDIT, txt_ExpTime, sizeof(txt_ExpTime) / sizeof(TCHAR));
	//ExpTime = atof(txt_ExpTime);


	float f = 2500.0;
	lDeviceParams->SetFloatValue("ExposureTime", atof(txt_ExpTime));

	lDeviceParams->SetIntegerValue("Width", 2080);
	lDeviceParams->SetIntegerValue("OffsetX", 0);
	lDeviceParams->SetIntegerValue("Height", 2080);
	lDeviceParams->SetIntegerValue("OffsetY", 0);

	lDeviceParams->SetEnumValue("AcquisitionMode", Type);
	lDeviceParams->SetEnumValue("PixelFormat", "Mono8");
	//lDeviceParams->SetEnumValue("AcquisitionMode", 0);

	//lDeviceParams->SetEnumValue("AcquisitionMode", "SingleFrame");
	//lDeviceParams->SetEnumValue("AcquisitionMode", 1);

	lDeviceParams->SetBooleanValue("AcquisitionFrameRateEnable", true);

	lDeviceParams->SetFloatValue("AcquisitionFrameRate", 50.5);
}

bool PF_AcquireImages()
{
	// Create a GEV Device finder dialog
	PvDeviceFinderWnd lDeviceFinderWnd;

	// Prompt the user to select a GEV Device
	lDeviceFinderWnd.ShowModal();

	// Get the connectivity information for the selected GEV Device
	PvDeviceInfo* lDeviceInfo = lDeviceFinderWnd.GetSelected();

	// If no device is selected, abort
	if (lDeviceInfo == NULL)
	{
		printf("No device selected.\n");
		return false;
	}

	// Connect to the GEV Device
	PvDevice lDevice;
	printf("Connecting to %s\n", lDeviceInfo->GetMACAddress().GetAscii());
	if (!lDevice.Connect(lDeviceInfo).IsOK())
	{
		printf("Unable to connect to %s\n", lDeviceInfo->GetMACAddress().GetAscii());
		return false;
	}
	printf("Successfully connected to %s\n", lDeviceInfo->GetMACAddress().GetAscii());
	printf("\n");

	// Get device parameters need to control streaming
	PvGenParameterArray *lDeviceParams = lDevice.GetGenParameters();

	// Camera configuration
	Configure(lDeviceParams);

	// Negotiate streaming packet size
	lDevice.NegotiatePacketSize();

	// Create the PvStream object
	PvStream lStream;

	// Open stream - have the PvDevice do it for us
	printf("Opening stream to device\n");
	lStream.Open(lDeviceInfo->GetIPAddress());

	// Create the PvPipeline object
	PvPipeline lPipeline(&lStream);

	// Reading payload size from device
	PvInt64 lSize = 0;
	lDeviceParams->GetIntegerValue("PayloadSize", lSize);

	// Set the Buffer size and the Buffer count
	lPipeline.SetBufferSize(static_cast<PvUInt32>(lSize));
	lPipeline.SetBufferCount(16); // Increase for high frame rate without missing block IDs
	// Set the UserBuffer size
	UserBuffer = (unsigned char*)malloc(lSize);

	// Have to set the Device IP destination to the Stream
	lDevice.SetStreamDestination(lStream.GetLocalIPAddress(), lStream.GetLocalPort());

	// IMPORTANT: the pipeline needs to be "armed", or started before 
	// we instruct the device to send us images
	printf("Starting pipeline\n");
	lPipeline.Start();

	// Create and open modeless display
	PvDisplayWnd lDisplay;
	lDisplay.ShowModeless();

	// Get stream parameters/stats
	PvGenParameterArray *lStreamParams = lStream.GetParameters();

	// TLParamsLocked is optional but when present, it MUST be set to 1
	// before sending the AcquisitionStart command
	lDeviceParams->SetIntegerValue("TLParamsLocked", 1);

	printf("Resetting timestamp counter...\n");
	lDeviceParams->ExecuteCommand("GevTimestampControlReset");

	// The pipeline is already "armed", we just have to tell the device
	// to start sending us images
	printf("Sending StartAcquisition command to device\n");
	lDeviceParams->ExecuteCommand("AcquisitionStart");

	char lDoodle[] = "|\\-|-/";
	int lDoodleIndex = 0;
	PvInt64 lImageCountVal = 0;
	double lFrameRateVal = 0.0;
	double lBandwidthVal = 0.0;

	// Acquire images until the user instructs us to stop
	printf("\n<press the enter key to stop streaming>\n");
	while (!_kbhit())
	{
		// Retrieve next buffer		
		PvBuffer *lBuffer = NULL;
		PvResult  lOperationResult;
		PvResult lResult = lPipeline.RetrieveNextBuffer(&lBuffer, 1000, &lOperationResult);
		PvUInt32 lWidth = 0, lHeight = 0;

		if (lResult.IsOK())
		{
			if (lOperationResult.IsOK())
			{
				lStreamParams->GetIntegerValue("ImagesCount", lImageCountVal);
				lStreamParams->GetFloatValue("AcquisitionRateAverage", lFrameRateVal);
				lStreamParams->GetFloatValue("BandwidthAverage", lBandwidthVal);

				// If the buffer contains an image, display width and height

				if (lBuffer->GetPayloadType() == PvPayloadTypeImage)
				{
					// Get image specific buffer interface
					PvImage *lImage = lBuffer->GetImage();
					// Read width, height
					lWidth = lBuffer->GetImage()->GetWidth();
					lHeight = lBuffer->GetImage()->GetHeight();
				}

				printf("%c Timestamp: %012llX BlockID: %06llX W: %i H: %i %.01f FPS %.01f Mb/s\r",
					lDoodle[lDoodleIndex],
					lBuffer->GetTimestamp(),
					lBuffer->GetBlockID(),
					lWidth,
					lHeight,
					lFrameRateVal,
					lBandwidthVal / 1000000.0);
			}
			// We have an image - do some processing (...) 
			PvUInt8 *img;
			img = NULL;
			img = lBuffer->GetDataPointer();
			memcpy(UserBuffer, img, lWidth*lHeight); // lWidth*lHeight is equivalent to lSize
			lDisplay.Display(*lBuffer);

			//and VERY IMPORTANT, release the buffer back to the pipeline
			lPipeline.ReleaseBuffer(lBuffer);
		}
		else
		{
			// Timeout
			printf("%c Timeout\r", lDoodle[lDoodleIndex]);
		}

		++lDoodleIndex %= 6;

		// End of acquisition loop, work Message Queue ?next best thing to a true Message Pump
		lDisplay.DoEvents();
	}

	_getch(); // Flush key buffer for next stop
	printf("\n\n");

	// Tell the device to stop sending images
	printf("Sending AcquisitionStop command to the device\n");
	lDeviceParams->ExecuteCommand("AcquisitionStop");

	// If present reset TLParamsLocked to 0. Must be done AFTER the 
	// streaming has been stopped
	lDeviceParams->SetIntegerValue("TLParamsLocked", 0);

	// We stop the pipeline - letting the object lapse out of 
	// scope would have had the destructor do the same, but we do it anyway
	printf("Stop pipeline\n");
	lPipeline.Stop();

	// Now close the stream. Also optionnal but nice to have
	printf("Closing stream\n");
	lStream.Close();

	// Finally disconnect the device. Optional, still nice to have
	printf("Disconnecting device\n");
	lDevice.Disconnect();

	return true;
}
