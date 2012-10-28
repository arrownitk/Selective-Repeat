// DUMITRESCU EVELINA 321CA Tema1 PC

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/timeb.h>
#include <signal.h>
#include "lib.h"
#include "crc.h"

#define ACK 3
#define NAK 4	
#define HOST "127.0.0.1"
#define PORT 10000
#define FIRST_FRAME 6000
#define TRUE 1
#define FALSE 0

int is_armed;
int speed, delay;
int dim_vector=0;
unsigned short int BUF;
int id_ack;		
msg first_frame;
msg buffer[4500];
int q[6500];	
unsigned short MAX_SEQ;
unsigned short int *crc_table;
struct stat buf;

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
========================================================
  incrementeaza circular pe k pentru deplasarea ferestrei
========================================================
*/
void increment(unsigned short int *k, int max_seq)
{
	 (*k)=((*k)<max_seq)?(*k)+=1:0;
}

/*
=========================================================
  sterge un element din coada
==========================================================
*/
void Remove( int seq)
{
    int k, poz = 0, ok = 0;
    while (ok == 0) 
		{
		  for (k = poz; k < dim_vector; k++)
	    	if (q[k] == seq)
		     {
					poz= k;
					break;
		    }
				if (k == dim_vector)
						    ok = 1;
				if (poz < dim_vector) 
				{
					    for (k = poz; k < dim_vector - 1; k++)q[k] = q[k + 1];
					    dim_vector--;
	      }
    }

}



/*
======================================================================
  determina dimensiunea maxima a ferestrei
======================================================================
*/
unsigned short int getMAX_SEQ(int speed, int delay)
{
	unsigned short max =  (speed * delay* 2000) /(8 * sizeof(msg));
	int pow[]={(1<<1)-1, (1<<2)-1,(1<<3)-1, (1<<4)-1, (1<<5)-1, (1<<6)-1,(1<<7)-1,(1<<8)-1,(1<<9)-1,(1<<10)-1};
	int i = 1;
	int n = 10;
	if(max > 1000)max = 1000;
	int p=1;
	for(i =1;i<=10;i++)
	{
			if(pow[i-1]>max) 
			{
					p= pow[i-2];
					break;
		}

	}
	return p;	

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
======================================================================
  metoda armeaza alarma
======================================================================
*/
void arm()
{
		int const_delay = 2* delay*1000 ;
		ualarm(const_delay, 0);
		is_armed =1;

}
/*
=====================================================================
  metoda dezarmeaza alarma
=====================================================================
*/
 void unarm()
{
		ualarm(0, 0);
		is_armed =0;



}

/*
===============================================================
  metoda construieste un mesaj ce urmeaza sa fie trimis 
===============================================================
*/
msg build_message( unsigned short int max_seq,char * filename)
{
	/*mesajul contine 
	  - crc (2)
	  - max_seq (2)
	  - nume fisier
	  - dimensiunea fisierului 
	  - delay  
	*/
	msg t;
	t.type = 1;

	t.len = strlen(filename) + 1;
	filename[t.len-1] = 0;
	memcpy(t.payload+8+strlen(filename) + 1,&delay,4);
	memcpy(t.payload+4+strlen(filename) + 1,&buf.st_size,4);	
	memcpy(t.payload+4,filename,strlen(filename) + 1);
	memcpy(t.payload+2,&max_seq,2);	

	unsigned short int crc = calc_crc(&t,crc_table);
	memcpy(t.payload, &crc, 2);
	return t;

}

/*
============================================================
  alarm handler
============================================================
*/
static void handler(int sig)
{
    if (id_ack == 0)
	 {
		arm();
		send_message(&first_frame);
		return;
   }
	 else
	 {
			if (dim_vector != 0) 
			{
		    int p = q[0];
				Remove(q[0]);
		    if (!is_armed) 
				{
					arm();
					q[dim_vector++] = p;

    		}
				else 
				{
					Remove(p);
					q[dim_vector++] = p;
					

		    }
		    int i;
        for(i =0;i<BUF;i++)
	  	    send_message(buffer + i);
	    	return;

			}
			else 
			{
		    dim_vector = 0;
			  unarm();
			}
   }

}

/*
======================================================
  initializeaza alarma
=======================================================
*/
void init_alarm()
{
	signal(SIGALRM, handler);	
	siginterrupt(SIGALRM, 1);	

}
/*
=================================================
trimite primul cadru 
=================================================
*/
void send_first_frame(char * filename)
{
   	msg t,*r;
  	unsigned short int crc;
		unsigned short payload_crc;

		/* trimite primul mesaj cu numele fisierului si cu dimensiunea maxim a nr de secventa */
		t=build_message( MAX_SEQ, filename);
		first_frame = t;
		arm();
		send_message(&t);
		id_ack = 0;
		for(;;)
		{
		  /* primeste un mesaj din partea receiverului */
			r = receive_message();
			if (r != NULL)
			 {
		    unsigned short int read_seq;
		    memcpy(&read_seq, r->payload + 2, 2);
			 	crc = calc_crc(r, crc_table);
    		memcpy(&payload_crc, r->payload, 2);
        /* daca mesajul este nedeteriorat si este de tip ACK 
            atunci dezarmeaza alarma 
        */
		    if ((payload_crc == crc) && r->type == ACK && (read_seq == FIRST_FRAME))
				 {
					unarm();
					memcpy(&MAX_SEQ, r->payload+4,2);	

					id_ack = 1;
					break;
	  		  }
	  		  /* daca mesajul este  de tip NCK 
            atunci armeaza alarma si retrimite pachetul
          */
	  	  if ((payload_crc == crc) && r->type == NAK && (read_seq == FIRST_FRAME)) 
				{
					arm();
					send_message(&t)	;
	  	  }
			}

    }

}
/*
=============================================================
metoda trimite continutul fisierului
=============================================================
*/
void send_file(char *filename)
{
    msg t, *r;
		int is_finished = 0;
	unsigned short int next_ack = 0;
	unsigned short int	 next_fr = 0;
	unsigned short int  nr_buf = 0;
	/* initializeaza alarma */
    init_alarm();
    if (stat(filename, &buf) < 0)
	 {
		perror("Stat failed");
		return;
   }

    int fd = open(filename, O_RDONLY);

    if (fd < 0)
	 {
		perror("Couldn't open file");
		return;
    }
    /*determin numarul maxim de secventa*/
    MAX_SEQ = getMAX_SEQ(speed,delay);
    /* dimensiunea maxima a unei ferestre*/
    BUF = (MAX_SEQ + 1) / 2;
    /*determina  tabela crc*/
		crc_table = tabelcrc(CRCCCITT);
    unsigned short int crc;

   dim_vector = 0;
    
		unsigned short payload_crc;
		/* trimte primul cadru*/
		send_first_frame(filename);
		BUF = (MAX_SEQ + 1) / 2;
    is_finished = 0;
    t.type = 2;

    while (is_finished == FALSE)
	 {
      /* cat timp mai este spatiu liber in buffer */
			for(;nr_buf < BUF;nr_buf++)
		 {
		    /*citesc chunkuri din fisier */
		    t.len = read(fd, t.payload + 4, 1396);
		    /*daca numarul de caractere intors de read este 0
		    sau daca nu mai am elemente un buffer break;
		    
		    */
	  	 is_finished=(t.len == 0 && nr_buf == 0)?TRUE:FALSE;
		    if (t.len == 0)break;
		    memcpy(t.payload + 2, &next_fr, 2);
        /* calculez CRC-ul si il pun in frame*/
		    crc = calc_crc(&t, crc_table);
		    memcpy(t.payload, &crc, 2);
				int ind = next_fr% BUF;
				/*adauga mesajul in buffer si deplasez fereastra*/
		    buffer[ind] = t;
		   	increment(&next_fr, MAX_SEQ);
				if (!is_armed) 
				{
					arm();
					int elem = next_fr % BUF;
					q[dim_vector++] = elem;
					
    		}
				else 
				{
					int elem =next_fr % BUF; 
					Remove(elem);
					q[dim_vector++] = elem;

		    }

	    send_message(&t);
		}
		/* daca am terminat de transmis opresc alarma */
		if (is_finished == TRUE) 
		{
		   unarm();
			 break;
		}
		/*astept mesaj de la reciever */
		r = receive_message();
		if (r != NULL)
		{
  		/*calculez crc-ul si verfici daca mesajul este nedeteriorat*/
			crc = calc_crc(r, crc_table);
    	memcpy(&payload_crc, r->payload, 2);
			if ((payload_crc == crc)) 
			{
						unsigned short int is_ack_nak;
						memcpy(&is_ack_nak, r->payload + 2, 2);
						/* daca mesajul primit este de tip ACK */
						if (r->type == ACK)
  					{
              /* daca mesajul se incadreaza in limitele indicilor de secventa ale ferestrei*/
				    	while (between(next_ack, is_ack_nak, next_fr))
		 					{
									nr_buf --;
									int elem = next_ack % BUF; 
									 /*elimin elementul din coada si opresc alarma*/ 
									Remove(elem);			
									if (dim_vector == 0 || nr_buf	 == 0) 
									{
										   unarm();
									}
									/*deplasez fereastra*/
									increment(&next_ack, MAX_SEQ);

		    			}	
						}
						int elem = (is_ack_nak + 1) % (MAX_SEQ + 1) % BUF;
						/*daca mesajul este de tip NAK*/
						if( (r->type == NAK) &&(between(next_ack, (is_ack_nak + 1) % (MAX_SEQ + 1), next_fr)) )
						{
					    if (!is_armed)
							 {
									arm();
									q[dim_vector++] =elem;					

					    } 
							else
							{

									Remove(elem);
									q[dim_vector++] = elem;


					    }
			      /*retrimit elementul */
						send_message(buffer + elem);
		    	}
				
	    }
		}
   }
   close(fd);
}

/*
====================================================
  main
====================================================
*/
int main(int argc, char **argv)
{
    init(HOST, PORT);

    if (argc < 4) {
	printf("Usage %s filename speed(Mb/s) delay(ms)\n", argv[0]);
	return -1;
    }
    /* determin delay si speed */
    delay = extract_parameters(argv[3]);
		speed = extract_parameters(argv[2]);
		/* incep transmiterea fisierului */
    send_file(argv[1]);
    return 0;
}
