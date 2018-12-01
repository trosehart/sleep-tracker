/**********************************************************************************

File: sleep_record.c

Purpose: This program records data on sleep patterns.  Using two ultrasonic sensors 
	and two sound sensors, it records when sounds are made and measures distance 
        to see motion with the ultrasonics.  

	It records the data in a file for stats, then creates a report on the data.
        
        Note:
        The functions setToOutput and getTime are from sample code given by ECE150 
        professors/TAs, as well as part of the main function (dealing with watchdog).  
        Some changes have been made to make these functions work for our program, but 
        they are someone else's ideas.
        
Date: December 3, 2018

**********************************************************************************/

#include "gpiolib_addr.h"
#include "gpiolib_reg.h"

#include <stdint.h>
#include <stdio.h>		//for the printf() function
#include <fcntl.h>
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#include <unistd.h> 		//needed for sleep
#include <sys/ioctl.h> 		//needed for the ioctl function
#include <stdlib.h> 		//for atoi
#include <time.h> 		//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()

#include <string.h>
#include <math.h>

//Below is a macro that had been defined to output appropriate logging messages
//file        - will be the file pointer to the log file
//time        - will be the current time at which the message is being printed
//programName - will be the name of the program, in this case it will be Lab4Sample
//str         - will be a string that contains the message that will be printed to the file.
#define PRINT_MSG(logFile, time, programName, str) \
	do{ \
			fprintf(logFile, "%s : %s : %s", time, programName, str); \
			fflush(logFile); \
	}while(0)


//for printing data to files
//-file is the file that is beig printed to, num is the value to be printed
//-used for recording ultrasonic and sound sensor data
#define PRINT_DATA(file, num) \
	do{ \
			fprintf(file, "%d ", num); \
			fflush(file); \
	}while(0)

//for printing analysed data to the report file
//takes file, message, time, and data point
#define PRINT_ANALYSIS(file, str, min, sec, data) \
	do{ \
        		fprintf(file, "%s: %d:%d - %d\n" , str, min, sec, data); \
                        fflush(file); \
	}while(0)



//This function will change the appropriate pins value in the select register
//so that the pin can function as an output
void setToOutput(GPIO_Handle gpio, int pinNumber)
{
	//Check that the gpio is functional
	if(gpio == NULL)
	{
		printf("The GPIO has not been intitialized properly \n");
		return;
	}

	//Check that we are trying to set a valid pin number
	if(pinNumber < 2 || pinNumber > 27)
	{
		printf("Not a valid pinNumber \n");
		return;
	}

	//This will create a variable that has the appropriate select
	//register number. For more information about the registers
	//look up BCM 2835.
	int registerNum = pinNumber / 10;

	//This will create a variable that is the appropriate amount that 
	//the 1 will need to be shifted by to set the pin to be an output
	int bitShift = (pinNumber % 10) * 3;

	//variables for the register number and the bit shift
	uint32_t sel_reg = gpiolib_read_reg(gpio, GPFSEL(registerNum));
	sel_reg |= 1  << bitShift;
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);
}

//This is a function used to read from the config file.

/*

#sample config file#

#watchdog timeout#
WATCHDOG_TIMEOUT = 6
 
#log file name#
LOG_FILE = /home/pi/sleep_log.log

#stat file name#
ULTRA_STAT_FILE = /home/pi/sleep_ultra_stats.txt

#stat file name#
SOUND_STAT_FILE = /home/pi/sleep_sound_stats.txt

#report file name#
REPORT_FILE = /home/pi/sleep_report.txt

#how long program records data for in minutes#
RUN_LENGTH = 1

 */

enum ReadState {START, VAR_NAME, WHITESPACE, VALUE, FILE_NAME, COMMENT, DONE};
//function to read config file
void readConfig(FILE* configFile, int* timeout, char* logFileName, char* ultraDataName, char* soundDataName,  char* reportFileName, int* timeLimit)
{
  	char logDef[50] = "/home/pi/defaultLog.log";
	
	char ultraDef[50] = "/home/pi/defaultUltra.txt";
	
	char soundDef[50] = "/home/pi/defaultSound.txt";
	
	char reportDef[50] = "/home/pi/defaultRep.txt";
  
  	for (int i = 0; i < 50; i++) {
		logFileName[i] = 0;
		ultraDataName[i] = 0;
		soundDataName[i] = 0;
		reportFileName[i] = 0;
	}
  
	//if the config file does not exist, it sets default values
  	if(!configFile) {
          	for (int i = 0; i < 50; i++) {
                        logFileName[i] = logDef[i];
                }
                for (int i = 0; i < 50; i++) {
                        ultraDataName[i] = ultraDef[i];
                } 
          	for (int i = 0; i < 50; i++) {
                        reportFileName[i] = reportDef[i];
                } 
          	for (int i = 0; i < 50; i++) {
                        soundDataName[i] = soundDef[i];
                }
          
                *timeout = 15;
                
                *timeLimit = 1;
          
          	return;
        }
	
  	//Loop counter
	int i = 0;
	
	//A char array to act as a buffer for the file
	char buffer[255];
	fgets(buffer, 255, configFile);

	//The value of the timeout variable is set to zero at the start
	*timeout = 0;
  
  	//The value of record length is set to 0
  	*timeLimit = 0;

	//This is a variable used to track which input we are currently looking
	//for (timeout, logFileName or numBlinks)
	int input = 0;
  
  	//storing the names of variables to make sure they are what is needed
  	char varName[100];
  	int varNamePos = 0;
  
  	int filePos = 0;
  
  	//if there is an equal sign for WHITESPACE state (because equal signs should have whitespace on both sides)
  	int gotEquals = 0;
  	//
  	enum ReadState s = START;
  
  	int counter = 0;
  	
  	fgets(buffer, 255, configFile);
	

  
  	while(s != DONE) {
          	if(buffer[counter] == '\n') {
                  	fgets(buffer, 255, configFile);

                  	counter = 0;
                }
          
          	if(buffer[counter] == 0) {
                  	s = DONE;	
                }
          
          	switch(s) {
                  	case(START):

                    		//if first char is a comment
                    		if(buffer[counter] == '#') {
                                  	s = COMMENT;
                                }
                    		//if whitespace
                    		if(buffer[counter] == ' ' || buffer[counter] == '\n') {
                                  	s = WHITESPACE;
                                }
                    		//if it is a letter character, it must be the start of a variable name
                    		if(buffer[counter] >= 'A' && buffer[counter] <= 'z') {
                                  	s = VAR_NAME;
                                  
                                  	varName[varNamePos] = buffer[counter];
                                  	++varNamePos;
                                }
                    		break;
                    
                    	//if the current item is the name of a variable
                    	case(VAR_NAME):

                          	if(buffer[counter] >= 'A' && buffer[counter] <= 'z') {
                                  	varName[varNamePos] = buffer[counter];
                                  	++varNamePos;
                                }
                          	else {
                                  	varNamePos = 0;
                                  	if(buffer[counter] == ' ' || buffer[counter] == '\n') {
                                                s = WHITESPACE;
                                        }
                                        else if(buffer[counter] == '#') {
                                                s = COMMENT;
                                        }
                                        else if(buffer[counter] == '=') {
                                                s = WHITESPACE;
                                        }
                                }
                          	break;
                    	
                    	//if item is whitespace
                    	case(WHITESPACE):

                    		if(buffer[counter] == '#'){
                                  	s = COMMENT;
                                }
                    		else if(buffer[counter] == '=') {
                                  	gotEquals = 1;
                                }
                                else if(buffer[counter] == ' ') {
                                        s = WHITESPACE;
                                }
                    		else if(gotEquals && buffer[counter] >= '0' && buffer[counter] <= '9'){

                                  	s = VALUE;
                                  	--counter;
                                }
                    		else if(gotEquals && ((buffer[counter] >= 'A' && buffer[counter] <= 'z') || buffer[counter] == '/' || buffer[counter] == ':')) {
                                  	s = FILE_NAME;
                                  	--counter;
                                }
                                else if(!gotEquals && ((buffer[counter] >= 'A' && buffer[counter] <= 'z') || buffer[counter] == '/' || buffer[counter] == ':')) {
                                  	s = VAR_NAME;
                                  	--counter;
                                }
                    		
                    		break;
                    
                    	//if it is a number value to be recorded
                    	case(VALUE):        

                    		if(buffer[counter] >= '0' && buffer[counter] <= '9') {
                                  	if(!strncmp(varName, "WATCHDOG_TIMEOUT", 5)) {
                                          	*timeout = *timeout*10 + (buffer[counter]-'0');
                                        }
                                  	if(!strncmp(varName, "RUN_LENGTH", 5)) {
                                          	*timeLimit = *timeLimit*10 + (buffer[counter]-'0');
                                        }
                                }
                    		else {
                                  	gotEquals = 0;
                                  	varNamePos = 0;
                                  	for(int i = 0; i < 100; i++) {
                                          	varName[i] = 0;
                                        }
                                  	if(buffer[counter] == '#'){
                                  		s = COMMENT;
                                	}
                                  	else if(buffer[counter] == ' ' || buffer[counter] == '\n') {
                                          	s = WHITESPACE;
                                        }
                                        else if(buffer[counter] >= 'A' && buffer[counter] <= 'z') {
                                                s = VAR_NAME;
                                                varName[0] = buffer[counter];
                                                varNamePos = 1;
                                        }
                                }
                    		break;
                    
                    	//if it is the name of a file to be recorded
                    	case(FILE_NAME):
                    		if((buffer[counter] >= 'A' && buffer[counter] <= 'z') || buffer[counter] == '/' || buffer[counter] == '.' || buffer[counter] == ':') {

                                  	if(!strncmp(varName, "LOG_FILE", 7)) {
                                          	logFileName[filePos] = buffer[counter];
                                        }
                                  	if(!strncmp(varName, "ULTRA_STAT_FILE", 7)) {
                                         	ultraDataName[filePos] = buffer[counter];
                                        }
                                  	if(!strncmp(varName, "SOUND_STAT_FILE", 7)) {
                                          	soundDataName[filePos] = buffer[counter];
                                        }
                                        if(!strncmp(varName, "REPORT_FILE", 7)) {
                				reportFileName[filePos] = buffer[counter];
                                        }
                                  	++filePos;
                                }
                    		else {
					/*if (log) {
                                                logFileName[filePos] = 0;
                                                log = 0;
                                        }
                                        else if (ultra) {
                                                ultraDataName[filePos] = 0;
                                                ultra = 0;
                                        }
                                        else if (sound) {
                                                soundDataName[filePos] = 0;
                                                sound = 0;
                                        }
                                        else if (report) {
                                                reportFileName[filePos] = 0;
                                                report = 0;
                                        }*/

					fgets(buffer, 255, configFile);

					counter = 0;			
                                  	gotEquals = 0;
                                  	varNamePos = 0;
                                  	filePos = 0;
                                  	
                                  	for(int i = 0; i < 100; i++) {
                                          	varName[i] = 0;
                                        }
                                  	if(buffer[counter] == '#'){
                                  		s = COMMENT;
                                	}
                                  	else if(buffer[counter] == ' ' || buffer[counter] == '\n') {
                                          	s = WHITESPACE;
                                        }
                                        else if(buffer[counter] >= 'A' && buffer[counter] <= 'z') {
                                                s = VAR_NAME;
                                                varName[0] = buffer[counter];
                                                varNamePos = 1;
                                        }
                                }
                    		
                    		break;
			
                    	//ignores everything in a comment until the end of the comment
                    	case(COMMENT):

                    		if(buffer[counter] == '#' || buffer[counter] == '\n'){
                                  	s = WHITESPACE;
                                }
                    		break;
                    
                    	//makes sure all variables were given a value
                    	//if not, they are set to defaults
                    	case(DONE):
							
                            	if(logFileName[0] == 0) {
                                        for (int i = 0; i < 50; i++) {
                                                logFileName[i] = logDef[i];
                                        }
                           	}
                    		if(ultraDataName[0] == 0) {
                                        for (int i = 0; i < 50; i++) {
                                                ultraDataName[i] = ultraDef[i];
                                        }  	
                            	}				
                            	if(reportFileName[0] == 0) {
                                        for (int i = 0; i < 50; i++) {
                                                reportFileName[i] = reportDef[i];
                                        } 	
                            	}	
                                if(soundDataName[0] == 0) {
                                        for (int i = 0; i < 50; i++) {
                                                soundDataName[i] = soundDef[i];
                                        }
                            	}
						
                    		//timeout of 15 seconds
                                if(*timeout == 0) {
                                        *timeout = 10;
                                }
                            //time limit for program to run of 1 minute
                                if(*timeLimit == 0) {
                                        *timeLimit = 1;
                                }
                    
                                break;
                    
                  	default:
                		break;
                }
                ++counter;
        }
}

//This function will get the current time using the gettimeofday function
void getTime(char* buffer)
{
	//Create a timeval struct named tv
  	struct timeval tv;

	//Create a time_t variable named curtime
  	time_t curtime;

	//Get the current time and store it in the tv struct
  	gettimeofday(&tv, NULL); 

	//Set curtime to be equal to the number of seconds in tv
  	curtime=tv.tv_sec;

	//This will set buffer to be equal to a string that in
	//equivalent to the current date, in a month, day, year and
	//the current time in 24 hour notation.
  	strftime(buffer,30,"%m-%d-%Y  %T.",localtime(&curtime));

} 



//finding time in microseconds for the ultrasonic sensors
long getMicroTime(){
	struct timeval currentTime;
	gettimeofday(&currentTime,NULL);
	return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

//This function should initialize the GPIO pins
GPIO_Handle initializeGPIO(FILE* logFile, char programName[]) {
    GPIO_Handle gpio;
    gpio = gpiolib_init_gpio();
    if (gpio == NULL) {
	char time[30];
	getTime(time);

        PRINT_MSG(logFile, time, programName, "Could not initialize GPIO\n\n");
    }
    return gpio;
}

//function for turning on pins given the pin number
void turnOn(GPIO_Handle gpio, int pinNum)
{
  	if(pinNum < 2 || pinNum > 27)
	{
		printf("Not a valid pinNumber \n");
		return;
	}
  	
	gpiolib_write_reg(gpio, GPSET(0), 1 << pinNum);
}
//function for turning off pins given the pin number
void turnOff(GPIO_Handle gpio, int pinNum)
{
  	if(pinNum < 2 || pinNum > 27)
	{
		printf("Not a valid pinNumber \n");
		return;
	}
  	
	gpiolib_write_reg(gpio, GPCLR(0), 1 << pinNum);
}

//pins 14 and 15 are ECHO inputs for the ultrasonic sensors
#define ULTRA1_ECHO 14
#define ULTRA2_ECHO 15
//pins 17 and 18 are outputs for TRIG
#define ULTRA1_TRIG 17
#define ULTRA2_TRIG 18

//returns if there is an error
#define ULTRA_ERROR -1
long getDistanceData(GPIO_Handle gpio, int ultraNum) {
	
	if(gpio == NULL) {
		return ULTRA_ERROR;
	}

	long initial_time;
	long final_time;

	if(ultraNum == 1) {
		turnOn(gpio,ULTRA1_TRIG);
		usleep(1000);
		turnOff(gpio,ULTRA1_TRIG);

		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));
		//pin 14 is ECHO input
		int pin_state = level_reg & (1 << ULTRA1_ECHO);

		//waits for echo to start
		while(!pin_state){
			level_reg = gpiolib_read_reg(gpio, GPLEV(0));
			pin_state = level_reg & (1 << ULTRA1_ECHO);
		}
		initial_time = getMicroTime();
		//wait for echo to end
		while(pin_state){
			level_reg = gpiolib_read_reg(gpio, GPLEV(0));
			pin_state = level_reg & (1 << ULTRA1_ECHO);
		}
		final_time = getMicroTime();

		//calculating distance using speed of sound estimate (343m/s)
		long distance = (final_time-initial_time)/58;
		
		//if the distance is over this, it is invalid
		if(distance >= 1000) {
			return ULTRA_ERROR;
		}

		return distance;
	}

	else if(ultraNum == 2) {
		turnOn(gpio,ULTRA2_TRIG);
		usleep(1000);
		turnOff(gpio,ULTRA2_TRIG);

		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));
		//pin 15 is ECHO input
		int pin_state = level_reg & (1 << ULTRA2_ECHO);

		//waits for echo to start
		while(!pin_state){
			level_reg = gpiolib_read_reg(gpio, GPLEV(0));
			pin_state = level_reg & (1 << ULTRA2_ECHO);
		}
		initial_time = getMicroTime();
		//wait for echo to end
		while(pin_state){
			level_reg = gpiolib_read_reg(gpio, GPLEV(0));
			pin_state = level_reg & (1 << ULTRA2_ECHO);
		}
		final_time = getMicroTime();

		//calculating distance using speed of sound estimate (343m/s)
		long distance = (final_time-initial_time)/58;

		//if the distance is over this, it is invalid
		if(distance >= 1000) {
			return ULTRA_ERROR;
		}

		return distance;
	}
	
	//invalid ultrasonic number
	else {
		return ULTRA_ERROR;
        }
}

//SOUND SENSOR
//pins 23 and 24 are inputs for the sound sensors
#define SOUND1_PIN 23
#define SOUND2_PIN 24
//error for any problems
#define SOUND_ERROR -2
long getSoundData(GPIO_Handle gpio, int soundNum) {

	if(gpio == NULL) {
		return SOUND_ERROR;
	}

  	//sound sensor 1
	if(soundNum == 1) {
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));
		//returns pin state of the sensor (0 if no sound, 1 if sound)
		if (level_reg & (1 << SOUND1_PIN))
		    return 1;
		else
		    return 0;
	}
	//sound sensor 2
	else if(soundNum == 2) {
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));
		//returns pin state of the sensor
		if (level_reg & (1 << SOUND2_PIN))
		    return 1;
		else
		    return 0;
	}

	else
		return SOUND_ERROR;
}

//RECORDING DATA
//if there are errors from the sensors, they are recorded in the log file
//this function is for recording ultrasonic distances
void printUltraToFile(GPIO_Handle gpio, FILE* ultraData, FILE* logFile, char programName[], long* ultraData1[], long* ultraData2[], int k) {
	
  
  	if (!ultraData) {
          printf("Unable to open ultraData file\n");
          return;
        }
  	if (!logFile) {
          printf("Unable to open log file\n");
          return;
        }
	char time[30];
	getTime(time);
  
	//measuring and calculating distances
	long dist1 = getDistanceData(gpio, 1);
	long dist2 = getDistanceData(gpio, 2);
  	*ultraData1[k] = dist1;
  	*ultraData2[k] = dist2;

	//recording ultrasonic distances, records error if there is an error
	if(dist1 == ULTRA_ERROR) {
		PRINT_DATA(ultraData, ULTRA_ERROR);
		PRINT_MSG(logFile, time, programName, "Warning: Invalid ultrasonic data from sensor 1\n\n");
	}
  	//records valid readings
	else {
		PRINT_DATA(ultraData, dist1);
	}
	if(dist2 == ULTRA_ERROR) {
		PRINT_DATA(ultraData, ULTRA_ERROR);
		PRINT_MSG(logFile, time, programName, "Warning: Invalid ultrasonic data from sensor 2\n\n");
	}
	else {
		PRINT_DATA(ultraData, dist2);
	}

	return;

}
//this function is for recording sound
void printSoundToFile(GPIO_Handle gpio, FILE* soundData, FILE* logFile, char programName[], int* prev1, int* prev2, long startTime) {
  
  	if (!soundData) {
          printf("Unable to open soundData file\n");
          return;
        }
  	if (!logFile) {
          printf("Unable to open log file\n");
          return;
        }
  	char time[30];
	getTime(time);
  
  	//checking sound values
  	int sound1 = getSoundData(gpio, 1);
	int sound2 = getSoundData(gpio, 2);
  
  	//recording sound values, records the error if there is an error
	if(sound1 == SOUND_ERROR) {
		PRINT_DATA(soundData, SOUND_ERROR);
		PRINT_MSG(logFile, time, programName, "Warning: Invalid sound data from sensor 1\n\n");
	}
	else {
		if(sound1 == 1 && *prev1 != getMicroTime()/1000000) { 	
			*prev1 = getMicroTime()/1000000;	
			PRINT_DATA(soundData, *prev1-(startTime/1000000));
		}
	}	
	if(sound2 == SOUND_ERROR) {
		PRINT_DATA(soundData, SOUND_ERROR);
		PRINT_MSG(logFile, time, programName, "Warning: Invalid sound data from sensor 2\n\n");
	}
	else {
		//PRINT_DATA(soundData, sound2);
		if(sound2 == 1 && *prev2 != getMicroTime()/1000000) {
			*prev2 = getMicroTime()/1000000;
			PRINT_DATA(soundData, *prev2-(startTime/1000000));
		}
	}
  	return;
}

//function to find top time periods that had multiple sounds
//stores data by minute in byMinute array, and stores minute with most activity in topMinutes
void analyzeSound(FILE* soundFile, int* byMinute, int* topMinutes, int timeLimit) {
	if (!soundFile) {
        	printf("Unable to open soundData file\n");
          	return;
        }
        
        //number of highest minutes of sound data
  	const int NUM_TOP = timeLimit/6 + 1;
  	//char to store the value of what it reads
  	char c = fgetc(soundFile);
  	int curTimeSec = 0;
  
  	for(int i = 0; i < timeLimit; i++) {
          	byMinute[i] = 0;
        }
  
  	while(c != 0 && !feof(soundFile)) {
          	
          	while(c != ' ' && c != 0) {
                  	//if it has a sound error (-2)
                  	if(c == '-') {
                                //waits until it finds a space
                          	while(c != ' ' && c != 0) {
                                        c = fgetc(soundFile);
                                }
                        }

			if(c != 0) {
                          	//if not a null character, calculates a time value in seconds
                  		curTimeSec = curTimeSec*10 + (c-'0');
                  		c = fgetc(soundFile);
			}
                }
		if(c != 0) {
                  	//increments array of number of sounds per minute
		  	byMinute[curTimeSec/60] = byMinute[curTimeSec/60] + 1;
                  	//resets time value
		  	curTimeSec = 0;
		  	//reads the next character until it is not a space
		  	while(c == ' ') {
		          	c = fgetc(soundFile);
		        }
          	}
        }
  	
  	//topMinutes is minute that has highest value in byMinute
  	for(int i = 0; i < NUM_TOP || i < timeLimit; i++) {
          	topMinutes[i] = i;
        }
  	for(int i = NUM_TOP; i < timeLimit || i < timeLimit; i++) {
          	//sets the minutes that had the most sound activity to the topMinutes array
          	for(int j = 0; j < NUM_TOP; j++) {
                  	if(byMinute[i] > byMinute[topMinutes[j]]) {
                          	topMinutes[j] = i;
                          	j = NUM_TOP;
                        }	
                }
          	
        }
}

// Prints to report if change is more than 8cm
#define MIN_DIFF 8
void analyzeUltra(FILE* reportFile, long* ultraData1[], long* ultraData2[], const int k, const int loopTime) {
	
  	if(!reportFile) {
          printf("Unable to open report file\n");
          return;
        }
  
  	int passedMinutes = 0;
  	int passedSeconds = 0;
  
  	int j = 1;
  	int diff1 = 0;
  	int diff2 = 0;

  	// Goes through the array and checks for changes in movement
  	// Prints time in minute, seconds from movement
  	while (j <= k) {
		
          	passedSeconds = j * loopTime;
          	passedMinutes = passedSeconds/60;
          
        	if (*ultraData1[j] != -1 || *ultraData2[j] != -1) {
			if (*ultraData1[j] != -1 && *ultraData1[j-1] != -1)
                          	diff1 = fabs(*ultraData1[j] - *ultraData1[j-1]);
                  	else
                          	diff1 = 0;
                  	if (*ultraData2[j] != -1 && *ultraData2[j-1] != -1)
                          	diff2 = fabs(*ultraData2[j] - *ultraData2[j-1]);
                  	else
                          	diff2 = 0;
                }
          	else {
			diff1 = 0;
                  	diff2 = 0;
                }
          	// If distances is greater than certain difference, print it to analysis
          	if (diff1 > MIN_DIFF || diff2 > MIN_DIFF) {
			if (diff1 > diff2)
                          	PRINT_ANALYSIS(reportFile, "Movement at", passedMinutes, passedSeconds, diff1);
                        else
                          	PRINT_ANALYSIS(reportFile, "Movement at", passedMinutes, passedSeconds, diff2);
                }
          	j++;
        }
}


/**********************************

Functions above


Main below

**********************************/




int main(const int argc, const char* const argv[]) {
	//Create a string that contains the program name
	const char* argName = argv[0];

	//These variables will be used to count how long the name of the program is
	int i = 0;
	int namelength = 0;

	while(argName[i] != 0)
	{
		namelength++;
		i++;
	} 

	char programName[namelength];

	i = 0;

	//Copy the name of the program without the ./ at the start
	//of argv[0]
	while(argName[i + 2] != 0)
	{
		programName[i] = argName[i + 2];
		i++;
	} 	

	//Create a file pointer named configFile
	FILE* configFile;
	//Set configFile to point to the Lab4Sample.cfg file. It is
	//set to read the file.
	configFile = fopen("/home/pi/sleep_config.cfg", "r");

	//Output a warning message if the file cannot be openned
	if(!configFile)
	{
		perror("The config file could not be opened");
		return -1;
	}



	/*

		what is read from config file

		file includes: name of log file, name of stats file, how long user wants to record (?), etc.
		

		if file does not have these, use defaults of some sort
			(file with default name)

	*/
  
  	//values that are to be read from the config file
	int timeout;
	char logFileName[50];
  	char ultraDataName[50];
  	char soundDataName[50];
	char reportFileName[50];
	int timeLimit;
	
	readConfig(configFile, &timeout, logFileName, ultraDataName, soundDataName, reportFileName, &timeLimit);

	//Create a new file pointer to point to the log file
	FILE* logFile;
	//Set it to point to the file from the config file and make it append to the file when it writes to it.
	logFile = fopen(logFileName, "w");
	
  	//Create a new file pointer to point to the ultrasonic data record file
	FILE* ultraData;
	ultraData = fopen(ultraDataName, "w");
  	//Create a new file pointer to point to the sound data record file
	FILE* soundData;
	soundData = fopen(soundDataName, "w");
  
 	 //Create a new file pointer to point to the report file
	FILE* reportFile;
	reportFile = fopen(reportFileName, "w");
  
  	//close after reading what you need
	fclose(configFile);
  
  	char time[30];

	getTime(time);
  	//logs that files have been opened
  	PRINT_MSG(logFile, time, programName, "Files have been opened\n\n");

	getTime(time);
  	//logs that GPIO pins are ready
	GPIO_Handle gpio = initializeGPIO(logFile, programName);
	PRINT_MSG(logFile, time, programName, "The GPIO pins have been initialized\n\n");

	getTime(time);
  	//initializes ultrasonic pins
	setToOutput(gpio,ULTRA1_TRIG);
	setToOutput(gpio,ULTRA2_TRIG);
  	//logs that pins for the ultrasonic sensors have been set
	PRINT_MSG(logFile, time, programName, "The ultrasonic pins have been initialized\n\n");

	/*

		watchdog stuff

	*/
 
  	//This variable will be used to access the /dev/watchdog file, similar to how
	//the GPIO_Handle works
	int watchdog;

	//We use the open function here to open the /dev/watchdog file. If it does
	//not open, then we output an error message. We do not use fopen() because we
	//do not want to create a file if it doesn't exist
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
          	getTime(time);
		PRINT_MSG(logFile, time, programName, "Error: Couldn't open watchdog device! \n");
		return -1;
	} 
	//Log that the watchdog file has been opened
	getTime(time);
	PRINT_MSG(logFile, time, programName, "The Watchdog file has been opened\n\n");

	//This line uses the ioctl function to set the time limit of the watchdog
	//timer to 15 seconds. The time limit can not be set higher that 15 seconds
	//so please make a note of that when creating your own programs.
	//If we try to set it to any value greater than 15, then it will reject that
	//value and continue to use the previously set time limit
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);
	
	//Log that the Watchdog time limit has been set
	getTime(time);
	PRINT_MSG(logFile, time, programName, "The Watchdog time limit has been set\n\n");

	//The value of timeout will be changed to whatever the current time limit of the
	//watchdog timer is
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);
	//This print statement will confirm to us if the time limit has been properly
	//changed. The \n will create a newline character similar to what endl does.
	printf("The watchdog timeout is %d seconds.\n\n", timeout);
  
	PRINT_MSG(logFile, time, programName, "Waiting for user to enter bed.\n\n");
	//this loop waits for the user to get into bed before it allows the program to begin running
	while(getDistanceData(gpio,1) > 60 && getDistanceData(gpio,2) > 60 && getDistanceData(gpio,1) != ULTRA_ERROR && getDistanceData(gpio,2) != ULTRA_ERROR) {
		usleep(2000000);
		ioctl(watchdog, WDIOC_KEEPALIVE, 0);
	}
	getTime(time);
	PRINT_MSG(logFile, time, programName, "User has entered the bed.\nData collection has started.\n\n");


  	long startTime = getMicroTime();
  
  
  /****** 
   * 
   * 
   * 	How we are recording data
   * 
   * 	time limit, etc in while loop
   * 
   * 	ping watchdog and record in log
   * 
   * 
   *******/

  	//how much time must pass between ultrasonic recording data
  	//1000000 = 1sec
  	int loopTime = timeout-1;
	long* ultraData1[timeLimit/60 * (loopTime/1000000)];
        long* ultraData2[timeLimit/60 * (loopTime/1000000)];

  	int k = 0;
  	//prev1 and 2 make sure it doesn't record more than 1 data point per second for sound
  	int prev1 = -1;
  	int prev2 = -1;
  	int passedMinutes = 0;
  	int passedSeconds = 0;
  	//to confirm watchdog is pinged
	int isPinged = 0;
          
  	while((getMicroTime() - startTime)/1000000 < timeLimit * 60) {//(getMicroTime() - startTime)/1000000 < timeLimit*60) {

          	//records ultrasonic data and pings the watchdog every (timeOut-1) seconds
          	if((getMicroTime() - startTime)/1000000 % loopTime == 0 && !isPinged) {
                  	//This ioctl call will write to the watchdog file and prevent 
                        //the system from rebooting. It does this every (timeOut-1) seconds, so 
                        //setting the watchdog timer lower than this will cause the timer
                        //to reset the Pi
                        ioctl(watchdog, WDIOC_KEEPALIVE, 0);
                        getTime(time);
                        //Log that the Watchdog was kicked
                        PRINT_MSG(logFile, time, programName, "The Watchdog was pinged\n\n");
                  	//records ultrasonic data
                  	printUltraToFile(gpio, ultraData, logFile, programName, ultraData1, ultraData2, k);
                  	k++;

			isPinged = 1;
                }
          	//to stop ultrasonic functions to be called multiple times in same microsecond
		else if((getMicroTime() - startTime)/1000000 % loopTime != 0 && isPinged) {
			isPinged = 0;
		}
          	//records sound data
          	printSoundToFile(gpio, soundData, logFile, programName, &prev1, &prev2, startTime);
        }
  
 /*******
   * 
   * 	Analyze data
   * 
   * 	Trends in motion and sound, what time, etc.
   * 
   * 	Need functions for this
   * 
  ********/

	getTime(time);
	//logs that all data is gathered
	PRINT_MSG(logFile, time, programName, "Data collection complete\n\n");
	//prints to report file to make a new header for the current day
	PRINT_MSG(reportFile, time, programName, "THIS DAY'S REPORT:\n________________________________________________\n\n");
	//opens sound file to read
  	fclose(soundData);
  	soundData = fopen(soundDataName,"r");
  
  	getTime(time);
  
    	PRINT_MSG(reportFile, time, programName, "Report on sound data:\n\n");
  	//analyzing sound data
  	int topMinutes[(int)(timeLimit/6) + 1];
  	int byMinute[timeLimit];
  	analyzeSound(soundData, byMinute, topMinutes, timeLimit);
  
  	for(int i = 0; i < timeLimit/6 + 1; i++) {
          	PRINT_ANALYSIS(reportFile, "Greatest sound activity at", topMinutes[i]%60, (int)(topMinutes[i]/60), byMinute[topMinutes[i]]);
        }
  
  	PRINT_MSG(reportFile, time, programName, "Report on ultrasonic data:\n\n");

  	//analyzing ultrasonic data
  	analyzeUltra(reportFile, ultraData1, ultraData2, k, loopTime);
  	/*int j = 1;
  	int diff1 = 0;
  	int diff2 = 0;
  	// Prints to report if change is more than 8cm
  	const int difference = 8;

  	// Goes through the array and checks for changes in movement
  	// Prints time in minute, seconds from movement
  
  	while (j <= k) {
		
          	passedSeconds = j * loopTime;
          	passedMinutes = passedSeconds/60;
          
        	if (ultraData1[j] != -1 || ultraData2[j] != -1) {
			if (ultraData1[j] != -1 && ultraData1[j-1] != -1)
                          	diff1 = fabs(ultraData1[j] - ultraData1[j-1]);
                  	else
                          	diff1 = 0;
                  	if (ultraData2[j] != -1 && ultraData2[j-1] != -1)
                          	diff2 = fabs(ultraData2[j] - ultraData2[j-1]);
                  	else
                          	diff2 = 0;
                }
          	else {
			diff1 = 0;
                  	diff2 = 0;
                }
          	// If distances is greater than certain difference, print it to analysis
          	if (diff1 > difference || diff2 > difference) {
			if (diff1 > diff2)
                          	PRINT_ANALYSIS(reportFile, "Movement at", passedMinutes, passedSeconds, diff1);
                        else
                          	PRINT_ANALYSIS(reportFile, "Movement at", passedMinutes, passedSeconds, diff2);
                }
          	j++;
        }*/
  
  	//logging that a report was made
	PRINT_MSG(logFile, time, programName, "Report made on data\n\n");
  

  
  
 
  	//Writing a V to the watchdog file will disable to watchdog and prevent it from
	//resetting the system
	write(watchdog, "V", 1);
	getTime(time);
	//Log that the Watchdog was disabled
	PRINT_MSG(logFile, time, programName, "The Watchdog was disabled\n\n");

	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	getTime(time);
	//Log that the Watchdog was closed
	PRINT_MSG(logFile, time, programName, "The Watchdog was closed\n\n");

	//Free the gpio pins
	gpiolib_free_gpio(gpio);
	getTime(time);
	//Log that the GPIO pins were freed
	PRINT_MSG(logFile, time, programName, "The GPIO pins have been freed\n\n");


  
	return 0;
}