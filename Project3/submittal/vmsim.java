/*
	This file is created at 8:52 PM, 11/02/2015, last modified at 10:52 PM, 11/17/2015
	The author who writes this file is Danchen Huang, a great man of all time
	The purpose of this file is to implement a virtual memory simulator that emulates four main page replace algorithms
	The source code is dedicated for project 3 of cs1550
	Compilation: javac vmsim.java
	Execution (General): java vmsim -n <numframes> -a <opt|clock|nru|work> -r <refresh> -t <tau> <tracefile>
		for Opt: java vmsim	-n <numframes> -a opt <tracefile>
		for Clock: java vmsim -n <numframes> -a clock <tracefile>
		for Nru: java vmsim -n <numframes> -a nru -r <refresh_time> <tracefile>
		for Work: java vmsim -n <numframes> -a work -r <refresh_time> -t <tau> <tracefile>
		
*/


import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Collections;

public class vmsim 
{	
	
	private static final String OPERATION_READ = "R";
	private static final String OPERATION_WRITE = "W";

	//32-bit address, page size 4KB = 2^12, page table size 2^20
	private static final int PAGE_SIZE = 4096;
	private static final int PAGE_TABLE_SIZE = 1048576;

	//Detailed system output mode
	private static final boolean DETAIL_INFO = false;

	private int[] FRAME_TABLE;
	private int FULL_FRAME_COUNT;

	//Data structure for "opt"
	private HashMap<Integer, PageTableEntry>PAGE_TABLE;
	private HashMap<Integer, LinkedList<Integer>>DISTANCE_MAP;

	//Data Structure for "work"
	//private PageTableEntry[] PAGE_TABLE_WORK;

	//Command line arguments
	private int NUM_OF_FRAMES;
	private String TYPE_OF_ALGORITHM;
	private int REFRESH_TIME;
	private int TAU;
	private String FILE_NAME;

	//System output parameters
	private int TOTAL_NUM_MEMORY_ACCESS;
	private int TOTAL_NUM_PAGE_FAULT;
	private int TOTAL_NUM_DISK_WRITE;
	private long TOTAL_RUN_TIME;

	//For "nru" and "work"
	private int REFRESH_COUNTER;
	private int TOTAL_NUM_OF_REFRESHES;

	//For "Clock (Second Chance)"
	private int CLOCK_POINTER_POSITION;

	//For "Working set" mock counter for timestamp
	private int CURRENT_VIRTUAL_TIMESTAMP;
	private int LAST_POSITION;

	public static void main(String[] args) 
	{	
		vmsim pageAlgo = new vmsim();
		
		long start = System.currentTimeMillis();
		
		pageAlgo.initSystem(args);
		
		pageAlgo.TOTAL_RUN_TIME = System.currentTimeMillis() - start;
		
		pageAlgo.printFinalStats();
	}


	private void initSystem(String[] args)
	{	
		TOTAL_NUM_MEMORY_ACCESS = 0;
		TOTAL_NUM_PAGE_FAULT = 0;
		TOTAL_NUM_DISK_WRITE = 0;

		TOTAL_NUM_OF_REFRESHES = 0;

		//System.out.println("The args length is: " + args.length);
		if(args.length == 5 || args.length == 7 || args.length == 9)
		{	
			NUM_OF_FRAMES = Integer.parseInt(args[1]);
			TYPE_OF_ALGORITHM = args[3];
			
			FILE_NAME = args[args.length - 1];


			if(TYPE_OF_ALGORITHM.equalsIgnoreCase("opt"))
			{	
				if(NUM_OF_FRAMES == 8 || NUM_OF_FRAMES == 16 || NUM_OF_FRAMES == 32 || NUM_OF_FRAMES == 64)
				{	
					if(args.length == 5)
					{	
						scanFileOpt();
						opt();
					}
					else
					{
						System.out.println("Improper number of arguments!");
						error();
					}
				}
				else
				{
					System.out.println("Number of frames error!");
					System.out.println("Make sure to use numbers that are 8, 16, 32 or 64");
				}
			}
			else if(TYPE_OF_ALGORITHM.equalsIgnoreCase("clock"))
			{
				if(NUM_OF_FRAMES == 8 || NUM_OF_FRAMES == 16 || NUM_OF_FRAMES == 32 || NUM_OF_FRAMES == 64)
				{
					if(args.length == 5)
					{
						secondChanceAlgorithm();
					}
					else
					{	
						System.out.println("Improper number of arguments!");
						error();
					}
				}
				else
				{
					System.out.println("Number of frames error!");
					System.out.println("Make sure to use numbers that are 8, 16, 32 or 64");
				}
			}
			else if(TYPE_OF_ALGORITHM.equalsIgnoreCase("nru"))
			{	
				if(NUM_OF_FRAMES == 8 || NUM_OF_FRAMES == 16 || NUM_OF_FRAMES == 32 || NUM_OF_FRAMES == 64)
				{	
					if(args.length == 7)
					{	
						REFRESH_TIME = Integer.parseInt(args[5]);
						if(REFRESH_TIME <= 0)
						{
							System.out.println("Refresh time has to be a non-zero positive number");
						}
						else
						{	
							System.out.println("Refresh time is: " + REFRESH_TIME);
							nru();
							
						}
					}
					else
					{
						System.out.println("NRU: Improper number of arguments!");
						error();
					}
				}
				else
				{	
					System.out.println("Number of frames error!");
					System.out.println("Make sure to use numbers that are 8, 16, 32 or 64");
				}
			}
			else if(TYPE_OF_ALGORITHM.equalsIgnoreCase("work"))
			{
				if(NUM_OF_FRAMES == 8 || NUM_OF_FRAMES == 16 || NUM_OF_FRAMES == 32 || NUM_OF_FRAMES == 64)
				{
					if(args.length == 9)
					{	
						REFRESH_TIME = Integer.parseInt(args[5]);
						TAU = Integer.parseInt(args[7]);

						if(REFRESH_TIME <= 0 || TAU <= 0)
						{
							System.out.println("Refresh time or Tau has to be a non-zero positive number");
						}
						else
						{
							System.out.println("Refresh time is: " + REFRESH_TIME);
							System.out.println("TAU is: " + TAU);
							work();
						}
					}
					else
					{
						System.out.println("WORK: Improper number of arguments!");
						error();
					}
				}
				else
				{
					System.out.println("Number of frames error!");
					System.out.println("Make sure to use numbers that are 8, 16, 32 or 64");
				}
			}
			else
			{
				error();
			}
			
		}
		else
		{
			error();
		}
	}

	private void scanFileOpt()
	{
		FRAME_TABLE = new int[NUM_OF_FRAMES];
		FULL_FRAME_COUNT = 0;

		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			FRAME_TABLE[i] = -1;
		}

		PAGE_TABLE = new HashMap<Integer, PageTableEntry>();
		DISTANCE_MAP = new HashMap<Integer, LinkedList<Integer>>();
		for(int i = 0;i < PAGE_TABLE_SIZE;i++)
		{
			PageTableEntry entry = new PageTableEntry();
			PAGE_TABLE.put(i, entry);
			DISTANCE_MAP.put(i, new LinkedList<Integer>());
		}

		try {
			File file = new File(FILE_NAME);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			
			StringBuffer stringBuffer = new StringBuffer();
			String line;
			String memAddress;
			String operation;

			int currentPosition = 0;
			while ((line = bufferedReader.readLine()) != null) {
				String [] splited = line.split("\\s+");
				memAddress = splited[0];
				operation = splited[1];

				//Take upper 5 hex bits for the page number from the address
				//The lower 3 is for the offset
				int pn = hexToDecimal(memAddress.substring(0,5));
				//System.out.println("page number in decimal: " + pn);
				
				DISTANCE_MAP.get(pn).add(currentPosition);
				currentPosition++;
			}

			fileReader.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private void opt()
	{
		try {
			File file = new File(FILE_NAME);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			
			StringBuffer stringBuffer = new StringBuffer();
			String line;
			String memAddress;
			String operation;

			int currentPosition = 0;
			while ((line = bufferedReader.readLine()) != null) {
				String [] splited = line.split("\\s+");
				memAddress = splited[0];
				operation = splited[1];

				TOTAL_NUM_MEMORY_ACCESS++;
				//Take upper 5 hex bits for the page number from the address
				//The lower 3 is for the offset
				int pn = hexToDecimal(memAddress.substring(0,5));

				//If the current page does occur more than once in the distance map
				//Remove current position
				//Now that the head of the linkedlist represents the distance until next occurance
				if(!DISTANCE_MAP.get(pn).isEmpty())
				{
					DISTANCE_MAP.get(pn).removeFirst();
				}
				
				PageTableEntry entry = PAGE_TABLE.get(pn);
				
				//reference bit is initialized as 1
				entry.rBit = 1;

				//Only set the dirty bit to 1 when encounters a one
				if(operation.equalsIgnoreCase(OPERATION_WRITE))
				{
					entry.dBit = 1;
				}
				if(entry.vBit == 0)
				{	
					//Valid bit off, page fault occurs
					TOTAL_NUM_PAGE_FAULT++;

					//If frame table is full
					if(FULL_FRAME_COUNT >= NUM_OF_FRAMES)
					{
						int victimPageNumber = searchFartherstUsedVictim();
						PageTableEntry victim = PAGE_TABLE.get(victimPageNumber);
							
						//System.out.println("Victim's frame number: " + victim.frameNumber + "Victim's page number: " + victimPageNumber);
						
						//Victim's page is clean
						//Evict clean
						//Otherwise write to disk since encounters a dirty page
						if(victim.dBit == 0)
						{
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict clean!");
						}
						else
						{	
							TOTAL_NUM_DISK_WRITE++;
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict DIRTY!");
						}

						FRAME_TABLE[victim.frameNumber] = pn;
						entry.frameNumber = victim.frameNumber;
						entry.vBit = 1;

						//Update victim page to page table
						victim.rBit = 0;
						victim.dBit = 0;
						victim.vBit = 0;
						victim.frameNumber = -1;
						PAGE_TABLE.put(victimPageNumber, victim);
					}
					else
					{
						if(DETAIL_INFO) System.out.println(pn + "\t"  + memAddress + "\t" + operation + "\tPage fault – no eviction!");
						FRAME_TABLE[FULL_FRAME_COUNT] = pn;
						entry.frameNumber = FULL_FRAME_COUNT;
						entry.vBit = 1;
						FULL_FRAME_COUNT++;
					}
				}
				else
				{
					if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tHit!");
				}
				
				//System.out.println("Frame number in entry: " + entry.frameNumber);
				PAGE_TABLE.put(pn, entry);
			}

			fileReader.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private int searchFartherstUsedVictim()
	{
		int maxDistance = 0;
		int victimPageNumber = 0;
		int tempPageNumber = 0;
		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			tempPageNumber = FRAME_TABLE[i];
			//If the current page number has not been used twice in the future
			//Simply return that index without further comparison
			if(DISTANCE_MAP.get(tempPageNumber).isEmpty())
			{
				victimPageNumber = tempPageNumber;
				break;
			}
			else
			{
				if(DISTANCE_MAP.get(tempPageNumber).peekFirst() > maxDistance)
				{
					maxDistance = DISTANCE_MAP.get(tempPageNumber).peekFirst();
					victimPageNumber = tempPageNumber;
				}
			}
		}
		//System.out.println("Candidate Index in the frame table is: " + victimPageNumber);
		return victimPageNumber;
	}

	//Using second chance algorithm
	private void secondChanceAlgorithm()
	{	
		FRAME_TABLE = new int[NUM_OF_FRAMES];
		FULL_FRAME_COUNT = 0;
		
		//Initialize the clock pointer
		CLOCK_POINTER_POSITION = 0;
		
		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			FRAME_TABLE[i] = -1;
		}

		PAGE_TABLE = new HashMap<Integer, PageTableEntry>();
		for(int i = 0;i < PAGE_TABLE_SIZE;i++)
		{
			PageTableEntry entry = new PageTableEntry();
		    PAGE_TABLE.put(i, entry);
		}

		try {
			File file = new File(FILE_NAME);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			
			String line;
			String memAddress;
			String operation;

			while ((line = bufferedReader.readLine()) != null) {
				String [] splited = line.split("\\s+");
				memAddress = splited[0];
				operation = splited[1];

			
				TOTAL_NUM_MEMORY_ACCESS++;
				//Take upper 5 hex bits for the page number from the address
				//The lower 3 is for the offset
				int pn = hexToDecimal(memAddress.substring(0,5));

				PageTableEntry entry = PAGE_TABLE.get(pn);
				entry.rBit = 1;

				//Only set the dirty bit to 1 when encounters a one
				if(operation.equalsIgnoreCase(OPERATION_WRITE))
				{
					entry.dBit = 1;
				}
				if(entry.vBit == 0)
				{
					//Valid bit off, page fault occurs
					TOTAL_NUM_PAGE_FAULT++;
					
					//If frame table is full
					if(FULL_FRAME_COUNT >= NUM_OF_FRAMES)
					{	
						boolean isVictimFound = false;
						while(!isVictimFound)
						{
							//Simulate circular pointer
							if(CLOCK_POINTER_POSITION >= NUM_OF_FRAMES) CLOCK_POINTER_POSITION = 0;

							//if(DETAIL_INFO) System.out.println("Pointer position: " + CLOCK_POINTER_POSITION);

							int victimCandidatePageNumber = FRAME_TABLE[CLOCK_POINTER_POSITION];
							PageTableEntry victimCandidate = PAGE_TABLE.get(victimCandidatePageNumber);

							//Check candidate's reference bit
							//If it is 1, give it a second chance, set to 0 , put back to page table and go to next candidate in the queue
							//else evict the candidate and check its dirty bit to determine if the page has been modified
							if(victimCandidate.rBit == 1)
							{
								victimCandidate.rBit = 0;
							}
							else
							{
								if(victimCandidate.dBit == 0)
								{
									if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict clean!");
								}
								else
								{
									TOTAL_NUM_DISK_WRITE++;
									if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict DIRTY!");
								}

								FRAME_TABLE[victimCandidate.frameNumber] = pn;
								entry.frameNumber = victimCandidate.frameNumber;
								entry.vBit = 1;

								//update victim page to page table
								victimCandidate.rBit = 0;
								victimCandidate.dBit = 0;
								victimCandidate.vBit = 0;
								victimCandidate.frameNumber = -1;
								
								//Victim is found
								isVictimFound = true;
							}

							PAGE_TABLE.put(victimCandidatePageNumber, victimCandidate);

							CLOCK_POINTER_POSITION++;
						}
					}
					else
					{
						if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault – no eviction!");
						FRAME_TABLE[FULL_FRAME_COUNT] = pn;
						entry.frameNumber = FULL_FRAME_COUNT;
						entry.vBit = 1;
						FULL_FRAME_COUNT++;
					}

				}
				else
				{
					if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tHit!");
				}

				PAGE_TABLE.put(pn, entry);
			}

			fileReader.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private void nru()
	{	
		FRAME_TABLE = new int[NUM_OF_FRAMES];
		FULL_FRAME_COUNT = 0;
		REFRESH_COUNTER = 0;

		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			FRAME_TABLE[i] = -1;
		}

		PAGE_TABLE = new HashMap<Integer, PageTableEntry>();
		for(int i = 0;i < PAGE_TABLE_SIZE;i++)
		{
			PageTableEntry entry = new PageTableEntry();
			PAGE_TABLE.put(i, entry);
		}

		try {
			File file = new File(FILE_NAME);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			
			String line;
			String memAddress;
			String operation;

			while ((line = bufferedReader.readLine()) != null) {
				String [] splited = line.split("\\s+");
				memAddress = splited[0];
				operation = splited[1];

				//Take upper 5 hex bits for the page number from the address
				//The lower 3 is for the offset
				int pn = hexToDecimal(memAddress.substring(0,5));
				PageTableEntry entry = PAGE_TABLE.get(pn);

				//Reference bit is initialized to 1
				entry.rBit = 1;

				//Only set the dirty bit to 1 when encounters a one
				if(operation.equalsIgnoreCase(OPERATION_WRITE))
				{
					entry.dBit = 1;
				}
				if(entry.vBit == 0)
				{
					//Valid bit off, page fault occurs
					TOTAL_NUM_PAGE_FAULT++;
					if(FULL_FRAME_COUNT >= NUM_OF_FRAMES)
					{
						int victimPageNumber = findLowestClass();
						PageTableEntry victim = PAGE_TABLE.get(victimPageNumber);

						//Victim's page is clean
						//Evict clean
						//Otherwise write to disk since encounters a dirty page
						if(victim.dBit == 0)
						{	
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict clean!");
						}
						else
						{	
							TOTAL_NUM_DISK_WRITE++;
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict DIRTY!");
						}

						FRAME_TABLE[victim.frameNumber] = pn;
						entry.frameNumber = victim.frameNumber;
						entry.vBit = 1;

						//Update victim page to page table
						victim.rBit = 0;
						victim.dBit = 0;
						victim.vBit = 0;
						victim.frameNumber = -1;
						PAGE_TABLE.put(victimPageNumber, victim);

					}
					else
					{
						if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault – no eviction!");
						FRAME_TABLE[FULL_FRAME_COUNT] = pn;
						entry.frameNumber = FULL_FRAME_COUNT;
						entry.vBit = 1;
						FULL_FRAME_COUNT++;
					}
				}
				else
				{
					if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tHit!");
				}

				TOTAL_NUM_MEMORY_ACCESS++;
				REFRESH_COUNTER++;

				//Periodic refresh
				refreshReferenceBit();

				PAGE_TABLE.put(pn, entry);
				//System.out.println(pn + " " + Arrays.toString(FRAME_TABLE));
			}

			fileReader.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private void refreshReferenceBit()
	{	
		//refresh -> set ref bit to 0 for all pages in the memory
		if(REFRESH_COUNTER == REFRESH_TIME)
		{	
			int tempPageNumber = 0;
			for(int i = 0; i < FULL_FRAME_COUNT;i++)
			{
				tempPageNumber = FRAME_TABLE[i];
				if(tempPageNumber != -1)
				{
					PageTableEntry refreshEntry = PAGE_TABLE.get(tempPageNumber);
					refreshEntry.rBit = 0;
					PAGE_TABLE.put(tempPageNumber, refreshEntry);
				}
			}

			REFRESH_COUNTER = 0;
			TOTAL_NUM_OF_REFRESHES++;
		}
	}

	//Intuitively, the desire candidate should be the one with the lowest class
	private int findLowestClassVictim()
	{
		int mClass = 0;
		int victimPageNumber = 0;

		//Set min class to the max class in order to do comparison
		int minClass = 3;

		int tempPageNumber = 0;
		int tempRefBit = 0;
		int tempDirtyBit = 0;

		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			tempPageNumber = FRAME_TABLE[i];
			tempRefBit = PAGE_TABLE.get(tempPageNumber).rBit;
			tempDirtyBit = PAGE_TABLE.get(tempPageNumber).dBit;

			if(tempRefBit == 0 && tempDirtyBit == 0) mClass = 0;
			else if(tempRefBit == 0 && tempDirtyBit == 1) mClass = 1;
			else if(tempRefBit == 1 && tempDirtyBit == 0) mClass = 2;
			else if(tempRefBit == 1 && tempDirtyBit == 1) mClass = 3;

			if(mClass <= minClass)
			{
				minClass = mClass;
				victimPageNumber = tempPageNumber;
			}
		}

		return victimPageNumber;
	}


	private void work()
	{
		FRAME_TABLE = new int[NUM_OF_FRAMES];
		FULL_FRAME_COUNT = 0;
		REFRESH_COUNTER = 0;

		//Initialize the mock current system time
		CURRENT_VIRTUAL_TIMESTAMP = 0;

		for(int i = 0;i < NUM_OF_FRAMES;i++)
		{
			FRAME_TABLE[i] = -1;
		}

		PAGE_TABLE = new HashMap<Integer, PageTableEntry>();
		for(int i = 0;i < PAGE_TABLE_SIZE;i++)
		{
			PageTableEntry entry = new PageTableEntry();
			PAGE_TABLE.put(i, entry);
		}

		int lastRamPosition = 0;

		try {
			File file = new File(FILE_NAME);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			
			String line;
			String memAddress;
			String operation;

			while ((line = bufferedReader.readLine()) != null) {
				String [] splited = line.split("\\s+");
				memAddress = splited[0];
				operation = splited[1];

				//Take upper 5 hex bits for the page number from the address
				//The lower 3 is for the offset
				int pn = hexToDecimal(memAddress.substring(0,5));
				PageTableEntry entry = PAGE_TABLE.get(pn);

				//Reference bit is initialized to 1
				entry.rBit = 1;

				//Only set the dirty bit to 1 when encounters a one
				if(operation.equalsIgnoreCase(OPERATION_WRITE))
				{
					entry.dBit = 1;
				}

				if(entry.vBit == 0)
				{
					//Valid bit off, page fault occurs
					TOTAL_NUM_PAGE_FAULT++;

					if(FULL_FRAME_COUNT >= NUM_OF_FRAMES)
					{	

						boolean isUnreferencedCleanPage = false;

						int victimPageNumber = FRAME_TABLE[0];
						PageTableEntry victim = PAGE_TABLE.get(victimPageNumber);

						//Leave an timestamp to mark the oldest page
						int oldestTimeStamp = CURRENT_VIRTUAL_TIMESTAMP;
						int oldestPageNumber = -1;

						search: for(int i = 0;i < NUM_OF_FRAMES;i++)
						{	
							//Retrieve the last ram position
							int ramPointer = (lastRamPosition + i) % NUM_OF_FRAMES;
							
							int tempPageNumber = FRAME_TABLE[ramPointer];
							
							PageTableEntry tempEntry = PAGE_TABLE.get(tempPageNumber);

							int tempRefBit = tempEntry.rBit;
							int tempDirtyBit = tempEntry.dBit;
							int tempTimeStamp =	tempEntry.timeStamp;

							int age = CURRENT_VIRTUAL_TIMESTAMP - tempTimeStamp;

							//Bookkeep the oldest page timestamp
							//If a refresh just occurred and there is no unreferenced&clean page
							//Evict a random page -> The first page in the frame table
							//Note that if the victim is determined by the oldest timestamp
							//The performance of working set downgrades to FIFO
							if(tempTimeStamp < oldestTimeStamp)
							{
								oldestTimeStamp = tempTimeStamp;
								oldestPageNumber = tempPageNumber;
									
								victimPageNumber = tempPageNumber;
								victim = tempEntry;
							}

							//R = 0
							if(tempRefBit == 0)
							{	
								//R = 0 , D = 0, evict immediately
								if(tempDirtyBit == 0)
								{
									victimPageNumber = tempPageNumber;
									victim = tempEntry;

									//System.out.println("Found a Unreferenced&CleanPage!");
									break search;
								}
								//R = 0, D = 1, Age > TAU, write to disk and set 
								else if(tempDirtyBit == 1 && age > TAU)
								{	
									TOTAL_NUM_DISK_WRITE++;
									tempEntry.dBit = 0;		
								}

								//R = 0, D = 1, Age < TAU, no-op
							}
							//R = 1
							else
							{	
								//R = 1, D = 1, Age > TAU 
								if(tempDirtyBit == 1 && age > TAU)
								{	
									TOTAL_NUM_DISK_WRITE++;
									tempEntry.dBit = 0;
								}
								
							}

							PAGE_TABLE.put(tempPageNumber, tempEntry);
						}

						if(victim.dBit == 1)
						{
							TOTAL_NUM_DISK_WRITE++;
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict Dirty!" + "\t" + Arrays.toString(FRAME_TABLE) + "\t" + lastRamPosition);
						}
						else
						{
							if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault at evict clean!" + "\t" + Arrays.toString(FRAME_TABLE) + "\t" + lastRamPosition);
						}

						
						lastRamPosition = victim.frameNumber;

						//System.out.println("victim's page number: " + "\t" + victimPageNumber + " victim's frameNum: " + "\t" + victim.frameNumber);
						FRAME_TABLE[victim.frameNumber] = pn;
						entry.frameNumber = victim.frameNumber;
						entry.vBit = 1;
						entry.timeStamp = CURRENT_VIRTUAL_TIMESTAMP;


						//Update victim page to page table
						victim.timeStamp = 0;
						victim.rBit = 0;
						victim.dBit = 0;
						victim.vBit = 0;
						victim.frameNumber = -1;
						PAGE_TABLE.put(victimPageNumber, victim);

					}
					else
					{
						if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tPage fault – no eviction!" + "\t" + Arrays.toString(FRAME_TABLE) + "\t" + lastRamPosition);
						FRAME_TABLE[FULL_FRAME_COUNT] = pn;
						entry.frameNumber = FULL_FRAME_COUNT;
						entry.vBit = 1;

						//Update the time of last use
						entry.timeStamp = CURRENT_VIRTUAL_TIMESTAMP;
						FULL_FRAME_COUNT++;
					}
				}
				else
				{
					if(DETAIL_INFO) System.out.println(pn + "\t" + memAddress + "\t" + operation + "\tHit!" + "\t" + Arrays.toString(FRAME_TABLE) + "\t" + lastRamPosition);
				}

				TOTAL_NUM_MEMORY_ACCESS++;

				//Increment virtual timestamp
				CURRENT_VIRTUAL_TIMESTAMP++;

				REFRESH_COUNTER++;
				//Periodic refresh
				refreshReferenceBitWork();
			
				PAGE_TABLE.put(pn, entry);
			}

			fileReader.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	private void refreshReferenceBitWork()
	{
		//refresh -> set ref bit to 0 for all pages in the memory
				if(REFRESH_COUNTER == REFRESH_TIME)
				{	
					int tempPageNumber = 0;
					for(int i = 0; i < FULL_FRAME_COUNT;i++)
					{
						tempPageNumber = FRAME_TABLE[i];
						if(tempPageNumber != -1)
						{
							PageTableEntry refreshEntry = PAGE_TABLE.get(tempPageNumber);
							refreshEntry.rBit = 0;
							refreshEntry.timeStamp = CURRENT_VIRTUAL_TIMESTAMP;
							PAGE_TABLE.put(tempPageNumber, refreshEntry);
						}
					}

					REFRESH_COUNTER = 0;
					TOTAL_NUM_OF_REFRESHES++;
				}
	}

	private int hexToDecimal(String hex)
	{
		int result = Integer.decode("0x" + hex);
    	return result;
	}

	private String hexToBinary(String hex)
	{
		int i = Integer.parseInt(hex, 16);
		String result = Integer.toBinaryString(i);
		return result;
	}

	private void error()
	{	
		System.out.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		System.out.println("The system only supports four methods listed below:");
		System.out.println("Usage: For Opt -> java vmsim -n <numframes> -a opt <tracefile>");
		System.out.println("Usage: For Clock -> java vmsim -n <numframes> -a clock <tracefile>");
		System.out.println("Usage: For Nru -> java vmsim -n <numframes> -a nru -r <refresh_time> <tracefile>");
		System.out.println("Usage: For work -> java vmsim -n <numframes> -a work -r <refresh_time> -t <tau> <tracefile>");
		System.out.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}

	private void printFinalStats()
	{	
		System.out.println("******************************************************");
		System.out.println("Final stats are displayed here: ");
		System.out.println("Page Replacement algorithm is: " + TYPE_OF_ALGORITHM);
		System.out.println("File processed: " + FILE_NAME);
		System.out.println("Number of frames:\t" + NUM_OF_FRAMES);
		System.out.println("Total memory accesses:\t" + TOTAL_NUM_MEMORY_ACCESS);
		System.out.println("Total page faults:\t" + TOTAL_NUM_PAGE_FAULT);
		System.out.println("Total writes to disk:\t" + TOTAL_NUM_DISK_WRITE);
		System.out.println("Total run time is: " + TOTAL_RUN_TIME + " ms");
		System.out.println("Total number of refreshes: " + TOTAL_NUM_OF_REFRESHES);
		System.out.println("******************************************************");
	}

	//Return victim page number for NRU
	//The algorithm will return the candidate as soon as it finds a valid one
	//When equivalent classes condition occurs, always evict the first appeared one
	private int findLowestClass()
	{
		int victimPageNumber = -1;                                                   
		  
        // look for a first-choice victim                             
        for (int i = 0; i < NUM_OF_FRAMES; i++) 
        {
                         
            int tempPageNumber = FRAME_TABLE[i];
            int tempRefBit = PAGE_TABLE.get(tempPageNumber).rBit;
            int tempDirtyBit = PAGE_TABLE.get(tempPageNumber).dBit;
                                
            if (tempRefBit == 0 && tempDirtyBit == 0)
            {
                victimPageNumber = tempPageNumber;
                return victimPageNumber;
            }                            
        }
                                                        
        // look for a second-choice victim                             
        for (int i = 0; i < NUM_OF_FRAMES; i++) 
        {
                         
            int tempPageNumber = FRAME_TABLE[i];
            int tempRefBit = PAGE_TABLE.get(tempPageNumber).rBit;
            int tempDirtyBit = PAGE_TABLE.get(tempPageNumber).dBit;
                                
            if (tempRefBit == 0 && tempDirtyBit == 1)
            {
                victimPageNumber = tempPageNumber;
                return victimPageNumber;
            }                            
        }                        
        
        // look for a third class victim                             
        for (int i = 0; i < NUM_OF_FRAMES; i++) 
        {
                         
            int tempPageNumber = FRAME_TABLE[i];
            int tempRefBit = PAGE_TABLE.get(tempPageNumber).rBit;
            int tempDirtyBit = PAGE_TABLE.get(tempPageNumber).dBit;
                                
            if (tempRefBit == 1 && tempDirtyBit == 0)
            {
                victimPageNumber = tempPageNumber;
                return victimPageNumber;
            }                            
        }

        // look for a fourth class victim                             
        for (int i = 0; i < NUM_OF_FRAMES; i++) 
        {
                         
            int tempPageNumber = FRAME_TABLE[i];
            int tempRefBit = PAGE_TABLE.get(tempPageNumber).rBit;
            int tempDirtyBit = PAGE_TABLE.get(tempPageNumber).dBit;
                                
            if (tempRefBit == 1 && tempDirtyBit == 1)
            {
                victimPageNumber = tempPageNumber;
                return victimPageNumber;
            }                            
        }    

        return victimPageNumber;
	}

	//Inner class represents page table entry
	private class PageTableEntry
	{
		private int timeStamp;
		private int rBit;
		private int dBit;
		private int vBit;
		private int frameNumber;


		private PageTableEntry()
		{	
			this.timeStamp = 0;
			this.rBit = 0;
			this.dBit = 0;
			this.vBit = 0;
			this.frameNumber = -1;
		}
	}

}