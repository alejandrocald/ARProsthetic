// Fill out your copyright notice in the Description page of Project Settings.

#include "WebCamReader.h"
#include "limbitless3D.h"
#include <iostream>
#include "ColorDetector.h"
#include "Engine.h"

using namespace cv;
using namespace std;

// Sets default values
AWebCamReader::AWebCamReader()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialize OpenCV and webcam properties
	CameraID = 0;
	OperationMode = 3;
	RefreshRate = 8;
	isStreamOpen = false;
	VideoSize = FVector2D(0, 0);
	ShouldResize = false;
	ResizeDeminsions = FVector2D(320, 240);
	RefreshTimer = 0.0f;
	stream = cv::VideoCapture();
	frame = cv::Mat();
	initHSVvalues();
	rotation = FRotator(0, 0, 0);
}

// Called when the game starts or when spawned
void AWebCamReader::BeginPlay()
{
	Super::BeginPlay();
	
	// Open the stream
	stream.open(CameraID);
	if (stream.isOpened())
	{
		// Initialize stream
		isStreamOpen = true;
		UpdateFrame();
		VideoSize = FVector2D(frame.cols, frame.rows);
		size = cv::Size(ResizeDeminsions.X, ResizeDeminsions.Y);
		VideoTexture = UTexture2D::CreateTransient(VideoSize.X, VideoSize.Y);
		VideoTexture->UpdateResource();
		VideoUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, VideoSize.X, VideoSize.Y);

		// Initialize data array
		Data.Init(FColor(0, 0, 0, 255), VideoSize.X * VideoSize.Y);

		// Do first frame
		DoProcessing();
		UpdateTexture();
		OnNextVideoFrame();

		//determine an estimate of the area of the screen where the arm band will be present
		//roughly a bit above the center of the screen
		startPosX = VideoSize.Y*(VideoSize.X / 3);
		startPosY = VideoSize.Y/2 * VideoSize.X;
	}
}

// Called every frame and updates the texture
void AWebCamReader::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshTimer += DeltaTime;
	if (isStreamOpen && RefreshTimer >= 1.0f / RefreshRate)
	{
		RefreshTimer -= 1.0f / RefreshRate;
		UpdateFrame();
		DoProcessing();
		UpdateTexture();
		OnNextVideoFrame();
	}
}

void AWebCamReader::ChangeOperation()
{
	OperationMode++;
	OperationMode %= 3;
}

void AWebCamReader::UpdateFrame()
{
	if (stream.isOpened())
	{
		stream.read(frame);
		if (ShouldResize)
		{
			cv::resize(frame, frame, size);
		}
	}
	else {
		isStreamOpen = false;
	}
}

void AWebCamReader::DoProcessing()
{
	// TODO: Do any processing with frame here!

	if (OperationMode == 0) {
		//color detection
		cv::Mat src, imgThresholded; //imgOriginal , imgHSV

		cvtColor(frame, src, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV: COLOR_BGR2HSV

		//Threshold the image
		inRange(src, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), imgThresholded); 
		
		//morphological opening (remove small objects from the foreground)
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		//morphological closing (fill small holes in the foreground)
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		erode(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		//Calculate the moments of the thresholded image
		Moments oMoments = moments(imgThresholded);

		double dM01 = oMoments.m01;
		double dM10 = oMoments.m10;
		double dArea = oMoments.m00;

		// if the area <= 10000, there are probably no objects in the image and it's because of the noise
		if (dArea > 10000)
		{
			//calculate the position of the marker
			posX = (int)(dM10 / dArea);
			posY = (int)(dM01 / dArea);
			
			//calculate angle of the Yaw based on the visibility of left and right bands
			calculateYaw(imgThresholded, 200);

		}

	}

}



void AWebCamReader::UpdateTexture()
{
	if (isStreamOpen && frame.data)
	{
		// Copy Mat data to Data array
		for (int y = 0; y < VideoSize.Y; y++)
		{
			for (int x = 0; x < VideoSize.X; x++)
			{
				int i = x + (y * VideoSize.X);
				Data[i].B = frame.data[i * 3 + 0];
				Data[i].G = frame.data[i * 3 + 1];
				Data[i].R = frame.data[i * 3 + 2];
				
			}
		}

		//debugging function to visualize where the posX and posY variables are at
		drawLineOnTexture(posX, posY, posX + 200, posY , 0, 0, 255);
		
		// Update texture 2D
		UpdateTextureRegions(VideoTexture, (int32)0, (uint32)1, VideoUpdateTextureRegion, (uint32)(4 * VideoSize.X), (uint32)4, (uint8*)Data.GetData(), false);
	}
}

void AWebCamReader::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
	if (Texture->Resource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*, RegionData, RegionData,
			bool, bFreeData, bFreeData,
			{
				for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
				{
					int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
					if (RegionData->MipIndex >= CurrentFirstMip)
					{
						RHIUpdateTexture2D(
							RegionData->Texture2DResource->GetTexture2DRHI(),
							RegionData->MipIndex - CurrentFirstMip,
							RegionData->Regions[RegionIndex],
							RegionData->SrcPitch,
							RegionData->SrcData
							+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
							+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
						);
					}
				}
		if (bFreeData)
		{
			FMemory::Free(RegionData->Regions);
			FMemory::Free(RegionData->SrcData);
		}
		delete RegionData;
			});
	}
}

//initialize HSV values
void AWebCamReader::initHSVvalues()
{
	//currently set to Green
	iLowH = 53;
	iHighH = 83;
	
	iLowS = 60;
	iHighS = 255;
	
	iLowV = 60;
	iHighV = 255;
}

//reads the amount of the pixels on the left and right side of the Arm Band to calculate the orientation of the yaw
float AWebCamReader::calculateYaw(cv::Mat &image, int distance)
{
	int y = posY;
	int x = posX;

	//how many pixels on the left and right to check
	int dist = distance;

	int next = ((y)*VideoSize.X + x + 1); //origin
	dist += next;
	
	//right check
	int counterR = 0;
	for (next = next; next < dist; next++)
	{
		if (next < 0 || next > VideoSize.X*VideoSize.Y) break;

		if (image.data[next] != 0) counterR++; 		
	}

	
	next = ((y)*VideoSize.X + x + 1);
	dist = next - distance;

	//left check
	int counterL = 0;
	for (next = next; next > dist; next--)
	{
		if (next < 0 || next > VideoSize.X*VideoSize.Y) break;

		if (image.data[next] != 0) counterL++;
	}

	
	float yaw = 0;
	if (counterL > counterR)
	{
		if (counterR == 0)
		{
			counterR = 1;
			yaw = -60;
		}
		else yaw = (counterL / counterR) * -2; //scale angle
	}
	else
	{
		if (counterL == 0)
		{
			counterL = 1;
			yaw = 80;
		}
		else yaw = (counterR / counterL) * 2; //scale angle
	}

	//print to screen how many pixels the right and left are currently being seen
	GEngine->AddOnScreenDebugMessage(-1, .1f, FColor::Green, FString::Printf(TEXT("right: %d   left: %d"), counterR, counterL));

	//clamp the yaw to reasonable values
	if (yaw > 80) yaw = 80;
	else if (yaw < -60) yaw = -60;

	rotation.Yaw = yaw;
	return yaw;
}

//draws a line on the texture from the origin (x,y) to the end (x,y) point of color BGR
void AWebCamReader::drawLineOnTexture(int orgX, int orgY, int endX, int endY, int blue, int green, int red)
{
	int length = FMath::Sqrt(FMath::Pow((endX - orgX), 2) + FMath::Pow((endY - orgY), 2));
	int unitX = (endX - orgX) / length;
	int unitY = (endY - orgY) / length;
	int unit = ((unitY*VideoSize.X) + unitX);
	int next = ((orgY)*VideoSize.X + orgX + 1);

	if (endX < 0 || endX > VideoSize.X || endY < 0 || endY > VideoSize.Y) return;

	for (int g = 0; g < length; g++ )
	{
		next += unit;
		
		Data[next].B = blue;
		Data[next].G = green;
		Data[next].R = red;
	}
}

//draws a line on the texture from the origin (x,y) to the end (x,y) point of color BGR with the desired thickness
void AWebCamReader::drawLineOnTexture(int orgX, int orgY, int endX, int endY, int blue, int green, int red, int thickness)
{
	int x = endX - orgX;
	int y = endY - orgY;

	if (x < y)
	{
		for (int i = 0; i < thickness; i++)
		{
			drawLineOnTexture(orgX + i, orgY, endX + i, endY, blue, green, red);
		}
	}
	else
	{
		for (int i = 0; i < thickness; i++)
		{
			drawLineOnTexture(orgX, orgY + i, endX, endY + i, blue, green, red);
		}
	}
}