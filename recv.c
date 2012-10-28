//Evelina Dumitrescu Tema1 PC
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"
#include "crc.h"
#define HOST "127.0.0.1"
#define PORT 10001
#define FIRST_FRAME 6000
#define ACK 3
#define NAK 4
#define DATA 2
#define FIRST 1
unsigned short int *crc_table;
unsigned short int MAX_SEQ = 0, next_seq = 0;
char filename[1400];
int size, delay;
msg *r;
int fd;

/*
========================================================
  metoda extrage parametri send & delay 
  primiti in linie de comanda
========================================================
*/

int extract_parameters(char *argv)
{
		char * p =strchr(argv,'=');
		int nr=0;
		nr=atoi(p+1);
		return nr;	
}

/*
=========================================================================
  metoda intoarce true daca a<=b<c in mod circular
  fals altfel
=========================================================================
*/

int between(int a, int b, int c)
{
    return (((a <= b) && (b < c)) || ((c < a) && (a <= b))|| ((b < c) && (c < a)));
}


/*
========================================================
  incrementeaza circular pe k pentru deplasarea ferestrei
========================================================
*/
void increment(unsigned short int *k, int MAX_SEQ)
{
	 (*k)=((*k)<MAX_SEQ)?(*k)+=1:0;
}

/*
===========================================================
  trimite un mesaj de tip ack pentru primul pachet 
  trimis de sender(include dimensiunea ferestrei aleasa ca 
  minimul dintre parametrul window si cel trimis de sender 
  intial)
===========================================================
*/
void send_ack2(unsigned short seq_nr, unsigned short int *tbl_crc, unsigned short int window)
{
    msg t;
    t.type = ACK;
    memcpy(t.payload + 2, &seq_nr, 2);
    memcpy(t.payload + 4, &window, 2);
    unsigned short int crc = calc_crc(&t, tbl_crc);
    memcpy(t.payload, &crc, 2);
    send_message(&t);
}


/*
=======================================================================
  metoda construieste un mesaj ce urmeaza sa fie trimis
=======================================================================
*/msg message(unsigned short int seq, char * type)
{

	msg m;
	if(strcmp(type, "ACK") == 0)	m.type = 3;
	else m.type = 4;
	m.len = strlen(m.payload+4)+1;
	sprintf(m.payload+4,type);
	memcpy(m.payload+2,&seq,2);
	unsigned short int crc = calc_crc(&m,crc_table);
	memcpy(m.payload,&crc,2);
	return m;

}

/*
=======================================================================
  trimit primul frame
=======================================================================
*/
void send_first_frame(int window)
{
	unsigned short int crc;
	unsigned short payload_crc;
	for(;;)
		{
			r = receive_message_timeout(2*delay);
			if (r != NULL)
			 {
			    /*daca a sosot un mesaj care nu este de tipul FIRSt trimit NAK */
			    if (r->type != FIRST) 
					{
						msg m=message(FIRST_FRAME,"NAK");
						send_message(&m);

			    }
				 else
					{
			      /*calculez crc-ul si verfic daca mesajul este sau nu deteriorat*/
						crc = calc_crc(r, crc_table);
    				memcpy(&payload_crc, r->payload, 2);
						if ((payload_crc == crc)) 
						{
						      int len=r->len;
						      /*determin valorile pentru 
						        - delay
                    - numele fisierului 
                    - MAX_SEQ 
                    - dimensiunea fisierului*/
							    memcpy(&delay, r->payload + len + 8, 4);
							    memcpy(filename, r->payload + 4, len);
							    memcpy(&MAX_SEQ, r->payload + 2, 2);
							    memcpy(&size, r->payload + len + 4, 4);
							    /* 
							      determin minminimul dintre valoarea parametrului window si dimensiunea ferestrei senderului
							     */
							     int min ;
									if(window != 0) min= ((MAX_SEQ+1)/2>window)?window:(MAX_SEQ+1)/2;
									if(window!=0)MAX_SEQ =min *2 - 1; 
							    send_ack2(FIRST_FRAME, crc_table, MAX_SEQ);
							    break;
						} 
						else
						 {
						      /*daca mesajul este corupt trimit nak*/
									msg m=message(FIRST_FRAME,"NAK");
									send_message(&m);

		
					}
	    		}
			}	
			else
			{ 
  			  /*daca am primt un mesaj gol trimit nak*/
					msg m=message(FIRST_FRAME,"NAK");
					send_message(&m);
			}
    }

}

/*
=======================================================
  metoda trimite mesaje de confirmare/neconfirmare 
  inapoi catre sender pentru toate pachetele primite
========================================================
*/
void send_data(unsigned short int BUF)
{
  unsigned short int new_seq = 0, far = BUF;
	int i;
  msg *buffer=calloc(1000,sizeof(msg));
	int *arrived_messages=calloc(BUF,sizeof(int));
	unsigned short int crc;
	unsigned short payload_crc;
	/*cat timp nu am primit intreg continutul fisierului*/
	for (;size > 0;)
  	{
  	  /*primesc mesaj de la sender*/
			r = receive_message_timeout( delay);
			if (r != NULL) 
			{
			    /*daca nu este mesaj de tip 2 (de date) trimit nak*/
			    if (r->type != DATA) 
					{			
							msg m = message((new_seq + MAX_SEQ) % (MAX_SEQ + 1),"NAK");
							send_message(&m);
					
			    }
				 	else
					 {
					    /*altfel verfici dac numarul de secventa se incadreaza in fereastra curenta
					      si daca bufferul este liber pe acea pozitie
					    */
							memcpy(&next_seq, r->payload + 2, 2);
							if (between(new_seq, next_seq, far))
							if(arrived_messages[next_seq % BUF] == 0)
							{
							  /*calculez crc-ul*/
								crc = calc_crc(r, crc_table);
					    	memcpy(&payload_crc, r->payload, 2);
					    	/*daca am primit un mesaj nedeteriorat*/
						    if ((payload_crc == crc)) 
								{
								    /*marchez bufferul ca fiind ocupat pe acea pozitie*/
										arrived_messages[next_seq % BUF] = 1;
								    buffer[next_seq % BUF] = *r;
										while (arrived_messages[new_seq % BUF])
										{
										size =size- buffer[new_seq % BUF].len;							
								    arrived_messages[new_seq % BUF] = 0;
								    /*scriu datele in fisier*/
								    write(fd, buffer[new_seq % BUF].payload + 4,buffer[new_seq % BUF].len);
								    /*deplasez fereastra*/
			    					increment(&new_seq, MAX_SEQ);
			    					increment(&far, MAX_SEQ);
											}
                /*trimit mesaj de confirmare*/
								msg m = message((new_seq + MAX_SEQ) % (MAX_SEQ + 1),"ACK");
								send_message(&m);
								if (size == 0) break;

			    		} 
							else 
							{
							  /*mesaj corupt*/
								msg m = message((new_seq + MAX_SEQ) % (MAX_SEQ + 1),"NAK");
								send_message(&m);
					    }
					} 
					else 
					{
					  /* nu se incadreaza in limitele ferestrei*/
						msg m = message((new_seq + MAX_SEQ) % (MAX_SEQ + 1),"NAK");
						send_message(&m);
					}
	    }
	}
	 else
	 {	/*am primit mesaj gol*/			
			msg m = message((new_seq + MAX_SEQ) % (MAX_SEQ + 1),"NAK");
			send_message(&m);

	}

}

}
/*
==================================================================
  MAIN
==================================================================
*/
int main(int argc, char **argv)
{

	
    init(HOST, PORT);
    /*calculez tabel crc*/
		crc_table = tabelcrc(CRCCCITT);
    /*trimit primul cadru*/
		send_first_frame(extract_parameters(argv[1]));
    char *filename2=calloc(1000,sizeof(char));
    unsigned short BUF = (MAX_SEQ + 1) / 2;
    strcpy(filename2,"recv_");
		strcat(filename2,filename);
		
    fd = open(filename2, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) 
		{
				perror("Failed to open file\n");
    }
    /*trimit datele*/
		send_data(BUF);	
    close(fd);
    return 0;
}
