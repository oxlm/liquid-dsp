/*
 * Copyright (c) 2007, 2009 Joseph Gaeddert
 * Copyright (c) 2007, 2009 Virginia Polytechnic Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
//
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "liquid.internal.h"

#if HAVE_FFTW3_H
#   include <fftw3.h>
#endif

#define DEBUG_OFDMOQAMFRAME64GEN            1

struct ofdmoqamframe64gen_s {
    unsigned int num_subcarriers;
    unsigned int m;
    float beta;

    // constants
    float zeta;         // scaling factor

    // filterbank objects
    ofdmoqam synthesizer;

    // transform objects
    float complex * X;
    float complex * x;

    // PLCP
    float complex * S0; // short sequence
    float complex * S1; // long sequence

    // pilot sequence
    msequence ms_pilot;
};

ofdmoqamframe64gen ofdmoqamframe64gen_create()
{
    ofdmoqamframe64gen q = (ofdmoqamframe64gen) malloc(sizeof(struct ofdmoqamframe64gen_s));
    q->num_subcarriers = 64;
    q->m = 2;
    q->beta = 0.7f;

    q->zeta = 1.0f;

    // create synthesizer
    q->synthesizer = ofdmoqam_create(q->num_subcarriers,
                                     q->m,
                                     q->beta,
                                     0.0f,  // dt
                                     OFDMOQAM_SYNTHESIZER,
                                     0);    // gradient

    // allocate memory for transform objects
    q->X  = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->x  = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));

    // allocate memory for PLCP arrays
    q->S0 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->S1 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    ofdmoqamframe64_init_S0(q->S0);
    ofdmoqamframe64_init_S1(q->S1);

    // set pilot sequence
    q->ms_pilot = msequence_create(8);

    return q;
}

void ofdmoqamframe64gen_destroy(ofdmoqamframe64gen _q)
{
    // free synthesizer object memory
    ofdmoqam_destroy(_q->synthesizer);

    // free transform array memory
    free(_q->X);
    free(_q->x);

    // free PLCP memory arrays
    free(_q->S0);
    free(_q->S1);

    // free pilot msequence object memory
    msequence_destroy(_q->ms_pilot);

    // free main object memory
    free(_q);
}

void ofdmoqamframe64gen_print(ofdmoqamframe64gen _q)
{
    printf("ofdmoqamframe64gen:\n");
    printf("    num subcarriers     :   %u\n", _q->num_subcarriers);
}

void ofdmoqamframe64gen_reset(ofdmoqamframe64gen _q)
{
}

void ofdmoqamframe64gen_writeshortsequence(ofdmoqamframe64gen _q,
                                           float complex * _y)
{
    // move short sequence to freq-domain buffer
    memmove(_q->X, _q->S0, (_q->num_subcarriers)*sizeof(float complex));

    // execute synthesizer, store result in output array
    ofdmoqam_execute(_q->synthesizer, _q->X, _y);
}


void ofdmoqamframe64gen_writelongsequence(ofdmoqamframe64gen _q,
                                          float complex * _y)
{
    // move long sequence to freq-domain buffer
    memmove(_q->X, _q->S1, (_q->num_subcarriers)*sizeof(float complex));

    // execute synthesizer, store result in output array
    ofdmoqam_execute(_q->synthesizer, _q->X, _y);
}

void ofdmoqamframe64gen_writeheader(ofdmoqamframe64gen _q,
                                float complex * _y)
{
}

// x    :   48
// y    :   64
void ofdmoqamframe64gen_writesymbol(ofdmoqamframe64gen _q,
                                    float complex * _x,
                                    float complex * _y)
{
    unsigned int pilot_phase = msequence_advance(_q->ms_pilot);

    // move frequency data to internal buffer
    unsigned int i, j=0;
    int sctype;
    for (i=0; i<_q->num_subcarriers; i++) {
        sctype = ofdmoqamframe64_getsctype(i);
        if (sctype==OFDMOQAMFRAME64_SCTYPE_NULL) {
            // disabled subcarrier
            _q->X[i] = 0.0f;
        } else if (sctype==OFDMOQAMFRAME64_SCTYPE_PILOT) {
            // pilot subcarrier
            _q->X[i] = (pilot_phase ? 1.0f : -1.0f) * _q->zeta;
        } else {
            // data subcarrier
            _q->X[i] = _x[j] * _q->zeta;
            j++;
        }

        //printf("X[%3u] = %12.8f + j*%12.8f;\n",i+1,crealf(_q->X[i]),cimagf(_q->X[i]));
    }
    assert(j==48);

    // execute synthesizer, store result in output array
    ofdmoqam_execute(_q->synthesizer, _q->X, _y);
}

