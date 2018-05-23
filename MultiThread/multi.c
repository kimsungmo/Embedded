/* Embedded System 
Multi Thread Pratice
2013041021 김성모*/

//header
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <mysql/mysql.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

//Macro
#define CS_MCP3208  8        // BCM_GPIO_8
#define SPI_CHANNEL 0
#define SPI_SPEED   1000000   // 1MHz
#define VCC         4.8       // Supply Voltage
#define MAXTIMINGS 85
#define FAN	22
#define RGBLEDPOWER 24
#define MAX 5
#define LIGHTSEN_OUT 2

//DataBase
#define DBHOST "localhost"
#define DBUSER "root"
#define DBPASS "root"
#define DBNAME "demofarmdb"
MYSQL *connector;
MYSQL_RES *result;
MYSQL_ROW row;
char query[1024];


//Thread
pthread_cond_t empty, fill;
pthread_mutex_t mutex, mutex2;


//Function
int wiringPicheck(void);
int read_dht22_dat_temp();
void get_temperature_sensor();
void fanon();
void fanoff();
void tempcheck(int value);
void ledon();
void ledoff();
void lightcheck(int value);


//Global Variable
int received_temp;
int ret_temp;
static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};
int buffer2[MAX];
int buffer[MAX];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;
int count2 = 0;
int temperature_count = 0;
int counter2 = 0;
int adcValue = 0;


int read_mcp3208_adc(unsigned char adcChannel)
{
	unsigned char buff[3];

  	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);
  	buff[1] = ((adcChannel & 0x07) << 6);
  	buff[2] = 0x00;

  	digitalWrite(CS_MCP3208, 0);  // Low : CS Active

  	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);

  	buff[1] = 0x0F & buff[1];
  	adcValue = ( buff[1] << 8) | buff[2];

  	digitalWrite(CS_MCP3208, 1);  // High : CS Inactive

	printf("Light = %d\n",adcValue);
  	return adcValue;
}


void Bpluspinmodeset(void)
{
	pinMode(FAN, OUTPUT);
	pinMode(RGBLEDPOWER, OUTPUT);
	pinMode(CS_MCP3208, OUTPUT);
}


//Thread Function
void put_lightness(int value){
	buffer2[fill_ptr] = value;
	fill_ptr = (fill_ptr + 1) % MAX;
	count2++;
}


int get_lightness(){
	int tmp = buffer2[use_ptr];
	use_ptr = (use_ptr + 1) % MAX;
	count2--;
	return tmp;
}


void put_temperature(int value){
	buffer[fill_ptr] = value;
	fill_ptr = (fill_ptr + 1) % MAX;
	count++;
}


int get_temperature(){
	int tmp = buffer[use_ptr];
	use_ptr = (use_ptr + 1) % MAX;
	count--;
	return tmp;
}


void *temperature_lightness(void *arg){
	while(1)
	{
		delay(1100);
		pthread_mutex_lock(&mutex);
		while(count == MAX)
			pthread_cond_wait(&empty, &mutex);
		put_temperature(received_temp);
		put_lightness(adcValue);
		pthread_cond_signal(&fill);
    		pthread_mutex_unlock(&mutex);
	}
}


void *sendData(void *arg){
	while(1)
	{
		sleep(10);
		int temp = get_temperature();
		int temp2 = get_lightness();

		pthread_mutex_lock(&mutex2);
		
		sprintf(query,"insert into smartfarm values (now(),%d,%d)",temp,temp2);

    		if(mysql_query(connector, query))
    		{
      			fprintf(stderr, "%s\n", mysql_error(connector));
      			printf("Write DB error\n");
    		}
		pthread_mutex_unlock(&mutex2);
	}
}


void *fan(void *arg){
	while(1)
	{
		delay(1100);
		pthread_mutex_lock(&mutex);
		while(count == 0)
			pthread_cond_wait(&fill, &mutex);
		int temp = get_temperature();
		tempcheck(temp);	
		pthread_cond_signal(&empty);
		pthread_mutex_unlock(&mutex);
	}
}


void *led(void *arg){
	while(1)
	{
		delay(1100);
		pthread_mutex_lock(&mutex);
		while(count == 0)
			pthread_cond_wait(&fill, &mutex);
		int temp = get_lightness();
		lightcheck(temp);	
		pthread_cond_signal(&empty);
		pthread_mutex_unlock(&mutex);
	}
}



void tempcheck(int value){
	if(value > 25)
		counter2++;

	if (counter2 > 5){
		fanon();
		delay(1000);
		fanoff();
		counter2 = 0;
	}
}


void fanon(){
	if(wiringPicheck()) printf("Fail");
	digitalWrite(FAN, 1);
}


void fanoff(){
	if(wiringPicheck()) printf("Fail");
	digitalWrite(FAN, 0);
}

void lightcheck(int value){
	if(value > 1400){
		ledon();
		delay(500);
		ledoff();
	}
}


void ledon(){
	if(wiringPicheck()) printf("Fail");
	digitalWrite(RGBLEDPOWER, 1);
}


void ledoff(){
	if(wiringPicheck()) printf("Fail");
	digitalWrite(RGBLEDPOWER, 0);
} 

int main(void)
{		
	//wiringPi Check
	if(wiringPicheck()) printf("Fail");

  	if(wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1)
  	{
    		fprintf (stdout, "wiringPiSPISetup Failed: %s\n", strerror(errno));
    		return 1 ;
  	}	
	
	connector = mysql_init(NULL);
	
	if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
  	{
    		fprintf(stderr, "%s\n", mysql_error(connector));
    		return 0;
  	}

	//Set
	Bpluspinmodeset();

	//Thread
	pthread_t p1,p2,p3,p4;

        pthread_create(&p1, NULL, temperature_lightness, NULL);
	pthread_create(&p2, NULL, led, NULL);
	pthread_create(&p3, NULL, fan, NULL);
	pthread_create(&p4, NULL, sendData, NULL);	

	printf("센서 동작\n");
	while(1)
	{		
		get_temperature_sensor();
		read_mcp3208_adc(0);			 
	}
	mysql_close(connector);
}


static uint8_t sizecvt(const int read)
{
	/* digitalRead() and friends from wiringpi are defined as returning a value
	< 256. However, they are returned as int() types. This is a safety function */

	if (read > 255 || read < 0)
	{
    		printf("Invalid data from wiringPi library\n");
    		exit(EXIT_FAILURE);
  	}
  	return (uint8_t)read;
}


int wiringPicheck(void)
{
	if (wiringPiSetup () == -1)
	{
		fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
		return 1 ;
	}
}


void get_temperature_sensor()
{
	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;

	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}
	
	DHTPIN = 11;

	while (read_dht22_dat_temp() == 0)
	{
		delay(1000); // wait 1sec to refresh
	}
	received_temp = ret_temp;
	printf("온도 = %d ", received_temp);
}


int read_dht22_dat_temp()
{
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;

  dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40);
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 50)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) &&
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t;

        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

		ret_temp = (int)t;
		
	return ret_temp;
  }
  else
  {
    //printf("Data not good, skip\n");
    return 0;
  }
}



