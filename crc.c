#include "lib.h"
#include "crc.h"

/*
==================================================
metodele pentru calculul sumei de control din laborator
==================================================
*/
word calculcrc (word data, word genpoli, word acum)
{
  int i;
  data <<= 8;
  for (i=8; i>0; i--)
     {
     if ((data^acum) & 0x8000)
       acum = (acum << 1) ^ genpoli;
     else
       acum <<= 1;
     data <<= 1;
     }
  return acum;
}
void crctabel (word data, word* acum, word* tabelcrc)
  {
  word valcomb;
  valcomb = ((*acum >> 8) ^ data) & 0x00FF;
  *acum = ((*acum & 0x00FF) << 8) ^ tabelcrc [valcomb];
}


word calc_crc(msg * t, word *tbl_crc)
{
    int k;
    word crc = 0;
    for (k = 0; k < t->len + 2; k++)
				crctabel(t->payload[k + 2], &crc, tbl_crc);
    return crc;
}



word* tabelcrc (word poli )
{
  word* ptabelcrc;
  int i;
  if ((ptabelcrc = (word*) malloc (256 * sizeof (word))) == NULL)
     return NULL;
  for (i=0; i<256; i++)
    ptabelcrc [i] = calculcrc (i, poli, 0);
  return ptabelcrc;
}



