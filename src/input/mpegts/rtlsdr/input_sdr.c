/*
This file is part of rtl-dab
trl-dab is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Foobar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with rtl-dab.  If not, see <http://www.gnu.org/licenses/>.


david may 2012
david.may.muc@googlemail.com

*/

#include "tvheadend.h"
#include "tvhpoll.h"
#include "rtlsdr_private.h"
#include "dab.h"
#include "dab_tables.h"
#include "sdr_sync.h"

int readFromDevice(rtlsdr_frontend_t *lfe);

int readFromDevice(rtlsdr_frontend_t *lfe) {
	struct dab_state_t *dab = lfe->dab;
	struct sdr_state_t *sdr = &dab->device_state;
	tvhpoll_event_t ev[2];
	tvhpoll_t *efd;
	char b;
	int nfds;

	/* Setup poll */
	efd = tvhpoll_create(2);
	memset(ev, 0, sizeof(ev));
	ev[0].events = TVHPOLL_IN;
	ev[0].fd = lfe->lfe_control_pipe.rd;
	ev[0].ptr = lfe;
	ev[1].events = TVHPOLL_IN;
	ev[1].fd = lfe->lfe_dvr_pipe.rd;
	ev[1].ptr = &lfe->lfe_dvr_pipe;
	tvhpoll_add(efd, ev, 2);

	while (tvheadend_is_running() && lfe->lfe_dvr_pipe.rd > 0) {
		nfds = tvhpoll_wait(efd, ev, 1, 150);
		if (nfds < 1) continue;
		if (ev[0].ptr == &lfe->lfe_dvr_pipe) {
			if (read(lfe->lfe_dvr_pipe.rd, &b, 1) > 0) {
				return 0;
			}
			continue;
		}
		if (ev[0].ptr != lfe) break;
		if (read(lfe->lfe_control_pipe.rd, &b, 1) > 0) {
			tvhtrace(LS_RTLSDR, "fifo count %d", sdr->fifo.count);
			return 1;
		}
	}
	return 0;
}

uint8_t jan_abs(struct complex_t z) {
	int8_t re = z.real;
	int8_t im = z.imag;
	if (re < 0) re = -re;
	if (im < 0) im = -im;
	return re + im;
}

uint32_t getSamples(rtlsdr_frontend_t *lfe, struct complex_t *v, uint32_t size) {
	struct dab_state_t *dab = lfe->dab;
	struct sdr_state_t *sdr = &dab->device_state;
	uint32_t i;
	uint8_t buffer[2];
	for (i = 0; i < size; i++) {
		if (sdr->fifo.count < 2) {
			if (!readFromDevice(lfe)) {
				return i;
			}
		}
		sdr_read_fifo(&(sdr->fifo), 2, 0, buffer);
		v[i].real = buffer[0] - 127;
		v[i].imag = buffer[1] - 127;
		sdr->sLevel = 0.00001 * jan_abs(v[i]) + (1 - 0.00001) * sdr->sLevel;
	}
	return size;
}

//int sdr_demod(struct demapped_transmission_frame_t *tf, struct sdr_state_t *sdr){
//  int i,j;
//
//  tf->has_fic = 0;
//
//  /* resetting coarse freqshift */
//  sdr->coarse_freq_shift = 0;
//  
//  /* Check for data in fifo */
//  if (sdr->fifo.count < 196608*3) {
//    return 0;
//  }
//  
//  /* read fifo */
//  sdr_read_fifo(&(sdr->fifo),196608*2,sdr->coarse_timeshift+sdr->fine_timeshift,sdr->buffer);
//
//
//  
//  /* give the AGC some time to settle */
//  if (sdr->startup_delay<=GAIN_SETTLE_TIME) {
//    sdr->startup_delay+=1;
//	tvhdebug(LS_RTLSDR, "startup_delay=%i",sdr->startup_delay);
//    return 0;
//  }
//  
//
//
//  /* complex data conversion */
//  for (j=0;j<196608*2;j+=2){
//    sdr->real[j/2]=sdr->buffer[j]-127;
//    sdr->imag[j/2]=sdr->buffer[j+1]-127;
//  }
//
//  /* resetting coarse timeshift */
//  sdr->coarse_timeshift = 0;
//
//  /* coarse time sync */
//  /* performance bottleneck atm */
//  sdr->coarse_timeshift = dab_coarse_time_sync(sdr->real,sdr->filt,sdr->force_timesync);
//  // we are not in sync so -> next frame
//  sdr->force_timesync=0;
//  if (sdr->coarse_timeshift) {
//	tvhdebug(LS_RTLSDR, "coarse time shift");
//    return 0;
//  }
//
//  /* create complex frame */
//  for (j=0;j<196608;j++){
//    sdr->dab_frame[j][0] = sdr->real[j];
//    sdr->dab_frame[j][1] = sdr->imag[j];
//  }
//
//  /* fine time sync */
//  sdr->fine_timeshift = dab_fine_time_sync(sdr->dab_frame);
//  if (sdr->coarse_freq_shift) {
//    sdr->fine_timeshift = 0;
//    }
//  /* coarse_frequency shift */
//  fftw_plan p;
//  p = fftw_plan_dft_1d(2048, &sdr->dab_frame[2656+505+sdr->fine_timeshift], sdr->symbols[0], FFTW_FORWARD, FFTW_ESTIMATE);
//  fftw_execute(p);
//  fftw_destroy_plan(p);
//  
//  fftw_complex tmp;
//    for (i = 0; i < 2048/2; i++)
//    {
//      tmp[0]     = sdr->symbols[0][i][0];
//      tmp[1]     = sdr->symbols[0][i][1];
//      sdr->symbols[0][i][0]    = sdr->symbols[0][i+2048/2][0];
//      sdr->symbols[0][i][1]    = sdr->symbols[0][i+2048/2][1];
//      sdr->symbols[0][i+2048/2][0] = tmp[0];
//      sdr->symbols[0][i+2048/2][1] = tmp[1];
//    }
//  sdr->coarse_freq_shift = dab_coarse_freq_sync_2(sdr->symbols[0]);
//  if (abs(sdr->coarse_freq_shift)>1) {
//    sdr->force_timesync = 1;
//    return 0;
//  }
//
//  /* fine freq correction */
//  sdr->fine_freq_shift = dab_fine_freq_corr(sdr->dab_frame,sdr->fine_timeshift);
//
//  /* d-qpsk */
//  for (i=0;i<76;i++) {
//    p = fftw_plan_dft_1d(2048, &sdr->dab_frame[2656+(2552*i)+504],
//			 sdr->symbols[i], FFTW_FORWARD, FFTW_ESTIMATE);
//    fftw_execute(p);
//    fftw_destroy_plan(p);
//    for (j = 0; j < 2048/2; j++)
//      {
//	tmp[0]     = sdr->symbols[i][j][0];
//	tmp[1]     = sdr->symbols[i][j][1];
//	sdr->symbols[i][j][0]    = sdr->symbols[i][j+2048/2][0];
//	sdr->symbols[i][j][1]    = sdr->symbols[i][j+2048/2][1];
//	sdr->symbols[i][j+2048/2][0] = tmp[0];
//	sdr->symbols[i][j+2048/2][1] = tmp[1];
//      }
//    
//  }
//  //
//  for (j=1;j<76;j++) {
//    for (i=0;i<2048;i++)
//      {
//	sdr->symbols_d[j*2048+i][0] =
//	  ((sdr->symbols[j][i][0]*sdr->symbols[j-1][i][0])
//	   +(sdr->symbols[j][i][1]*sdr->symbols[j-1][i][1]))
//	  /(sdr->symbols[j-1][i][0]*sdr->symbols[j-1][i][0]+sdr->symbols[j-1][i][1]*sdr->symbols[j-1][i][1]);
//	sdr->symbols_d[j*2048+i][1] = 
//	  ((sdr->symbols[j][i][0]*sdr->symbols[j-1][i][1])
//	   -(sdr->symbols[j][i][1]*sdr->symbols[j-1][i][0]))
//	  /(sdr->symbols[j-1][i][0]*sdr->symbols[j-1][i][0]+sdr->symbols[j-1][i][1]*sdr->symbols[j-1][i][1]);
//      }
//  }
//  
//  uint8_t* dst = tf->fic_symbols_demapped[0];
//  tf->has_fic = 1;  /* Always true for SDR input */
//
//  int k,kk;
//  for (j=1;j<76;j++) {
//    if (j == 4) { dst = tf->msc_symbols_demapped[0]; }
//    k = 0;
//    for (i=0;i<2048;i++){
//      if ((i>255) && i!=1024 && i < 1793) {
//        /* Frequency deinterleaving and QPSK demapping combined */  
//        kk = rev_freq_deint_tab[k++];
//        dst[kk] = (sdr->symbols_d[j*2048+i][0]>0)?0:1;
//        dst[1536+kk] = (sdr->symbols_d[j*2048+i][1]>0)?1:0;
//      }
//    }
//    dst += 3072;
//  }
//  
//  return 1;
//}

void sdr_init(struct sdr_state_t *sdr)
{
  // circular buffer init
  cbInit(&(sdr->fifo),(196608*2*4)); // 4 frames

  sdr->sLevel = 0;
}
