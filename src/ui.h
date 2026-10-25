#pragma once
#include <Arduino.h>

void uiBegin();                                   // ecran, tactile, LVGL, interface
void uiLoop();                                    // a appeler tres souvent depuis loop()
void uiRebuild();                                 // reconstruit la grille apres un changement de config
void uiApplyDisplay();                            // luminosite + inversion des couleurs
void uiToast(const char* msg, bool err = false);  // petit message en bas de l'ecran
void uiRenderNow();                               // force un rafraichissement (avant une operation bloquante)
