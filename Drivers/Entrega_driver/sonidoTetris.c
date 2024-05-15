/*************************************************
 * Prueba de sonido con /dev/speaker
 * 
 * 
 * compilar:
 * gcc sonidoTetris.c -o sonidoTetris
 * 
 * 
 * *********************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define DEVICE      "/dev/speaker"
#define LOW_LEVEL   '0'
#define HIGH_LEVEL  '1'

int fd_speaker;

// frecuencias
const int c = 261;
const int d = 294;
const int e = 329;
const int f = 349;
const int g = 391;
const int gS = 415;
const int a = 440;
const int aS = 455;
const int b = 466;
const int cH = 523;
const int cSH = 554;
const int dH = 587;
const int dSH = 622;
const int eH = 659;
const int fH = 698;
const int fSH = 740;
const int gH = 784;
const int gSH = 830;
const int aH = 880;

// duraciones
const int blanca = 500;
const int negra = 250;
const int corch = 125;

void flanco_altavoz()
{
    static char state = 0;
    static char c;

    state = !state;    
    if (state)
        c = HIGH_LEVEL;
    else
        c = LOW_LEVEL;

    write(fd_speaker, &c, 1);
}


#define TIME_ADD(ts1, ts2)                  \
    do {                                    \
        ts1.tv_sec  += ts2.tv_sec;          \
        ts1.tv_nsec += ts2.tv_nsec;         \
        if (ts1.tv_nsec >= 1000000000) {    \
            ts1.tv_nsec -= 1000000000;      \
            ts1.tv_sec += 1;                \
        }                                   \
    } while(0)
    
static inline int TIME_CMP(struct timespec ts1, struct timespec ts2)
{
    if (ts1.tv_sec > ts2.tv_sec) return 1;
    if (ts1.tv_sec < ts2.tv_sec) return 0;
    return (ts1.tv_nsec > ts2.tv_nsec);
}

static inline struct timespec TIME_MIN(struct timespec ts1, struct timespec ts2)
{
    if (ts1.tv_sec < ts2.tv_sec) return ts1;
    if (ts1.tv_sec > ts2.tv_sec) return ts2;
    if (ts1.tv_nsec < ts2.tv_nsec) return ts1;
    return ts2;
}

static inline void delay(long tiempo)
{
    struct timespec ts1 = {0, tiempo*1000000}, ts2;
    clock_nanosleep(CLOCK_MONOTONIC, 0, &ts1, &ts2);
}

// freq in Herz
// duration in miliseconds
void toneManual(int frequency, int duration)
{
    double dperiod = 1.0/frequency;
    long iperiod = dperiod;
    long fperiod = (dperiod-iperiod)*1000000000;
    struct timespec period = {iperiod, fperiod};
    struct timespec length = {0, duration*1000000};
    struct timespec next, end, min;
    clock_gettime(CLOCK_MONOTONIC, &next);
    end = next;
    TIME_ADD(end, length);
    TIME_ADD(next, period);
    next = TIME_MIN(end, next);
    do {
        flanco_altavoz();
        TIME_ADD(next, period);
        min = TIME_MIN(end, next);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &min, NULL);
    } while (TIME_CMP(end, next));
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &end, NULL);
}

void Beep(int note, int duration)
{
    //Play tone on buzzerPin
    toneManual(note, duration);
    delay(20);
}

void firstSection()
{
    Beep(1320,blanca);Beep(990,negra);Beep(1056,negra);Beep(1188,negra);
    Beep(1320,corch);Beep(1188,corch);Beep(1056,negra);Beep(990,negra);
    Beep(aH,blanca);Beep(aH,negra);Beep(1056,negra);Beep(1320,blanca);
    Beep(1188,negra);Beep(1056,negra);Beep(990,750);Beep(1056,negra);
    Beep(1188,blanca);Beep(1320,blanca);Beep(1056,blanca);Beep(aH,blanca);
    Beep(aH,blanca);Beep(1188,blanca);Beep(1408,negra);Beep(1760,blanca);
    Beep(1584,negra);Beep(1408,negra);Beep(1320,750);Beep(1056,negra);
    Beep(1320,blanca);Beep(1188,negra);Beep(1056,negra);Beep(990,blanca);
    Beep(990,negra);Beep(1056,negra);Beep(1188,blanca);Beep(1320,blanca);
    Beep(1056,blanca);Beep(aH,blanca);Beep(aH,blanca);
}


int main (int argc, char** argv)
{

    printf ("\n") ;
    printf ("Raspberry Pi - prueba de sonido en /dev/speaker\n") ;
    printf ("===============================================\n") ;
    printf ("\n") ;

    if(argc>1)
    {
        struct sched_param sp;
        sp.sched_priority=85;
        pthread_setschedparam(pthread_self(),SCHED_FIFO, &sp);
    }
    
    printf("Abriendo device %s\n",DEVICE);
    
    fd_speaker=open(DEVICE, O_WRONLY);
    
    if(fd_speaker<0) 
    {
        printf("ERROR al abrir el device...\n");
        exit(-1);
    }
    
    //Play first section
    firstSection();

    exit(0);

}
