#pragma once
#include <Arduino.h>

void   netBegin();            // Wi-Fi (STA, sinon point d'acces de configuration) + serveur web + OTA
void   netLoop();             // a appeler dans loop()
String netIp();               // adresse IP a afficher
bool   netIsAp();             // true si on est en mode point d'acces de configuration
bool   netConnected();        // true si connecte a un routeur
