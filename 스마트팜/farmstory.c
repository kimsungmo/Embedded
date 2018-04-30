#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <wiringPi.h>

#define PUMP	21	// BCM_GPIO 5
#define FAN	22	// BCM_GPIO 6
#define DCMOTOR 23	// BCM_GPIO 13
#define RGBLEDPOWER  24 //BCM_GPIO 19

#define RED	7	//GPIO 16
#define GREEN	9	//GPIO 20
#define BLUE	8	//GPIO 21

#define LIGHTSEN_OUT 2  //gpio27 - J13 connect

#define MAXTIMINGS 85

void Bpluspinmodeset(void);
void sig_handler(int signo);

int ret_humid, ret_temp;

static int DHTPIN = 7;
static int dht22_dat[5] = {0,0,0,0,0};

int ret_humid, ret_temp;

int main (void)
{
	if(wiringPicheck()) printf("Fail");

	Bpluspinmodeset();

	signal(SIGINT, (void*)sig_handler);
	int curtemp = 0, curhumid = 0;

	while(1)
	{
		curtemp = get_temperature_sensor();

		if(curtemp > 25)
		{
			act_waterpump_on();
			delay(1000);
			act_waterpump_off();
		}

		curhumid = get_humidity_sensor();

		if(curhumid > 75)
		{
			act_rgbled_on();
			act_fan_on();
			act_dcmotor_on();
			delay(1000);
			act_fan_off();
			act_dcmotor_off();
		}
		delay(3000);
	}
	return 0 ;
}

int wiringPicheck(void)
{
	if (wiringPiSetup () == -1)
	{
		fprintf(stdout, "Unable to start wiringPi: %s\n", strerror(errno));
		return 1 ;
	}
}

void Bpluspinmodeset(void)
{
	pinMode(LIGHTSEN_OUT, INPUT);
	pinMode (PUMP, OUTPUT);
	pinMode (FAN, OUTPUT);
	pinMode (DCMOTOR, OUTPUT);
	pinMode(RGBLEDPOWER, OUTPUT);
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE,OUTPUT);
}

int get_light()
{
	// sets up the wiringPi library
	if (wiringPiSetup () < 0) 
	{
		fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
		return 1;
	}

	if(digitalRead(LIGHTSEN_OUT))	//day
		return 1;
	else //night
		return 0;

}

int get_temperature_sensor()
{
	int received_temp;
	DHTPIN = 11;
	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;

	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	while (read_dht22_dat_temp() == 0)
	{
		delay(500); // wait 1sec to refresh
	}

	received_temp = ret_temp;
	printf("온도 = %d\n", received_temp);

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
        float t, h;

        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

		ret_humid = (int)h;
		ret_temp = (int)t;
		//printf("습도 = %.2f %% 기온 = %.2f *C \n", h, t );
		//printf("(int)습도 = %d (int)기온 = %d\n", ret_humid, ret_temp);
	return ret_temp;
  }
  else
  {
    //printf("Data not good, skip\n");
    return 0;
  }
}

int get_humidity_sensor()
{
	int received_humid;
	DHTPIN = 11;

	if (wiringPiSetup() == -1)
		exit(EXIT_FAILURE) ;

	if (setuid(getuid()) < 0)
	{
		perror("Dropping privileges failed\n");
		exit(EXIT_FAILURE);
	}

	while (read_dht22_dat_humid() == 0) 
	{
		delay(500); // wait 1sec to refresh
	}

	received_humid = ret_humid;
	printf("습도 = %d\n", received_humid);

	return received_humid;
}

int read_dht22_dat_humid()
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
	for ( i=0; i< MAXTIMINGS; i++) 
	{
		counter = 0;
		while (sizecvt(digitalRead(DHTPIN)) == laststate) 
		{
			counter++;
			delayMicroseconds(1);
			if (counter == 255) {
			break;
		}
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) 
	{
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
        float t, h;

        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

		ret_humid = (int)h;
		ret_temp = (int)t;
		//printf("Humidity = %.2f %% Temperature = %.2f *C \n", h, t );
		//printf("Humidity = %d Temperature = %d\n", ret_humid, ret_temp);

    	return ret_humid;
	}
	else
	{
		//printf("Data not good, skip\n");
		return 0;
	}
}

void act_waterpump_on()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (PUMP, OUTPUT);
	digitalWrite (PUMP, 1) ; // On
}

void act_waterpump_off()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (PUMP, OUTPUT);
	digitalWrite (PUMP, 0) ; // On
}

void act_fan_on()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (FAN, OUTPUT);
	digitalWrite (FAN, 1) ; // On
}

void act_fan_off()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (FAN, OUTPUT);
	digitalWrite (FAN, 0) ; // On

}
void act_dcmotor_on()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (DCMOTOR, OUTPUT);
	digitalWrite(DCMOTOR, 1); //On 
}

void act_dcmotor_off()
{
	if(wiringPicheck()) printf("Fail");
	pinMode (DCMOTOR, OUTPUT);
	digitalWrite(DCMOTOR, 0); //On
}

void act_rgbled_on()
{
	signal(SIGINT, (void*)sig_handler);

	if(wiringPicheck()) printf("Fail");
	pinMode(RGBLEDPOWER, OUTPUT);
	pinMode(RED, OUTPUT);
	pinMode(GREEN, OUTPUT);
	pinMode(BLUE,OUTPUT);
	digitalWrite(RGBLEDPOWER, 0); //PWR on
	digitalWrite(RED, 1); //PWR on
	digitalWrite(BLUE, 0);
	digitalWrite(GREEN, 0);
}

void act_rgbled_off()
{
	if(wiringPicheck()) printf("Fail");
	digitalWrite(RGBLEDPOWER, 0); //PWR on
	digitalWrite(RED, 0);
	digitalWrite(BLUE, 0);
	digitalWrite(GREEN, 0);
}

void sig_handler(int signo)
{
	act_rgbled_off();
	act_dcmotor_off();
	act_fan_off();
	act_waterpump_off();
	exit(0);
}
