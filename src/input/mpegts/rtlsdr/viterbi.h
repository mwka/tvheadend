#ifndef _VITERBI_H
#define _VITERBI_H

//extern int mettab[2][256];

int init_viterbi(void);

int viterbi(void *p,unsigned char *symbols, unsigned char *data, unsigned int framebits);

#endif