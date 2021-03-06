/*
*    Copyright (C) 2016, 2017
*    Jan van Katwijk (J.vanKatwijk@gmail.com)
*    Lazy Chair Programming
*
*    This file is part of the DAB-library
*    DAB-library is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    DAB-library is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with DAB-library; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*	Adapted to tvheadend by Matthias Walliczek (matthias@walliczek.de)
*/

#include <fftw3.h>
#include <math.h>
#include <complex.h>

#include "tvheadend.h"
#include "../dab_constants.h"
#include "phasereference.h"
#include "dab.h"

struct phasetableElement {
    int32_t	kmin, kmax;
    int32_t i;
    int32_t n;
};

static const
struct phasetableElement modeI_table[] = {
    { -768, -737, 0, 1 },
{ -736, -705, 1, 2 },
{ -704, -673, 2, 0 },
{ -672, -641, 3, 1 },
{ -640, -609, 0, 3 },
{ -608, -577, 1, 2 },
{ -576, -545, 2, 2 },
{ -544, -513, 3, 3 },
{ -512, -481, 0, 2 },
{ -480, -449, 1, 1 },
{ -448, -417, 2, 2 },
{ -416, -385, 3, 3 },
{ -384, -353, 0, 1 },
{ -352, -321, 1, 2 },
{ -320, -289, 2, 3 },
{ -288, -257, 3, 3 },
{ -256, -225, 0, 2 },
{ -224, -193, 1, 2 },
{ -192, -161, 2, 2 },
{ -160, -129, 3, 1 },
{ -128,  -97, 0, 1 },
{ -96,   -65, 1, 3 },
{ -64,   -33, 2, 1 },
{ -32,    -1, 3, 2 },
{ 1,    32, 0, 3 },
{ 33,    64, 3, 1 },
{ 65,    96, 2, 1 },
//	{ 97,   128, 2, 1},  found bug 2014-09-03 Jorgen Scott
{ 97,	128, 1, 1 },
{ 129,  160, 0, 2 },
{ 161,  192, 3, 2 },
{ 193,  224, 2, 1 },
{ 225,  256, 1, 0 },
{ 257,  288, 0, 2 },
{ 289,  320, 3, 2 },
{ 321,  352, 2, 3 },
{ 353,  384, 1, 3 },
{ 385,  416, 0, 0 },
{ 417,  448, 3, 2 },
{ 449,  480, 2, 1 },
{ 481,  512, 1, 3 },
{ 513,  544, 0, 3 },
{ 545,  576, 3, 3 },
{ 577,  608, 2, 3 },
{ 609,  640, 1, 0 },
{ 641,  672, 0, 3 },
{ 673,  704, 3, 0 },
{ 705,  736, 2, 1 },
{ 737,  768, 1, 1 },
{ -1000, -1000, 0, 0 }
};

static
const int8_t h0[] = { 0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1,
0, 2, 0, 0, 0, 0, 1, 1, 2, 0, 0, 0, 2, 2, 1, 1 };
static
const int8_t h1[] = { 0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0,
0, 3, 2, 3, 0, 1, 3, 0, 2, 1, 2, 3, 2, 3, 3, 0 };
static
const int8_t h2[] = { 0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3,
0, 0, 0, 2, 0, 2, 1, 3, 2, 2, 0, 2, 2, 0, 1, 3 };
static
const int8_t h3[] = { 0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2,
0, 1, 2, 1, 0, 3, 3, 2, 2, 3, 2, 1, 2, 1, 3, 2 };

float _Complex refTable[T_u];

float phaseDifferences[DIFF_LENGTH];

int32_t	geth_table(int32_t i, int32_t j);

float	get_Phi(int32_t k);

int32_t	geth_table(int32_t i, int32_t j) {
    switch (i) {
    case 0:
        return h0[j];
    case 1:
        return h1[j];
    case 2:
        return h2[j];
    default:
    case 3:
        return h3[j];
    }
}

float	get_Phi(int32_t k) {
    int32_t k_prime, i, j, n = 0;

    for (j = 0; modeI_table[j].kmin != -1000; j++) {
        if ((modeI_table[j].kmin <= k) && (k <= modeI_table[j].kmax)) {
            k_prime = modeI_table[j].kmin;
            i = modeI_table[j].i;
            n = modeI_table[j].n;
            return M_PI / 2 * (geth_table(i, k - k_prime) + n);
        }
    }
    fprintf(stderr, "Help with %d\n", k);
    return 0;
}

void initConstPhaseReference(void) {
    int32_t	i;
    float	Phi_k;
    for (i = 1; i <= K / 2; i++) {
        Phi_k = get_Phi(i);
        refTable[i] = cosf(Phi_k) + sinf(Phi_k) * I;
        Phi_k = get_Phi(-i);
        refTable[T_u - i] = cosf(Phi_k) + sinf(Phi_k) * I;
    }
    //
    //      prepare a table for the coarse frequency synchronization
    for (i = 1; i <= DIFF_LENGTH; i++) {
        phaseDifferences[i - 1] = fabsf(cargf(refTable[(T_u + i) % T_u] *
            conjf(refTable[(T_u + i + 1) % T_u])));

    }
}

void initPhaseReference(struct sdr_state_t *sdr) {
    sdr->phaseReference.fftBuffer = fftwf_malloc(sizeof(fftwf_complex) * T_u);
    memset(sdr->phaseReference.fftBuffer, 0, sizeof(fftwf_complex) * T_u);
    sdr->phaseReference.plan = fftwf_plan_dft_1d(T_u, (float(*)[2]) sdr->phaseReference.fftBuffer, (float(*)[2])sdr->phaseReference.fftBuffer, FFTW_FORWARD, FFTW_ESTIMATE);
}

void destroyPhaseReference(struct sdr_state_t *sdr) {
    fftwf_destroy_plan(sdr->phaseReference.plan);
    fftwf_free(sdr->phaseReference.fftBuffer);
}

int32_t	phaseReferenceFindIndex(struct sdr_state_t *sdr, const float _Complex* v) {
    int32_t	i;
    int32_t	maxIndex = -1;
    float	sum = 0;
    float	Max = -10000;

    memcpy(sdr->phaseReference.fftBuffer, v, T_u * sizeof(fftwf_complex));
    fftwf_execute(sdr->phaseReference.plan);
    for (i = 0; i < T_u; i++) {
        sdr->phaseReference.fftBuffer[i] *= conjf(refTable[i]);
        sdr->phaseReference.fftBuffer[i] = conjf(sdr->phaseReference.fftBuffer[i]);
    }
    fftwf_execute(sdr->phaseReference.plan);
    /**
    *	We compute the average signal value ...
    */
    for (i = 0; i < T_u; i++) {
        float absValue = cabsf(sdr->phaseReference.fftBuffer[i]);
        sum += absValue;
        if (absValue > Max) {
            maxIndex = i;
            Max = absValue;
        }
    }
    /**
    *	that gives us a basis for defining the threshold
    */
    if (Max < 3 * sum / T_u) {
        return  -fabsf(Max * T_u / sum) - 1;
    } else
        return maxIndex;
}

#define SEARCH_RANGE    (2 * 35)
int16_t phaseReferenceEstimateOffset(struct sdr_state_t *sdr, const float _Complex* v) {
    int16_t i, j, index = 100;
    float   computedDiffs[SEARCH_RANGE + DIFF_LENGTH + 1];

    memcpy(sdr->phaseReference.fftBuffer, v, T_u * sizeof(fftwf_complex));
    fftwf_execute(sdr->phaseReference.plan);
    for (i = T_u - SEARCH_RANGE / 2;
        i < T_u + SEARCH_RANGE / 2 + DIFF_LENGTH; i++) {
        computedDiffs[i - (T_u - SEARCH_RANGE / 2)] =
            fabsf(cargf(sdr->phaseReference.fftBuffer[i % T_u] * conjf(sdr->phaseReference.fftBuffer[(i + 1) % T_u])));
    }
    float   Mmin = 1000;
    for (i = T_u - SEARCH_RANGE / 2; i < T_u + SEARCH_RANGE / 2; i++) {
        float sum = 0;
        for (j = 1; j < DIFF_LENGTH; j++)
            if (phaseDifferences[j - 1] < 0.1)
                sum += computedDiffs[i - (T_u - SEARCH_RANGE / 2) + j];
        if (sum < Mmin) {
            Mmin = sum;
            index = i;
        }
    }
    return index - T_u;
}
