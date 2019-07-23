#include <utils/Log.h>
//#include "skl_decode.h"

#define LOG_TAG "shockley"

static int indexTable[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4,
		6, 8, };

static int stepsizeTable[89] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21,
		23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
		118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
		449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
		1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
		4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
		13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 };

void able(unsigned char *inbuff, unsigned char *outbuff,
		int len_of_in/*，struct adpcm_state * state*/) {
	ALOGD("resample start -- ");
	int i, j = 0;
	unsigned int tmp1, tmp2;
	unsigned int Samp;
	unsigned int SampH, SampL;

	for (i = 0; i < len_of_in; i = i + 2) {
		tmp1 = inbuff[i] + inbuff[i + 1] * 256;

		if (tmp1 > 32767)
			tmp1 = 65536 - tmp1;

		tmp2 = inbuff[i + 2] + inbuff[i + 3] * 256;

		if (tmp2 > 32767)
			tmp2 = 65536 - tmp2;

		Samp = (tmp1 + tmp2) / 2;

		SampH = Samp / 256;
		SampL = Samp - 256 * SampH;

		outbuff[j++] = inbuff[i];
		outbuff[j++] = inbuff[i + 1];
		outbuff[j++] = inbuff[i];
		outbuff[j++] = inbuff[i + 1];

	}

}

int add(unsigned char *inbuff, unsigned char *outbuff,
		int len_of_in/*, struct adpcm_state *state*/) {
	ALOGE("start decoder--");
	int i = 0, j = 2;
	char tmp_data;
	static unsigned char outbuf[128];

	long step; /* Quantizer step size */
	signed long predsample; /* Output of ADPCM predictor */
	signed long diffq; /* Dequantized predicted difference */
	int index; /* Index into step size table */

	int Samp;
	unsigned char SampH, SampL;
	unsigned char inCode;
	long preL, preH;
	/* Restore previous values of predicted sample and quantizer step
	 size index
	 */
	int k = 0;

	outbuf[0] = inbuff[0];
	outbuf[1] =  inbuff[1];
	index = inbuff[3];

	predsample = inbuff[0] + inbuff[1] * 256;

//	ALOGD("pred sample is %d\n  ",predsample);
	if (predsample > 32767)
		predsample -= 65536;


	for (i = 8; i < len_of_in * 2; i++) {
		tmp_data = inbuff[i / 2];
		if (i % 2 == 0)
			inCode = (tmp_data & 0xf0) >> 4;
		else
			inCode = tmp_data & 0x0f;

		step = stepsizeTable[index];

		diffq = step >> 3;
		if (inCode & 4)
			diffq += step;
		if (inCode & 2)
			diffq += step >> 1;
		if (inCode & 1)
			diffq += step >> 2;

		if (inCode & 8)
			predsample -= diffq;
		else
			predsample += diffq;
		/* Check for overflow of the new predicted sample
		 */
		if (predsample > 32767)
			predsample = 32767;
		else if (predsample < -32768)
			predsample = -32768;
		/* Find new quantizer stepsize index by adding the old index
		 to a table lookup using the ADPCM code
		 */
		index += indexTable[inCode];
		/* Check for overflow of the new quantizer step size index
		 */
		if (index < 0)
			index = 0;
		if (index > 88)
			index = 88;
		/* Return the new ADPCM code */
		Samp = predsample;

		if (Samp >= 0) {
			SampH = Samp / 256;
			SampL = Samp - 256 * SampH;
		} else {
			Samp = 32768 + Samp;
			SampH = Samp / 256;
			SampL = Samp - 256 * SampH;
			SampH += 0x80;
		}

		outbuf[j++] = SampL;
		outbuf[j++] = SampH;
	}
	ALOGE("go able--");

	able(outbuf, outbuff, 256);

	ALOGE("finish decoder--");
	return 512;

}
