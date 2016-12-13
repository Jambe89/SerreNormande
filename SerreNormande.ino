
//Cablage
//DHT sur D3
//SCL sur D1
//SDA sur D2
//Relais pompe sur D5
//


/////////////BIBLIOTHEQUE/////////////
#include <ThingSpeak.h>
#include <Wire.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <SFE_BMP180.h>
#include <LiquidCrystal_I2C.h>
///////////////////////////////////////

/////////////Pré-proc///////////////
#define ALTITUDE 34.0 // Altitude du lieu du BMP180, pour correction de la pression absolue
///////////////////////////////////////

/////////////Trame HTML fournie par ThingSpeak/////////////
const String trameTmpDht = "<p><iframe width=\"450\" height=\"260\" style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/171244/charts/1?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=120&title=Temp%C3%A9rature+1&type=spline&yaxis=+%C2%B0C\"></iframe></p>";
const String trameHumDht = "<p><iframe width=\"450\" height=\"260\" style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/171244/charts/2?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=120&title=Humidit%C3%A9&type=spline&xaxis=+&yaxis=%25\"></iframe></p>";
const String trameTmpBmp = "<p><iframe width=\"450\" height=\"260\" style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/171244/charts/4?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=120&round=1&title=Temp%C3%A9rature+2&type=spline&xaxis=+&yaxis=+%C2%B0C\"></iframe></p>";
const String tramePresBmp = "<p><iframe width=\"450\" height=\"260\" style=\"border: 1px solid #cccccc;\" src=\"https://thingspeak.com/channels/171244/charts/3?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=120&round=1&title=Pression+atmosph%C3%A9rique&type=spline&xaxis=+&yaxis=hPa\"></iframe></p>";
///////////////////////////////////////

/////////////PARAMETRE WIFI/////////////
//const char* ssid = "Bbox-3B6A6A75"; //Froberville
//const char* password = "66CED4AF5CA6133341E1AED23427EC";

const char* ssid = "Bbox-2F0B0C93"; //Vaucottes
const char* password = "66A217A35CEC43334F23711533172C";
///////////////////////////////////////

/////////////PARAMETRE THINGSPEAK/////////////
const unsigned long myChannelNumber = 171244;
const char * myWriteAPIKey = "ITIGGYD7I3OUBZ20";
///////////////////////////////////////

/////////////VARIABLES/////////////
double Temp /*,P*/, p0; //Variable utile pour le BMP180
float Hum, Tmp; //Variable utile pour le DHT22

bool EtatPompe = 0; //Etat de la pompe d'arrossage

const int  RelaiPompe = D5; //pin de commande du relais de la pompe d'arrossage


String reponse = ""; //String pour reponse html

uint8_t coeur[8] = {0x0, 0xa, 0x1f, 0x1f, 0xe, 0x4, 0x0}; //Logo coeur pour indication activité programme
bool bcoeur = 0;
//"Timers" pour l'utilisation de millis et cadencement des actions
unsigned long tempsActuel = 0; //
unsigned long tempsEcoule = 0; // compteur mise à jour bdd
unsigned long tempsEcoule1 = 0; // compteur mesure
unsigned long tempsEcoule2 = 0; //
unsigned long tempsAllumagePompe = 0; // sauvegarde moment d'allummage de la pompe


///////////////////////////////////////

/////////////CONSTRUCTEUR DE CLASSE/////////////
DHT dht(D3, DHT22);
SFE_BMP180 bmp;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD address to 0x27 for a 16 chars and 2 line display
WiFiServer server(80);
WiFiClient  client;
///////////////////////////////////////

void setup()
{
  //Serial.begin(115200);
  pinMode(RelaiPompe, OUTPUT);
  pinMode(D6, INPUT);
  digitalWrite(RelaiPompe, HIGH);

  dht.begin();
  server.begin();
  bmp.begin();
  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, coeur);

  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    lcd.print(".");
  }

  //Serial.println("");
  //Serial.println("WiFi connected");

  ThingSpeak.begin(client);
}

////////////Fin de setup ////////////////////////////////////////

void loop()
{
  if (WiFi.status() != WL_CONNECTED) //Verification de l'état du Wifi
  {
    WiFi.reconnect(); //Reconnection si Wifi perdu
    while (WiFi.status() != WL_CONNECTED) {
      delay(250); //Attente de la reprise du wifi
      //lcd.print("."); //Partie à améliorer
      //Si le WiFi ne se reconnecte jamais, perte de l'affichage sur LCD
    }
  }


  tempsActuel = millis();

  //++++++++++++++Gestion client++++++++++++++++++++//
  WiFiClient client = server.available();
  if (client)
  {
    String req = client.readStringUntil('\r');
    client.flush();

    if (req.indexOf("/?pin=ON1") != -1)
    {

      EtatPompe = !EtatPompe;

      reponse = reponseClient(Tmp, Temp, Hum, p0, EtatPompe, trameTmpDht, trameTmpBmp, trameHumDht, tramePresBmp);
      client.print(reponse);
      digitalWrite(RelaiPompe, !EtatPompe);
      if (EtatPompe)
      { tempsAllumagePompe = millis();
        //Serial.println("pompe allumée");
      }


    }
    else
    {
      //Serial.println("no request display home");
      reponse = reponseClient(Tmp, Temp, Hum, p0, EtatPompe, trameTmpDht, trameTmpBmp, trameHumDht, tramePresBmp);
      client.print(reponse);
    }


    client.flush();


  }

  //++++++++++++++Gestion client++++++++++++++++++++//




  if (tempsActuel - tempsEcoule >= 900000UL) //900000UL
  {
    tempsEcoule = tempsActuel;
    envoieThingSpeak(Tmp, Hum, p0, Temp); //Ecriture dans BDD Thingspeak
  }

  if (tempsActuel - tempsEcoule1 >= 5000UL)
  {
    tempsEcoule1 = tempsActuel;
    mesurebmp(&Temp, &p0);
    mesuredht(&Tmp, &Hum);
    //affichagebmp(); //Affichage moniteur serie pr debug
    //affichagedht(); //Affichage moniteur serie pr debug
    //Serial.println(EtatPompe);
  }



  if (tempsActuel - tempsEcoule2 >= 500UL) //900000UL
  {
    tempsEcoule2 = tempsActuel;
    bcoeur = !bcoeur;
    if (bcoeur)
    {
      lcd.setCursor(15, 0);
      lcd.write(0);
    }
    else
    {
      lcd.setCursor(15, 0);
      lcd.print(" ");
    }

  }

  if (EtatPompe)
  {
    if (tempsActuel >= tempsAllumagePompe + 30000UL)
    {
      EtatPompe = 0;
      digitalWrite(RelaiPompe, !EtatPompe);
    }
  }

  /////
  lcd.setCursor(0, 0);
  lcd.print("T1:");
  lcd.print(Tmp);
  lcd.setCursor(7, 0);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(10, 0);
  lcd.print("H:");
  lcd.print((int)Hum);
  lcd.setCursor(14, 0);
  lcd.print((char)37);

  lcd.setCursor(0, 1);
  lcd.print("T2:");
  lcd.print(Temp);
  lcd.setCursor(7, 1);
  lcd.print((char)223);
  lcd.print("C");

  lcd.setCursor(10, 1);
  lcd.print("P:");
  lcd.print(p0);
}
////////////Fin de loop ////////////////////////////////////////


/////////////////////////Fonctions/////////////////////////////

////////Mesure DHT22/////
void mesuredht(float *pTmp, float *pHum) // Fonction pour lire les valeurs du DHT22
{
  *pHum = dht.readHumidity();
  *pTmp = dht.readTemperature();
}
/////Fin/////

////////Mesure BMP180/////
void mesurebmp(double *T, double *P) // Fonction pour lire les valeurs du BMP180
{
  char status;
  double Pabs, Te, Psea;
  //Pabs = pression absolue (lecture directe du capteur)
  //Te = Température du capteur
  //Psea = Pression au niveau de la mer (standard météo)

  status = bmp.startTemperature(); //Partie de code tiré directement de l'exemple fournis avec la bibliotheque SFE_BMP180
  if (status != 0)
  { delay(status);

    status = bmp.getTemperature(Te); //Mesure de la température du BMP, necessaire pour corriger la lecture de la pression
    if (status != 0)
    { status = bmp.startPressure(3);
      if (status != 0)
      {
        delay(status);

        status = bmp.getPressure(Pabs, Te);
        if (status != 0)
        { Psea = bmp.sealevel(Pabs, ALTITUDE); // Calcul de la pression atmosphérique au niveau de la mer
        }
      }
    }
  }

  *T = Te - 2.00; //Offset de la température du BMP180
  *P = Psea;
  return;
}
/////Fin/////

////////ecriture sur BDD/////
void envoieThingSpeak(float Tdht, float Hdht, double Pbmp, double Tbmp)
{

  float fTbmp = Tbmp;
  float fPbmp = Pbmp;

  ThingSpeak.setField(1, Tdht);
  ThingSpeak.setField(2, Hdht);
  ThingSpeak.setField(3, fPbmp);
  ThingSpeak.setField(4, fTbmp);
  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  //Serial.println("envoi fait");
}
/////Fin/////

////////ecriture serie BMP180/////
void affichagebmp() //Fonction d'affichage sur voie serie, utile pour debug pendant la réalisation du projet
{
  Serial.println("Value from BMP");
  Serial.print("temperature: "); Serial.print(Temp, 2); Serial.println(" deg C, ");
  //Serial.print("absolute pressure: ");Serial.print(P,2);Serial.println(" mb, ");
  Serial.print("relative (sea-level) pressure: "); Serial.print(p0, 2); Serial.println(" hPa, ");
  Serial.println("");
}
/////Fin/////

////////ecriture serie DHT22/////
void affichagedht() //Fonction d'affichage sur voie serie, utile pour debug pendant la réalisation du projet
{
  Serial.println("Value from DHT");
  Serial.print("Temperature: "); Serial.print(Tmp); Serial.println(" *C");
  Serial.print("Humidity: "); Serial.print(Hum); Serial.println(" %\t");
  Serial.println("");
}
/////Fin/////

////////Generation de page HTML/////
String reponseClient(float t1, double t2, float h1, double pres, bool etat, String trame1, String trame2, String trame3, String trame4) //Fonction de création de la page HTML
{
  String retour;
  retour += "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html><html><head><meta charset=\"utf-8\" /><title>Serre 2.0</title></head><body bgcolor=\"#E6E6E6\"><h1>Tableau de bord</h1><h3> Derniers relevés:</h3><table border=\"1\" width=\"450\"><tr><th>Capteur</th><th width=\"40%\">Valeur</th></tr><tr><td align=\"center\">Température 1</td><td align=\"center\">";
  retour += t1;
  retour += " °C</td></tr><td align=\"center\">Température 2</td><td align=\"center\">";
  retour += t2;
  retour += "°C</td><tr><td align=\"center\">Humidité</td><td align=\"center\">";
  retour += h1;
  retour += "%</td></tr><td align=\"center\">Pression atmosphérique</td><td align=\"center\">";
  retour += pres;
  retour += " hPa</td><tr></tr></table><h3> Materiel:</h3><table border=\"1\" width=\"450\"><tr><th width=\"33%\">Appareil</th><th width=\"34%\">Commande</th><th width=\"33%\">Etat</th></tr><tr><td align=\"center\">Pompe</td><td align=\"center\"><a href=\"?pin=ON1\"><button>ON/OFF</button></a></td><td align=\"center\">";
  retour += (etat) ? "En Marche" : "A l'arret";
  retour += "</td></tr></table><h4>Historique:</h4>";
  retour += trame1;
  retour += trame2;
  retour += trame3;
  retour += trame4;
  retour += "</body></html>";

  return retour;
}
/////Fin/////


