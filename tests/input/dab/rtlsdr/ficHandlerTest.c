#include "ficHandler.h"

void process_ficInput(struct sdr_state_t *sdr, int16_t ficno);

int main(int argc, char** argv) {
    struct sdr_state_t sdr;
    FILE *pFile;
    uint8_t *output;
    
    memset(&sdr, 0, sizeof(sdr));
    
    sdr.mmi = calloc(1, sizeof(dab_ensemble_instance_t));
    sdr.mmi->mmi_ensemble = calloc(1, sizeof(dab_ensemble_t));
    
    initConstViterbi768();
    initFicHandler(&sdr);
    
    pFile = fopen ("input/dab/rtlsdr/ficHandlerTest/ficInput" , "rb" );
    fread (sdr.ofdm_input, 2, 2304, pFile);
    fclose(pFile);
    
    process_ficInput(&sdr, 0);
    
    printf("sdr.fibCRCsuccess %d\n", sdr.fibCRCsuccess);
    
    output = malloc(256);
    
    pFile = fopen ("input/dab/rtlsdr/ficHandlerTest/ficOutput" , "rb" );
    fread (output, 1, 256, pFile);
    fclose(pFile);
    
    printf("memcmp: %d\n", memcmp(output, &sdr.bitBuffer_out[512], 256));
    
    destroyFicHandler(&sdr);
    free(sdr.mmi->mmi_ensemble);
    free(sdr.mmi);
    free(output);
    
    exit(EXIT_SUCCESS);
}