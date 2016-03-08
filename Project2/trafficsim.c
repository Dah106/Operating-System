/*
	Author: Danchen Huang
	Date Created: 10/11/2015
	Date last modified: 10/18/2015
	This file is dedicated to simulate a traffic system using semaphore
	Project 2 of CS1550, Fall 2015
	
	##################################################################################################
	To add a new syscall to the Linux kernel, there are three main files that need to be modified:

	linux-2.6.23.1/kernel/sys.c
	
	This file contains the actual implementation of the system calls.

	linux-2.6.23.1/arch/i386/kernel/syscall_table.S
	
	This file declares the number that corresponds to the syscalls

	linux-2.6.23.1/include/asm/unistd.h
	##################################################################################################

	Building kernel:

	make ARCH=i386 bzImage
	
	Copying the Files to QEMU:
	
	scp dah106@thoth.cs.pitt.edu:/u/OSLab/dah106/linux-2.6.23.1/arch/i386/boot/bzImage .
	scp dah106@thoth.cs.pitt.edu:/u/OSLab/dah106/linux-2.6.23.1/System.map .

	Installing the Rebuilt Kernel in QEMU:
	
	cp bzImage /boot/bzImage-devel
	cp System.map /boot/System.map-devel


	To compile the program, run following command:
	gcc -m32 -o trafficsim -I /u/OSLab/dah106/linux-2.6.23.1/include/ trafficsim.c
	
	##################################################################################################
	Three semaphores are used. 
	The binary semaphore 'mutex_north' controls access to the northbound car queue 
	The binary semaphore 'mutex_south' controls access to the southbound car queue
	The counting semaphore 'full_both' keeps track the number of full slots of either queue
	
	The buffer resembles the car queue
	Buffer N will be the northbound car queue
	Buffer S will be the southbound car queue

	Initially
	mutex_north = 1
	mutex_south = 1
	empty_north = 1
	empty_south = 1
	full_both = 0

	There are two createProducerSouth which simulate two directions of car flow
	Producer N will be the car flow to north
	Producer S will be the car flow to south

	There is one consumer which simulates the flag person in the construction area
*/

#include <linux/unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>

#define NUM_OF_PRODUCER 2
#define NUM_OF_CONSUMER 1
#define BUFFER_SIZE 10

struct Node
{
	struct task_struct *task;
	struct Node *next;
};

struct cs1550_sem {
	int value;
	struct Node *front;
	struct Node *back;
};

void down(struct cs1550_sem *sem, int identifier)
{
	syscall(__NR_cs1550_down, sem, identifier);
}

void up(struct cs1550_sem *sem)
{
	syscall(__NR_cs1550_up, sem);
}

bool isNextCar();
void createProducerNorth();
void createProducerSouth();
void createConsumer();
void createChild1();
void createChild2();
void createChild3();

struct cs1550_sem *mutex_north;
struct cs1550_sem *mutex_south;
struct cs1550_sem *empty_north;
struct cs1550_sem *empty_south;
struct cs1550_sem *full_both;

int *in_north;
int *in_south;
int *out_north;
int *out_south;
int *car_num_hist_north;
int *car_num_hist_south;
int *car_num_current_north;
int *car_num_current_south;
int *buffer_north;
int *buffer_south;


int main()
{	

	// Allocate memory for the car buffer and semaphore
	void *sharedMemPtr = mmap(NULL, sizeof(int)*(2*BUFFER_SIZE+8), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);	
	void *semaphorePtr = mmap(NULL, sizeof(struct cs1550_sem)*5, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);	

	//Initiate value for shared variables
	in_north = (int*)sharedMemPtr;
	in_south = (int*)sharedMemPtr + 1;
	out_north= (int*)sharedMemPtr + 2;
	out_south = (int*)sharedMemPtr + 3;
	car_num_current_north = (int*)sharedMemPtr + 4;
	car_num_current_south = (int*)sharedMemPtr + 5;
	car_num_hist_north = (int*)sharedMemPtr + 6;
	car_num_hist_south = (int*)sharedMemPtr + 7;
	buffer_north = (int*)sharedMemPtr + 8;
	buffer_south = (int*)sharedMemPtr + 8 + BUFFER_SIZE;

	mutex_north = (struct cs1550_sem*)semaphorePtr;
	mutex_south = (struct cs1550_sem*)semaphorePtr+1;
	empty_north = (struct cs1550_sem*)semaphorePtr+2;
	empty_south = (struct cs1550_sem*)semaphorePtr+3;
	full_both = (struct cs1550_sem*)semaphorePtr+4;

	*in_north = 0;
	*in_south = 0;
	*out_north = 0;
	*out_south = 0;
	*car_num_current_north = 0;
	*car_num_current_south = 0;
	*car_num_hist_north = 0;
	*car_num_hist_south = 0;

	mutex_north->value = 1;	
	mutex_north->front = NULL;	
	mutex_north->back = NULL;	

	mutex_south->value = 1;
	mutex_south->front = NULL;	
	mutex_south->back = NULL;

	empty_north->value = BUFFER_SIZE;
	empty_north->front = NULL;
	empty_north->back = NULL;

	empty_south->value = BUFFER_SIZE;
	empty_south->front = NULL;
	empty_south->back = NULL;

	full_both->value = 0;
	full_both->front = NULL;	
	full_both->back = NULL;
	

	int totalSize = NUM_OF_PRODUCER + NUM_OF_CONSUMER;
	printf("\n This is a traffic simulation program for CS1550 project 2 \n");
	printf("\n ###############################################################################################\n");
	printf("\n Please read below section before running the program");
	printf("\n A history car count is used to identify each car. E.G The 10th car in northbound\n");
	printf("\n A current car count is used to identify current car count in the buffer of each direction");
	printf("\n Producer will produce a car with the unique car id which is based on the history count of each direction \n");
	printf("\n Flagger will consume a car with the unique car id which is based on the history count of each direction \n");
	printf("\n ###############################################################################################\n");
	printf("\n Press enter to continue...\n");	

	getchar();
	printf("\n Total num of child processes: %d\n System starts in 3 seconds...\n", totalSize);
	sleep(3);

	//Create three child processes
	createChild1();
	createChild2();
	createChild3();

	return 0;
}

bool isNextCar()
{
	int result = rand() % 10;
	if(result < 8)
	{
		return true;
	}
	return false;
}

void createProducerNorth()
{		
	while (1)
	{
		down(empty_north, 1);		
		down(mutex_north, 1);
		if (isNextCar()) 
		{		

			*car_num_current_north += 1;
			*car_num_hist_north += 1;	
			buffer_north[*in_north] = *car_num_hist_north;
			*in_north = (*in_north + 1) % BUFFER_SIZE;					
			
			printf("\n Northbound produced car N-%d\n", *car_num_current_north);					

			up(mutex_north);
			up(full_both);
		}
		else
		{
			printf("\n NO CAR! Northbound coming in 20 seconds\n");
			up(mutex_north);
			up(full_both);			
			sleep(20);
		}
	}
}

void createProducerSouth()
{
	while (1)
	{
		down(empty_south, 2);
		down(mutex_south, 2);
		if (isNextCar())
		{									
			*car_num_current_south += 1;
			*car_num_hist_south += 1;
			buffer_south[*in_south] = *car_num_hist_south;
			*in_south = (*in_south + 1) % BUFFER_SIZE;		
	
			printf("\n Southbound produced car S-%d\n", *car_num_current_south);			

			up(mutex_south);
			up(full_both);									
		}
		else
		{
			up(mutex_south);
			up(full_both);

			printf("\n NO CAR! Southbound coming in 20 seconds\n");
			sleep(20);
		} 
	}
}

void createConsumer()
{
	int citem;
	while (1)
	{	
		down(full_both, 3);	
		down(mutex_north, 1);
		down(mutex_south, 2);	

		if (*car_num_current_north > 0)
		{
			if (*car_num_current_south < 10)
			{
				up(mutex_south);

				citem = buffer_north[*out_north]; 
				*car_num_current_north -= 1;	
				*out_north = (*out_north + 1) % BUFFER_SIZE;	

				printf("\n Flagger is consuming the car N-%d, northbound current car count: %d\n", citem, *car_num_current_north);				
				
				up(mutex_north);				
				up(empty_north);
				sleep(2);
			}
			else
			{
				up(mutex_north);

				citem = buffer_south[*out_south]; 
				*car_num_current_south -= 1;
				*out_south = (*out_south + 1) % BUFFER_SIZE;
				
				printf("\n Flagger is consuming the car S-%d, southbound current car count: %d\n", citem, *car_num_current_south);				
				
				up(mutex_south);
				up(empty_south);
				sleep(2);
			}			
		}
		else if (*car_num_current_south > 0)
		{
			if (*car_num_current_north < 10)
			{
				up(mutex_north);
				
				citem = buffer_south[*out_south]; 
				*car_num_current_south -= 1;
				*out_south = (*out_south + 1) % BUFFER_SIZE;
				
				printf("\n Flagger is consuming the car S-%d, southbound current car count: %d\n", citem, *car_num_current_south);
				
				up(mutex_south);
				up(empty_south);
				sleep(2);
			}
			else
			{
				up(mutex_south);
				
				citem = buffer_north[*out_north]; 
				*car_num_current_north -= 1;	
				*out_north = (*out_north + 1) % BUFFER_SIZE;				
				
				printf("\n Flagger is consuming the car N-%d, northbound current car count:\n", citem, *car_num_current_north);				
				
				up(mutex_north);
				up(empty_north);
				sleep(2);
			}
		}	
		else
		{	
			// No cars in either directions
			// Unlock both directions
			up(mutex_north);
			up(mutex_south);			
		}			
	}
}

void createChild1()
{
	int pid = fork();
	if(pid == 0)
	{
		createProducerNorth();
	}
}

void createChild2()
{
	int pid = fork();
	if(pid == 0)
	{
		createProducerSouth();
	}
}

void createChild3()
{
	int pid = fork();
	if(pid == 0)
	{
		createConsumer();
	}
}
