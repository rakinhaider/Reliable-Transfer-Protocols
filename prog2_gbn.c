#include "prog2.h"

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

#define BUFFER_SIZE 150
#define ind(x) (x) % BUFFER_SIZE
#define RTO_TIMEOUT 20
#define DEBUG 0
#define min(x, y) (x) > (y) ? (y) : (x)

#define FINAL 0
#define R 1
#define T 0

int is_corrupt(struct pkt packet);

struct pkt send_pkt_a[BUFFER_SIZE];
struct pkt send_pkt_b;

int in_flight[BUFFER_SIZE];

int base;
int next_seqnum;
int N;
int expected_seqnum;

void print_message(struct msg message){
    printf("Message Received:");
    printf("%.20s\n", message.data);
}

void print_packet(struct pkt packet, int RorT)
{
    if(is_corrupt(packet))printf("Corrupted ");
    if(RorT == R)printf("Packet Received:");
    else if(RorT ==T)printf("Packet Transmitted:");
    else printf("Packet Buffered:");
    if(packet.seqnum >= 0 && packet.acknum >= 0){
        printf("secnum = %d, acknum = %d, checksum = %d, data = %.20s\n",
            packet.seqnum,
            packet.acknum,
            packet.checksum,
            packet.payload);
    }
    else if(packet.seqnum >= 0){
        printf("secnum = %d, checksum = %d, data = %.20s\n",
            packet.seqnum,
            packet.checksum,
            packet.payload);
    }else if(packet.acknum >= 0){
        printf("acknum= %d data=\n", packet.acknum);
    }
}

int get_checksum(struct pkt packet)
{
    int checksum, i;
    checksum = packet.seqnum;
    checksum += packet.acknum;
    for(i = 0; i < 20; i++)
    {
        checksum += packet.payload[i] << 8*(i%4);
        //printf("%d %c %d %d\n", i, packet.payload[i],
        // packet.payload[i], packet.payload[i] << 8*(i%4));
    }
    return ~checksum;
}

struct msg extract_data(struct pkt packet)
{
    int i;
    struct msg message;
    for(i = 0; i < 20; i++)
        message.data[i] = packet.payload[i];
    return message;
}

struct pkt make_packet(int seqnum, int acknum, char* data, int datalen)
{
    int i;
    struct pkt packet;
    packet.seqnum = seqnum;
    packet.acknum = acknum;
    for(i = 0; i < datalen; i++)
        packet.payload[i] = data[i];
    packet.checksum = get_checksum(packet);
    return packet;
}

void refuse_data(struct msg message){
    if(next_seqnum == base + BUFFER_SIZE){
        printf("Buffer Full");
        exit(1);
    }
    send_pkt_a[ind(next_seqnum)] = make_packet(next_seqnum, -1, message.data, 20);
    if(FINAL)print_packet(send_pkt_a[ind(next_seqnum)], -1);
    next_seqnum++;
}

int is_corrupt(struct pkt packet){
    int checksum = get_checksum(packet);
    // printf("%d %d %d\n", checksum, ~checksum, packet.checksum);
    if(~checksum + packet.checksum == ~((int)0))
        return 0;
    return 1;
}

int has_expected_seqnum(struct pkt packet){
    return expected_seqnum == packet.seqnum;
}

/* called from layer 5, passed the data to be sent to other side */
int A_output(struct msg message)
{
    (void)message;
    if(FINAL)print_message(message);
    if(next_seqnum < base + N){
        send_pkt_a[ind(next_seqnum)] = make_packet(next_seqnum, -1, message.data, 20);
        if(FINAL)print_packet(send_pkt_a[ind(next_seqnum)], T);
        tolayer3(A, send_pkt_a[ind(next_seqnum)]);
        if(base == next_seqnum)
            starttimer(A, RTO_TIMEOUT);
        next_seqnum++;
    }else
        refuse_data(message);
    if(DEBUG){
        printf("nextseqnum %d\n", next_seqnum);
        print_packet(send_pkt_a[ind(next_seqnum-1)], T);
        if(!(next_seqnum < base + N))printf("Packet buffered.\n");

    }
    return 0;
}

/* called from layer 3, when a packet arrives for layer 4 */
int A_input(struct pkt packet)
{
    (void)packet;
    int i, prev_end, endwnd;
    if(FINAL)print_packet(packet, R);
    if(DEBUG){
        if(is_corrupt(packet))
            printf("Ack corrupted\n");
        else print_packet(packet, R);
    }
    if(!is_corrupt(packet)){
        if(base == packet.acknum + 1) return 0;
        prev_end = base + N;
        base = packet.acknum + 1;
        if(base == next_seqnum)
            stoptimer(A);
        else{
            stoptimer(A);
            endwnd = min(next_seqnum, base + N);
            if(endwnd > prev_end){
                if(DEBUG)printf("New packet in window\n");
                for(i = prev_end; i < endwnd; i++){
                    if(DEBUG)
                        printf("Transmitting %d\n", i);
                    if(FINAL)print_packet(send_pkt_a[ind(i)], T);
                    tolayer3(A, send_pkt_a[ind(i)]);
                }
            }
            starttimer(A, RTO_TIMEOUT);
        }
    }
    if(DEBUG){
        printf("Window: ");
        for(i=base; i < base + N; i++)
            printf("%d\t", i);
        printf("\n");
    }
    return 0;
}

/* called when A's timer goes off */
int A_timerinterrupt()
{
    int i;
    if(DEBUG)printf("Timeout\n");
    starttimer(A, RTO_TIMEOUT);
    for(i = base; i < base + N && i < next_seqnum; i++){
        if(DEBUG)
            printf("Re-transmitting %d\n", i);
        if(FINAL)print_packet(send_pkt_a[ind(i)], T);
        tolayer3(A, send_pkt_a[ind(i)]);
    }
    return 0;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
int A_init()
{
    int i;
    base = 0;
    next_seqnum = 0;
    N = 8;
    for(i = 0; i < BUFFER_SIZE; i++) in_flight[i] = 0;
    return 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
int B_input(struct pkt packet)
{
    (void)packet;
    if(FINAL)print_packet(packet, R);
    if(!is_corrupt(packet) && has_expected_seqnum(packet)){
        if(DEBUG){
            print_packet(packet, R);
            printf("Expected packet found.\n");
            printf("Ack sent. Delivered\n");
        }
        struct msg message = extract_data(packet);
        tolayer5(B, message.data);

        send_pkt_b = make_packet(-1, expected_seqnum, NULL, 0);
        if(FINAL)print_packet(send_pkt_b, T);
        tolayer3(B, send_pkt_b);
        expected_seqnum++;
    }else{
        if(DEBUG){
            print_packet(packet, R);
            if(is_corrupt(packet))printf("Corrupted.\n");
            printf("Nack sent.\n");
        }
        if(FINAL)print_packet(send_pkt_b, T);
        tolayer3(B, send_pkt_b);
    }
    return 0;
}

/* called when B's timer goes off */
int B_timerinterrupt()
{
    return 0;
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
int B_init()
{
    expected_seqnum = 0;
    send_pkt_b = make_packet(-1, -1, NULL, 0);
    return 0;
}

int TRACE = 1;   /* for my debugging */
int nsim = 0;    /* number of messages from 5 to 4 so far */
int nsimmax = 0; /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;    /* probability that a packet is dropped  */
float corruptprob; /* probability that one bit is packet is flipped */
float lambda;      /* arrival rate of messages from layer 5 */
int ntolayer3;     /* number sent into layer 3 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/

int main()
{
    struct event * eventptr;
    struct msg msg2give;
    struct pkt pkt2give;

    int i, j;

    init();
    A_init();
    B_init();

    for (;; )
    {
        eventptr = evlist; /* get next event to simulate */
        if (NULL == eventptr)
        {
            goto terminate;
        }
        evlist = evlist->next; /* remove this event from event list */
        if (evlist != NULL)
        {
            evlist->prev = NULL;
        }
        if (TRACE >= 2)
        {
            printf("\nEVENT time: %f,", eventptr->evtime);
            printf("  type: %d", eventptr->evtype);
            if (eventptr->evtype == 0)
            {
                printf(", timerinterrupt  ");
            }
            else if (eventptr->evtype == 1)
            {
                printf(", fromlayer5 ");
            }
            else
            {
                printf(", fromlayer3 ");
            }
            printf(" entity: %d\n", eventptr->eventity);
        }
        time = eventptr->evtime; /* update time to next event time */
        if (nsim == nsimmax)
        {
            break; /* all done with simulation */
        }
        if (eventptr->evtype == FROM_LAYER5)
        {
            generate_next_arrival(); /* set up future arrival */
            /* fill in msg to give with string of same letter */
            j = nsim % 26;
            for (i = 0; i < 20; i++)
            {
                msg2give.data[i] = 97 + j;
            }
            if (TRACE > 2)
            {
                printf("          MAINLOOP: data given to student: ");
                for (i = 0; i < 20; i++)
                {
                    printf("%c", msg2give.data[i]);
                }
                printf("\n");
            }
            nsim++;
            if (eventptr->eventity == A)
            {
                A_output(msg2give);
            }
        }
        else if (eventptr->evtype == FROM_LAYER3)
        {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i = 0; i < 20; i++)
            {
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
            }
            if (eventptr->eventity == A)   /* deliver packet by calling */
            {
                A_input(pkt2give);           /* appropriate entity */
            }
            else
            {
                B_input(pkt2give);
            }
            free(eventptr->pktptr); /* free the memory for packet */
        }
        else if (eventptr->evtype == TIMER_INTERRUPT)
        {
            if (eventptr->eventity == A)
            {
                A_timerinterrupt();
            }
            else
            {
                B_timerinterrupt();
            }
        }
        else
        {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }
    return 0;

terminate:
    printf(
        " Simulator terminated at time %f\n after sending %d msgs from layer5\n",
        time, nsim);
    return 0;
}

void init() /* initialize the simulator */
{
    int i;
    float sum, avg;

    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
    printf("Enter the number of messages to simulate: ");
    scanf("%d", &nsimmax);
    printf("Enter  packet loss probability [enter 0.0 for no loss]:");
    scanf("%f", &lossprob);
    printf("Enter packet corruption probability [0.0 for no corruption]:");
    scanf("%f", &corruptprob);
    printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
    scanf("%f", &lambda);
    printf("Enter TRACE:");
    scanf("%d", &TRACE);

    srand(rand_seed); /* init random number generator */
    sum = 0.0;   /* test random number generator for students */
    for (i = 0; i < 1000; i++)
    {
        sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
    }
    avg = sum / 1000.0;
    if (avg < 0.25 || avg > 0.75)
    {
        printf("It is likely that random number generation on your machine\n");
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(0);
    }

    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;

    time = 0.0;              /* initialize time to 0.0 */
    generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
    double mmm = INT_MAX;         /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
    float x;                      /* individual students may need to change mmm */
    x = rand_r(&rand_seed) / mmm; /* x should be uniform in [0,1] */
    return x;
}

/************ EVENT HANDLINE ROUTINES ****************/
/*  The next set of routines handle the event list   */
/*****************************************************/
void generate_next_arrival()
{
    double x;
    struct event * evptr;

    if (TRACE > 2)
    {
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
    }

    x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
    /* having mean of lambda        */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + x;
    evptr->evtype = FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand() > 0.5))
    {
        evptr->eventity = B;
    }
    else
    {
        evptr->eventity = A;
    }
    insertevent(evptr);
}

void insertevent(struct event * p)
{
    struct event * q, * qold;

    if (TRACE > 2)
    {
        printf("            INSERTEVENT: time is %lf\n", time);
        printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
    }
    q = evlist;      /* q points to header of list in which p struct inserted */
    if (NULL == q)   /* list is empty */
    {
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    }
    else
    {
        for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
        {
            qold = q;
        }
        if (NULL == q)   /* end of list */
        {
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        }
        else if (q == evlist)     /* front of list */
        {
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        }
        else     /* middle of list */
        {
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void printevlist()
{
    struct event * q;
    printf("--------------\nEvent List Follows:\n");
    for (q = evlist; q != NULL; q = q->next)
    {
        printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
               q->eventity);
    }
    printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
{
    struct event * q;

    if (TRACE > 2)
    {
        printf("          STOP TIMER: stopping timer at %f\n", time);
    }

    for (q = evlist; q != NULL; q = q->next)
    {
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            /* remove this event */
            if (NULL == q->next && NULL == q->prev)
            {
                evlist = NULL;              /* remove first and only event on list */
            }
            else if (NULL == q->next)     /* end of list - there is one in front */
            {
                q->prev->next = NULL;
            }
            else if (q == evlist)     /* front of list - there must be event after */
            {
                q->next->prev = NULL;
                evlist = q->next;
            }
            else     /* middle of list */
            {
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB, float increment)
{
    struct event * q;
    struct event * evptr;

    if (TRACE > 2)
    {
        printf("          START TIMER: starting timer at %f\n", time);
    }

    /* be nice: check to see if timer is already started, if so, then  warn */
    for (q = evlist; q != NULL; q = q->next)
    {
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
        {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }
    }

    /* create future event for when timer goes off */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtime = time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}

/************************** TOLAYER3 ***************/
void tolayer3(int AorB, struct pkt packet)
{
    struct pkt * mypktptr;
    struct event * evptr, * q;
    float lastime, x;
    int i;

    ntolayer3++;

    /* simulate losses: */
    if (jimsrand() < lossprob)
    {
        nlost++;
        if (TRACE > 0)
        {
            printf("          TOLAYER3: packet being lost\n");
        }
        return;
    }

    /*
     * make a copy of the packet student just gave me since he/she may decide
     * to do something with the packet after we return back to him/her
     */

    mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i = 0; i < 20; ++i)
    {
        mypktptr->payload[i] = packet.payload[i];
    }
    if (TRACE > 2)
    {
        printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
               mypktptr->acknum, mypktptr->checksum);
        for (i = 0; i < 20; ++i)
        {
            printf("%c", mypktptr->payload[i]);
        }
        printf("\n");
    }

    /* create future event for arrival of packet at the other side */
    evptr = (struct event *)malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
    evptr->eventity = (AorB + 1) & 1; /* event occurs at other entity */
    evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */

    /*
     * finally, compute the arrival time of packet at the other end.
     * medium can not reorder, so make sure packet arrives between 1 and 10
     * time units after the latest arrival time of packets
     * currently in the medium on their way to the destination
     */

    lastime = time;
    for (q = evlist; q != NULL; q = q->next)
    {
        if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
        {
            lastime = q->evtime;
        }
    }
    evptr->evtime = lastime + 1 + 9 * jimsrand();

    /* simulate corruption: */
    if (jimsrand() < corruptprob)
    {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
        {
            mypktptr->payload[0] = 'Z'; /* corrupt payload */
        }
        else if (x < .875)
        {
            mypktptr->seqnum = 999999;
        }
        else
        {
            mypktptr->acknum = 999999;
        }
        if (TRACE > 0)
        {
            printf("          TOLAYER3: packet being corrupted\n");
        }
    }

    if (TRACE > 2)
    {
        printf("          TOLAYER3: scheduling arrival on other side\n");
    }
    insertevent(evptr);
}

void tolayer5(int AorB, const char * datasent)
{
    (void)AorB;
    int i;
    if (TRACE > 2)
    {
        printf("          TOLAYER5: data received: ");
        for (i = 0; i < 20; i++)
        {
            printf("%c", datasent[i]);
        }
        printf("\n");
    }
}
