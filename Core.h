#include <time.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <SoftwareSerial.h>

#define FIREBASE_HOST "your-project.firebaseio.com"
#define FIREBASE_AUTH "key" //database password

#define WIFI_SSID "U+village"
#define WIFI_PASSWORD "lguplus100"

#define TIMEZONE 9

#define DB_PATH "/ENV-DATA-LOG" // Database path

enum class WorkStatus
{
	Success,
	FailedToSensor,
	FailedToFirebase,
	Unknown
};

class Core
{
private:
	DHT dht;
	
	float humidity;
	float temperature;

	long pmcf10;
	long pmcf25;
	long pmcf100;
	long pmat10;
	long pmat25;
	long pmat100;

	SoftwareSerial mySerial;
    
public:
	Core()
		: dht()
		, humidity(0)
		, temperature(0)
		, pmcf10(0)
		, pmcf25(0)
		, pmcf100(0)
		, pmat10(0)
		, pmat25(0)
		, pmat100(0)
		, mySerial(D4, D5)
	{
	}

	void setup()
	{
		Serial.begin(115200);
		Serial.println("-----Start-----");

		// WiFi
		WiFi.mode(WIFI_STA);
		Serial.print("[*] connecting to ");
		Serial.print(WIFI_SSID);  
  
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			Serial.print(".");
		}
  
		Serial.print("\n[*] local IP: ");
		Serial.println(WiFi.localIP());
  
		// Time
		configTime(TIMEZONE * 3600, 0, "pool.ntp.org", "time.nist.gov"); 
		Serial.println("[*] waiting for time");
		while (!time(nullptr))
		{
			Serial.print(".");
			delay(1000);
		}
		
		delay(1500);
		Serial.println("[*] now: " + getNow());

		// Firebases
		Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
		Serial.println("\n[*] Firebase begin.");
		
		//DHT11
		dht.setup(D6);

		// PMS3003
		mySerial.begin(9600);
	}

	DHT &getDHT()
	{
		return dht;
	}

	SoftwareSerial &getSerial()
	{
		return mySerial;
	}

	String getNow()
	{
		time_t t = time(nullptr);
		struct tm *nt = localtime(&t);
		String tmp = "";
  
		tmp += String(nt->tm_year + 1900);
		tmp += "-";
		tmp += String(nt->tm_mon + 1);
		tmp += "-";
		tmp += String(nt->tm_mday);
		tmp += " ";
		tmp += String(nt->tm_hour);
		tmp += ":";
		tmp += String(nt->tm_min);
		tmp += ":";
		tmp += String(nt->tm_sec);
  
		return tmp;
	}

	WorkStatus updateAll()
	{
		WorkStatus retn;

		retn = updateDHT11();
		if (retn == WorkStatus::Unknown || retn == WorkStatus::FailedToSensor)
		{
			Serial.println("[-] failed to DHT11 sensor recognition.");
			return retn;
		}

		delay(1000);

		retn = updatePMS3003();
		if (retn == WorkStatus::Unknown || retn == WorkStatus::FailedToSensor)
		{
			Serial.println("[-] failed to PMS3003 sensor recognition.");
			return retn;
		}

		StaticJsonBuffer<200> jsonBuffer;
		JsonObject &root = jsonBuffer.createObject();
		root["humidity"] = humidity;
		root["temperature_C"] = temperature;
		root["PM1_0"] = pmat10;
		root["PM2_5"] = pmat25;
		root["PM10"] = pmat100;
		root["time"] = getNow();
		
		String pathName = Firebase.push(DB_PATH, root);

		// handle error
		if (Firebase.failed())
		{
			Serial.print("[-] failed to push.");
    
			if (Firebase.error())
			{
				Serial.print(" error: ");
				Serial.print(Firebase.error());
			}
  
			Serial.println();
			return WorkStatus::FailedToFirebase;
		}

		Serial.print("[+] ");
		Serial.print(getNow());
		Serial.print("\t-\tpushed: ");
		Serial.print(DB_PATH);
		Serial.println("/");
	}

	WorkStatus updateDHT11()
	{
		delay(dht.getMinimumSamplingPeriod() + 500);

		humidity = dht.getHumidity();
		temperature = dht.getTemperature();

		String dhtStatus = dht.getStatusString();
		if (!dhtStatus.equals("OK"))
		{
			return WorkStatus::FailedToSensor;
		}
		
		return WorkStatus::Success;
	}

	WorkStatus updatePMS3003()
	{
		WorkStatus retn = WorkStatus::Unknown;

		int count = 0;
		unsigned char c;
		unsigned char high;

		while (mySerial.available())
		{
			c = mySerial.read();

			if((count == 0 && c != 0x42) || (count == 1 && c != 0x4d))
			{
				//Serial.println("[-] check failed");
				retn = WorkStatus::FailedToSensor;
				break;
			}

			if (count > 15)
			{
				//Serial.println("[+] complete");
				retn = WorkStatus::Success;
				break;
			}
			else if (count == 4 || count == 6 || count == 8 || count == 10 || count == 12 || count == 14)
			{
				high = c;
			}
			else if (count == 5)
			{
				pmcf10 = 256 * high + c;
				//Serial.print("CF=1, PM1.0=");
				//Serial.print(pmcf10);
				//Serial.println(" ug/m3");
			}
			else if(count == 7)
			{
				pmcf25 = 256 * high + c;
				//Serial.print("CF=1, PM2.5=");
				//Serial.print(pmcf25);
				//Serial.println(" ug/m3");
			}
			else if(count == 9)
			{
				pmcf100 = 256 * high + c;
				//Serial.print("CF=1, PM10=");
				//Serial.print(pmcf100);
				//Serial.println(" ug/m3");
			}
			else if(count == 11)
			{
				pmat10 = 256 * high + c;
				//.print("atmosphere, PM1.0=");
				//Serial.print(pmat10);
				//Serial.println(" ug/m3");
			}
			else if(count == 13)
			{
				pmat25 = 256 * high + c;
				//Serial.print("atmosphere, PM2.5=");
				//Serial.print(pmat25);
				//Serial.println(" ug/m3");
			}
			else if(count == 15)
			{
				pmat100 = 256 * high + c;
				//Serial.print("atmosphere, PM10=");
				//Serial.print(pmat100);
				//Serial.println(" ug/m3");
			}

			count++;
		}

		while (mySerial.available())
		{
			mySerial.read();
		}

		// delay
		delay(500);

		return retn;
	}
};