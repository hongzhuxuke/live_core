#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "AudioMixing.h"

#define ALL_CH 4			//混音路数，最大为16
#define SAMPLERATE 32000	//采样率，支持8000、16000、32000、44100
#define SAMPLETYPE 0		//采样精度 0-short型， 1-float型
#define SINGLEOUT  0		//是否只输出总体混音 0-不是， 1-是

int main(){
	FILE *fpAudioFileInput[ALL_CH];	
	FILE *fpAudioFileOutput[ALL_CH+1];
	
	int iChannels;
	int iFlag[ALL_CH];
#if SAMPLETYPE == 0
	short *pDataIn;
	short *pDataOut;
#else
	float *pDataIn;
	float *pDataOut;
#endif

	int iSampleRate;
	int iSampleType;
	int iBits;
	int iFrameLength;
	int iFrameSamples;
	int iOffset;
	int i;


	iChannels = ALL_CH;
	for (i = 0; i < ALL_CH; i++){
		iFlag[i] = 0;
	}

	iSampleRate = SAMPLERATE;
	iSampleType = SAMPLETYPE;
	if (iSampleType == 0){
		iBits = 2;
	}
	else{
		iBits = 4;
	}
	iFrameLength = iSampleRate * PROCUNIT / 1000 * iBits;
	iFrameSamples = iSampleRate * PROCUNIT / 1000;
	iOffset = 0;

	fpAudioFileInput[0] = fopen("mMediaFile.pcm", "rb");
	fpAudioFileInput[1] = fopen("Send2f.pcm", "rb");
	fpAudioFileInput[2] = fopen("Send3.pcm", "rb");
	fpAudioFileInput[3] = fopen("Send4.pcm", "rb");
	


	fpAudioFileOutput[0] = fopen("Receive1.pcm", "w+b");
	fpAudioFileOutput[1] = fopen("Receive2.pcm", "w+b");
	fpAudioFileOutput[2] = fopen("Receive3.pcm", "w+b");
	fpAudioFileOutput[3] = fopen("Receive4.pcm", "w+b");
	fpAudioFileOutput[ALL_CH] = fopen("ReceiveAll.pcm", "w+b");


#if SAMPLETYPE == 0
	pDataIn = (short *)malloc(iFrameLength * ALL_CH);
	pDataOut = (short *)malloc(iFrameLength * (ALL_CH+1));
#else 
	pDataIn = (float *)malloc(iFrameLength * ALL_CH);
	pDataOut = (float *)malloc(iFrameLength * (ALL_CH + 1));
#endif


	for (; iChannels > 0;){
		iOffset = 0;
		for (i = 0; i < ALL_CH; i++){
			if (!iFlag[i]){				
				if (fread(pDataIn + iOffset, iBits, iFrameSamples, fpAudioFileInput[i]) != iFrameSamples){
					iFlag[i] = 1;
					iChannels--;
				}
				else{
					iOffset += iFrameSamples;
				}
			}
		}

		if (!iChannels){
			break;
		}
		if (VhallAudioMixing(pDataIn, pDataOut, iChannels, iSampleRate, iSampleType, SINGLEOUT)){
			printf("Oops!!Error occured in audio mixing!!!\n");
		}
		/*memcpy(pDataOut,pDataIn,iFrameLength*iChannels);
		memcpy(pDataOut + iFrameSamples*iChannels, pDataIn, iFrameLength);*/
		iOffset = 0;
		if (SINGLEOUT == 0){
			for (i = 0; i < ALL_CH; i++){
				if (!iFlag[i]){
					fwrite(pDataOut + iOffset, iBits, iFrameSamples, fpAudioFileOutput[i]);
					iOffset += iFrameSamples;
				}
			}
		}		
		fwrite(pDataOut + iOffset, iBits, iFrameSamples, fpAudioFileOutput[ALL_CH]);
	}
	

	free(pDataIn);
	free(pDataOut);
	for (i = 0; i < ALL_CH; i++){
		fclose(fpAudioFileInput[i]);
		fclose(fpAudioFileOutput[i]);
	}
	fclose(fpAudioFileOutput[ALL_CH]);
	system("pause");
	return 0;
}