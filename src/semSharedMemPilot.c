/**
 *  \file semSharedMemPilot.c (implementation file)
 *
 *  \brief Problem name: Air Lift
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the pilot:
 *     \li flight
 *     \li signalReadyForBoarding
 *     \li waitUntilReadyToFlight
 *     \li dropPassengersAtTarget
 *
 *  \author Nuno Lau - January 2022
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"


/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

static void flight (bool go);
static void signalReadyForBoarding ();
static void waitUntilReadyToFlight ();
static void dropPassengersAtTarget ();
static bool isFinished ();

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the pilot.
 */

int main (int argc, char *argv[])
{
    int key;                                                           /*access key to shared memory and semaphore set */
    char *tinp;                                                                      /* numerical parameters test flag */

    /* validation of command line parameters */

    if (argc != 4) { 
        freopen ("error_PT", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else freopen (argv[3], "w", stderr);
    strcpy (nFic, argv[1]);
    key = (unsigned int) strtol (argv[2], &tinp, 0);
    if (*tinp != '\0') {
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */

    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

    srandom ((unsigned int) getpid ());                                                 /* initialize random generator */

    /* simulation of the life cycle of the pilot */

    while(!isFinished()) {
        flight(false); // from target to origin
        signalReadyForBoarding();
        waitUntilReadyToFlight();
        flight(true); // from origin to target
        dropPassengersAtTarget();
    }

    /* unmapping the shared region off the process address space */

    if (shmemDettach (sh) == -1) { 
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief test if air lift finished
 */
static bool isFinished ()
{
    return sh->fSt.finished;
}

/**
 *  \brief flight.
 *
 *  The pilot takes passenger to destination (go) or 
 *  plane back to starting airport (return)
 *  state should be saved.
 *
 *  \param go true if going to destination
 */

static void flight (bool go)
{
    if (semDown (semgid, sh->mutex) == -1) {                                                      /* enter critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */

    if (go == true) {                           //caso seja true
        sh->fSt.st.pilotStat = FLYING;          //caso seja true significa que o piloto está a viajar de origin para target
        saveState(nFic, &sh->fSt);
    }else{
        sh->fSt.st.pilotStat = FLYING_BACK;     //caso contrário está a está a viajar de target para origin
        saveState(nFic, &sh->fSt);
    }
    
    if (semUp (semgid, sh->mutex) == -1) {                                                      /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    usleep((unsigned int) floor ((MAXFLIGHT * random ()) / RAND_MAX + 100.0));
}

/**
 *  \brief pilot informs hostess that plane is ready for boarding
 *
 *  The pilot updates its state and signals the hostess that boarding may start
 *  The flight number should be updated.
 *  The internal state should be saved.
 */

static void signalReadyForBoarding ()
{
    if (semDown (semgid, sh->mutex) == -1) {                                                      /* enter critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */

    sh->fSt.st.pilotStat = READY_FOR_BOARDING;                  //piloto atualiza o seu estado, sinalizando que o avião está pronto para boarding
    saveState(nFic, &sh->fSt);                                  //guardar estado inicial
    sh->fSt.nFlight++;                                          //atualiza o número do voo
    saveStartBoarding(nFic, &sh->fSt);                          //guardar estado inicial do logging num ficheiro

    if (semUp (semgid, sh->mutex) == -1) {                                                      /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */

    if (semUp(semgid, sh->readyForBoarding) == -1) {            //piloto notifica hostess que o boarding pode começar
        perror("error on the down operation for semaphore access (PT)");
        exit(EXIT_FAILURE);
    }
}

/**
 *  \brief pilt waits for plane to get filled with passengers.
 *
 *  The pilot updates its state and wait for Boarding to finish 
 *  The internal state should be saved.
 */

static void waitUntilReadyToFlight ()
{
    if (semDown (semgid, sh->mutex) == -1) {                                                      /* enter critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */

    sh->fSt.st.pilotStat = WAITING_FOR_BOARDING;                //piloto atualiza o seu estado para WAITING_FOR_BOARDING, sinalizando que està à espera dos passageiros entrarem
    saveState(nFic, &sh->fSt);


    if (semUp (semgid, sh->mutex) == -1) {                                                      /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    if (semDown(semgid, sh->readyToFlight) == -1) {             //semáforo utilizado pelo piloto para esperar que o boarding esteja completo
        perror ("error on the down operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

}

/**
 *  \brief pilot drops passengers at destination.
 *
 *  Pilot update its state and allows passengers to leave plane
 *  Pilot must wait for all passengers to leave plane before starting to return.
 *  The internal state should not be saved twice (after allowing passengeres to leave and after the plane is empty).
 */

static void dropPassengersAtTarget ()
{
    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */

    saveFlightArrived(nFic, &sh->fSt);
    sh->fSt.st.pilotStat = DROPING_PASSENGERS;              //piloto atualiza o seu estado para DROPING_PASSENGERS, sinalizando que está a deixar os passageiros saírem
    saveState(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1)  {                                                   /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    for (int i = 0; i < sh->fSt.nPassengersInFlight[sh->fSt.nFlight-1]; i++){   //ciclo for que percorrer o número de passageiros no avião
        if (semUp(semgid, sh->passengersWaitInFlight) == -1){                   //semáforo usado pelos passageiros para esperar pelo piloto, vai estar up
            perror("error on the up operation for semaphore access (PT)");
            exit(EXIT_FAILURE);
        }
    }

    if (semDown (semgid, sh->planeEmpty) == -1) {                         //semáforo utilizado pelo piloto para esperar pelo último passageiro para sair do avião                       
        perror ("error on the down operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }


    if (semDown (semgid, sh->mutex) == -1) {                                                  /* enter critical region */
        perror ("error on the down operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }

    /* insert your code here */
    sh->fSt.st.pilotStat = FLYING_BACK;              //piloto atualiza o seu estado para FLYING_BACK, sinalizando que está a regressar após deixar os passageiros
    saveState(nFic, &sh->fSt);
    saveFlightReturning(nFic, &sh->fSt);

    if (semUp (semgid, sh->mutex) == -1)  {                                                   /* exit critical region */
        perror ("error on the up operation for semaphore access (PT)");
        exit (EXIT_FAILURE);
    }
}

