//Header
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
#define CS_MCP3208  8        // BCM_GPIO_8
#define MAXTIMINGS 85
#define RETRY 5

//Temperature
int ret_temp;
static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};
int read_dht22_dat_temp();
int get_temperature_sensor();
int received_temp;

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
pthread_mutex_t mutex;

//Producer and Consumer
int buffer[1];
int fill_ptr = 0;
int use_ptr = 0;
int count = 0;
void put(int value);
int get();


void *producer(void *arg){
  while(1)
  {    
    int i;
    pthread_mutex_lock(&mutex);
    while (count == 1)
      pthread_cond_wait(&empty, &mutex);
    put(received_temp);
    pthread_cond_signal(&fill);
    pthread_mutex_unlock(&mutex);
  }
}


void *consumer(void *arg){ 
  while(1)
  {
    delay(2500);
    int i;
    pthread_mutex_lock(&mutex);
    while (count == 0)
      pthread_cond_wait(&fill, &mutex);
    int temp = get();
    pthread_cond_signal(&empty);
    pthread_mutex_unlock(&mutex);

    sprintf(query,"insert into tem values (now(),%d)", temp);

    if(mysql_query(connector, query))
    {
      fprintf(stderr, "%s\n", mysql_error(connector));
      printf("Write DB error\n");
    }
  }
}


void put(int value){
  buffer[fill_ptr] = value;
  fill_ptr = (fill_ptr + 1) % 1;
  count++;
  printf("Temperature = %d\n", received_temp);    
}


int get(){
  int tmp = buffer[use_ptr];
  use_ptr = (use_ptr + 1) % 1;
  count--;
  printf("Temperature = %d\n", tmp);
  return tmp;
}


int main (void)
{
  pthread_t p1,p2;

  int adcValue[8] = {0};

  if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;
	
  if (setuid(getuid()) < 0)
  {
    perror("Dropping privileges failed\n");
    exit(EXIT_FAILURE);
  }

  pinMode(CS_MCP3208, OUTPUT);

  // MySQL connection
  connector = mysql_init(NULL);
  if (!mysql_real_connect(connector, DBHOST, DBUSER, DBPASS, DBNAME, 3306, NULL, 0))
  {
    fprintf(stderr, "%s\n", mysql_error(connector));
    return 0;
  }
  printf("MySQL(rpidb) opened.\n");

  pthread_create(&p1, NULL, producer, NULL);
  pthread_create(&p2, NULL, consumer, NULL);

  while(1)
  {
    get_temperature_sensor(); // Temperature Sensor
    delay(1000); //1sec delay
  }

  mysql_close(connector);

  return 0;
}


int get_temperature_sensor()
{
	int _retry = RETRY;

	DHTPIN = 11;

	while (read_dht22_dat_temp() == 0 && _retry--)
	{
		delay(500); // wait 1sec to refresh
	}
	received_temp = ret_temp ;
	return received_temp;
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
  // prepar
  
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
    printf("Data not good, skip\n");
    return 0;
  }
}