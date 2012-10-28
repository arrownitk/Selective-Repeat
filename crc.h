//
// fisier crc.h
//
// contine directivele si definitiile utile programelor
// de calcul al codului de control ciclic CRC-CCITT
//

#define CRCCCITT 0x1021

#include <stdio.h>
#include <stdlib.h>
#include<unistd.h>
#include<string.h>


typedef unsigned short int word;
typedef unsigned char byte;

word calculcrc (word data, word genpoli, word acum);
void crctabel (word data, word* acum, word* tabelcrc);
int check_crc(msg *m, unsigned short int * tbl_crc);
word* tabelcrc (word poli);
word calc_crc(msg * t, word *tbl_crc);
