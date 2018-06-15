#ifndef _PHASEREFERENCE_H
#define _PHASEREFERENCE_H

#include "tvheadend.h"
#include "dab_constants.h"

void initPhaseReference(struct sdr_state_t *sdr);
void destoryPhaseReference(struct sdr_state_t *sdr);

int32_t	phaseReferenceFindIndex(struct sdr_state_t *sdr, struct complex_t* v);

#endif