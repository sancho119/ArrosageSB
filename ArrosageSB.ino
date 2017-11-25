
///////////////////////////////////////////////////////////////////////////////
//                            BIBILOTHEQUES                                  //
///////////////////////////////////////////////////////////////////////////////

#include <EEPROM.h>
#include <Wire.h>
#include <DS3231.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3F, 16, 2); // à chercher

DS3231 rtc;
bool Century = false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;

byte year, month, date, DoW, hour, minute, second;

///////////////////////////////////////////////////////////////////////////////
//                            Définition des variables
///////////////////////////////////////////////////////////////////////////////

// Attribution des sorties

int relais[6] = {2, 4, 6, 8, 10, 12};

// *******************
//      Arrosage
// *******************

byte modeArrosage = 0;  // 0->OFF 1->AUTO 2->MANUEL

byte tempsArrosage[] = {30, 30, 30, 30, 30, 30}; // durée z[i] en minutes

byte heureDeb[] = {19, 19, 19, 19, 19, 19,};  // heure début horaires
byte minuteDeb[] = {28, 29, 30, 31, 32, 33};  // minute début horaires

bool start_z[] = {0, 0, 0, 0, 0, 0}; // bit démarrage zones

bool mem_etat_R[6]; // bit mémorisation état relais zone i

bool on_jour[] = {1, 1, 1, 1, 1, 1, 1}; // bit arrosage jour ON/OFF

byte cumulEtatRelais = 0;

// *******************
// adresse de stockage données en eeprom
// *******************

int addr_tempsArrosage = 0;
int addr_heureDeb = addr_tempsArrosage + sizeof(tempsArrosage);
int addr_minuteDeb = addr_heureDeb + sizeof(heureDeb);
int addr_start_z = addr_minuteDeb + sizeof(minuteDeb);
int addr_mem_etat_R = addr_start_z + sizeof(start_z);
int addr_point_on_jour = addr_mem_etat_R + sizeof(mem_etat_R);
int addr_on_jour = addr_point_on_jour + sizeof(point_on_jour);


// *******************
// Divers
// *******************

unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;


///////////////////////////////////////////////////////////////////////////////
//                            SETUP
///////////////////////////////////////////////////////////////////////////////

void setup() {

  Wire.begin();
  Serial.begin(9600);
  lcd.begin();
  lcd.backlight();

  // Initialisation des variables stockées en eeprom

  EEPROM.put(addr_tempsArrosage, tempsArrosage);
  EEPROM.put(addr_heureDeb, heureDeb);
  EEPROM.put(addr_minuteDeb, minuteDeb);
  EEPROM.put(addr_start_z, start_z);
  EEPROM.put(addr_mem_etat_R, mem_etat_R);
  EEPROM.put(addr_point_on_jour, point_on_jour);
  EEPROM.put(addr_on_jour, on_jour);

  // Lectures des variables stockées en eeprom pour utilisation

  EEPROM.get(addr_tempsArrosage, tempsArrosage);
  EEPROM.get(addr_heureDeb, heureDeb);
  EEPROM.get(addr_minuteDeb, minuteDeb);
  EEPROM.get(addr_on_jour, on_jour);

  //  rtc.set(0, 21, 15, 7, 5, 11, 17);

  for (int i = 0; i < 6; i++) {
    pinMode(relais[i], OUTPUT); // Configure la broche relais electrovanne en sortie
    digitalWrite(relais[i], LOW); // par défaut, la relais n'est pas collé
  }

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//                            Programme principal
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void loop() {

  unsigned long currentMillis = millis();
  unsigned long currentMillis2 = millis();

  String reception = "";
  if (Serial.available()) { // teste s'il reste des données en attente
    while (Serial.available()) { //tant que des données sont en attente
      char caractere = Serial.read(); // lecture
      reception += String(caractere);
      delay(10); //petite attente
      Serial.println(reception);
    }
//    reception.trim(); //on enlève le superflu en début et fin de chaine
//    reception = reception.substring(0, 1); // ne prend que les caractère de 0 à 1 (soit 2 caractères)

char reception0=reception.charAt(0);
char reception1=reception.charAt(1);
char reception2=reception.charAt(2);

    switch (reception0) {
      case '0':
        Serial.println ("Systeme en arret");
        modeArrosage = 0;
        break;

      case '1':
        Serial.println("Systeme en mode automatique");
        modeArrosage = 1;
        break;
      case '2':
        Serial.println("Systeme en mode manuel");
        modeArrosage = 2;
        for (int p=0;p>6;p++){
          if (reception1==p){
            start_z[p] = 1;
          }
        }
        for (int p=0;p>6;p++){
          if (reception2==p){
            start_z[p] = 0;
          }
        }
        break;
        default :
        Serial.println("Demande incorrect, arret du systeme !");
        modeArrosage = 0;
    }
  }

  ////////////////////////// Temporisation de 1 seconde /////////////////////////

  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;

    Lcd();

    Serial.print(rtc.getYear(), DEC);
    Serial.print('/');
    Serial.print(rtc.getMonth(Century), DEC);
    Serial.print('/');
    Serial.print(rtc.getDate(), DEC);
    Serial.print(" (");
    Serial.print(rtc.getDoW());
    Serial.print(") ");
    Serial.print(rtc.getHour(h12, PM), DEC);
    Serial.print(':');
    Serial.print(rtc.getMinute(), DEC);
    Serial.print(':');
    Serial.print(rtc.getSecond(), DEC);

    Serial.println();

    // Lancement arrosage auto
    switch (modeArrosage) {
      case 0 : // mode arrêt

        extinctionArrosage(); // Affichage à voir mode arrêt, si suppression séquence lecture sur moniteur série
        break;

      case 1 : // mode automatique

        for (int i = 0; i < 6; i++) {
          if ((rtc.getHour(h12, PM) == heureDeb[i] && rtc.getMinute() == minuteDeb[i] && on_jour[rtc.getDoW()] == 1)) {
            start_z[i] = 1; //// VOIR pour verrouiller les autres relais
            arrosageAuto();
          }
        }
        break;

      case 2 :// mode manuel
        // A VOIR
        Serial.println("Mode manuel");
        arrosageManuel();
        break;
    }
  }


  ////////////////////////// Fin Temporisation de 1 seconde/////////////////////////


  ////////////////////////// Temporisation de 1 minute /////////////////////////////

  if (currentMillis2 - previousMillis2 >= 1000) {
    previousMillis2 = currentMillis2;

    decompte_arrosage_auto();

  }

  ////////////////////////// Fin Temporisation de 1 minute/////////////////////////

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                           Arrosage
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void decompte_arrosage_auto() {

  // Décompte temps zone i
  for (int i = 0; i < 6; i++) {
    if ((start_z[i] == 1)) {
      Serial.print("temps d'arrosage restant sur zone ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.println(tempsArrosage[i]);
      tempsArrosage[i] -= 1;
      if (tempsArrosage[i] < 0) {
        tempsArrosage[i] = 0;
      }
    }
  }
}

void arrosageAuto() {  // Déclenchement ou arrêt zone i suivant horaires programmés

  for (int i = 0; i < 5; i++) {
    if ((start_z[i] == 1) && (tempsArrosage[i] > 0) && (mem_etat_R[0] == 0) && (mem_etat_R[1] == 0) && (mem_etat_R[2] == 0) && (mem_etat_R[3] == 0) && (mem_etat_R[4] == 0) && (mem_etat_R[5] == 0)) {
      digitalWrite(relais[i], HIGH);
      mem_etat_R[i] = 1;
      delay (1000);
    }
  }

  for (int i = 0; i < 5; i++) {
    if ((start_z[i] == 1) && (tempsArrosage[i] == 0)) {
      start_z[i] = 0;
      digitalWrite(relais[i], LOW);
      mem_etat_R[i] = 0;
      EEPROM.get(addr_tempsArrosage, tempsArrosage);
      delay (1000);
    }
  }
}

void arrosageManuel() {

  for (int i = 0; i < 6; i++) {
    cumulEtatRelais = 0;
    for (int c = 0; c < 6; i++) {
      cumulEtatRelais = cumulEtatRelais + mem_etat_R[c];
    }
    if ((start_z[i] == 1) && (mem_etat_R[i] == 0) && cumulEtatRelais == 0) {
      digitalWrite(relais[i], HIGH);
      mem_etat_R[i] = 1;
      cumulEtatRelais = 0;
      delay (1000);
    }
    else if ((start_z[i] == 1) && cumulEtatRelais > 1) {
      Serial.println("Trop de zones ouvertes : fermer les autres zones !");
      cumulEtatRelais = 0;
    }
    if ((start_z[i] == 0) && (mem_etat_R[i] == 1)) {
      digitalWrite(relais[i], LOW);
      mem_etat_R[i] = 0;
      cumulEtatRelais = 0;
      delay (1000);
    }
  }
}

void extinctionArrosage() {
  for (int i = 0; i < 6; i++) {
    start_z[i] = 0;
    digitalWrite(relais[i], LOW);
    mem_etat_R[i] = 0;
    EEPROM.get(addr_tempsArrosage, tempsArrosage);
    Serial.println("systeme en arret");
    delay (1000);
  }
}

void Lcd() {

  int second, minute, hour, date, month, year, temperature;
  second = rtc.getSecond();
  minute = rtc.getMinute();
  hour = rtc.getHour(h12, PM);
  date = rtc.getDate();
  month = rtc.getMonth(Century);
  year = rtc.getYear();

  lcd.setCursor(1, 1);

  if (rtc.getDoW() == 1)
  {
    lcd.print("Lu");
  }
  if (rtc.getDoW() == 2)
  {
    lcd.print("Ma");
  }
  if (rtc.getDoW() == 3)
  {
    lcd.print("Me");
  }
  if (rtc.getDoW() == 4)
  {
    lcd.print("Je");
  }
  if (rtc.getDoW() == 5)
  {
    lcd.print("Ve");
  }
  if (rtc.getDoW() == 6)
  {
    lcd.print("Sa");
  }
  if (rtc.getDoW() == 7)
  {
    lcd.print("Di");
  }
  lcd.print(" ");
  if (date < 10)
  {
    lcd.print("0");
  }
  lcd.print(date, DEC);
  lcd.print('/');
  if (month < 10)
  {
    lcd.print("0");
  }
  lcd.print(month, DEC);
  lcd.print('/');
  lcd.print("20");
  lcd.print(year, DEC);


  lcd.setCursor(4, 0);
  if (hour < 10)
  {
    lcd.print("0");
  }
  lcd.print(hour, DEC);
  lcd.print(':');
  if (minute < 10)
  {
    lcd.print("0");
  }
  lcd.print(minute, DEC);
  lcd.print(':');
  if (second < 10)
  {
    lcd.print("0");
  }
  lcd.print(second, DEC);
}
