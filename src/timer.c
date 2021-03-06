#include <system.h>
#include <vga.h>
#include <kernel_assert.h>
#include <zz.h>

static void runproc(); // not exported
void(*proc)(int);

#define DEFAULT_PRIORITY 1
#define JIFFY 18.222 //@ We might change the timer frequency later

int atomicity; 	    //@ multithreading disabled?
int maxpid;	    //@ number of threads
int cpid;	    //@ currently running thread
int threadticks;    //@ ticks on current thread

void *processes[MAX_PROCESSES]; //@ array of pointers to static functions
int priority[MAX_PROCESSES];	//@ priority in ticks

int request_atomicity(int req) { //@ turn on or off multithreading
	atomicity=req; // later we might do more checking
	return atomicity;
}

void yield() {      //@ yields timeslice. Also called by process switch
	if(!atomicity) {
		cpid++; // round robin is good enough for now
		if(cpid>maxpid)
			cpid=0;
		threadticks=0;
		runproc();
	}
}

static void runproc() { //@
	if(!atomicity && cpid>=0 && cpid<=maxpid) {
		proc=processes[cpid];
		outportb(0x20, 0x20);
		proc(cpid);
	}
}

void kill(int pid) { //@
	int i;
	for(i=pid+1; i<MAX_PROCESSES; i++) {
		processes[i-1]=processes[i];
	}
	maxpid--;
}

int spawn(void(*fn)(int)) { //@
	maxpid++;
	kernel_assert(maxpid<MAX_PROCESSES, "PANIC: too many processes", "-1");
	processes[maxpid]=fn;
	priority[maxpid]=DEFAULT_PRIORITY;
	return maxpid;
}

int renice(int pid, int nice) { //@ what it says on the tin. Higher priorities aren't as nice
	if(nice>=JIFFY/2) { // sanity check: if priority is more than half a second this is an exploit
		return priority[pid];
	}
	priority[pid]=nice;
	return nice;
}

int fork() { //@
	return spawn(processes[cpid]);
}


int timedirty; //@ Time saver for the clock
int timer_ticks = 0;
int uptime_secs;
char timeset[5];


void timer_handler(struct regs *r) { //@ 
	// Increment our 'tick count' 
	timer_ticks++;
	threadticks++;
	if(!(timer_ticks%(int)JIFFY)) {
		uptime_secs++;
		displaytime(); 
	}
	
	if(threadticks>priority[cpid]) {
		yield();
	}
}

void displaytime() { //@ time/status bar
	int i;
    	int attr=color;
	int x=cursorx;
	int y=cursory;
	int tempset=uptime_secs;
	
	// clear time-status bar
	locate(0, VGAY-1);
	setattr(0x00);
	for(i=0; i<VGAX; i++) {
		write(' ');
	}
	
	// display time-status bar
	locate(0, VGAY-1);
	setattr(0x30);
	puts(padl(2, itoa(tempset/(60*60), timeset), '0'));
	tempset%=(60*60);
	puts(":");
	puts(padl(2, itoa(tempset/60, timeset), '0'));
	tempset%=60;
	puts(":");
	puts(padl(2, itoa(tempset, timeset), '0'));
	setattr(0x20);
	puts(" Threads:");
	puts(padl(3, itoa(maxpid+1, timeset), ' '));
	puts(" PID:");
	puts(padl(3, itoa(cpid, timeset), ' '));
	puts(" Ticks:");
	puts(padl(3, itoa(threadticks, timeset), ' '));
	puts(" Nice:");
	puts(itoa(priority[cpid], timeset));
	puts(" CID:");
	puts(padl(3, itoa(currcell, timeset), ' '));
	setattr(0x50);
	puts("Press h for help");
	locate(VGAX-8, VGAY-1);
	setattr(0x01);
	puts("IX v0.1");
	setattr(0x04);
	locate(VGAX-8, VGAY-2);
	puts("\x0f");
	
	// corner dim display
	setattr(0x01);
	locate(VGAX-4, 2);
	puts("    "); // clear out old dimy
	locate(VGAX-6, 0);
	puts("    "); // clear out old dimx
	locate(VGAX-3, 0);
	puts("--+");
	locate(VGAX-1, 1);
	puts("|");
	setattr(0x05);
	locate(VGAX-(strlen(itoa(dimx, timeset))+3), 0);
	puts(itoa(dimx, timeset));
	locate(VGAX-strlen(itoa(dimy, timeset)), 2);
	puts(itoa(dimy, timeset));
	
	// go back to normal
	setattr(attr);
	locate(x, y);
}


void timer_wait(int ticks) { //@ Loop for ticks jiffies
	unsigned long eticks;
	
	eticks = timer_ticks + ticks;
	while(timer_ticks < eticks);
}


void timer_install() { //@ Set up the timer handler and multithreading
	// setup multithreading
	atomicity=1;
	maxpid=-1;
	cpid=0;
	
	// Installs 'timer_handler' to IRQ0 
	irq_install_handler(0, timer_handler);
}

