#pragma once
#include "dashboard.h"
void nav_begin();
void nav_tick(Dashboard* d);                 // lit l'encodeur, change de page si besoin
void nav_goto_dir(Dashboard* d, int delta);  // delta>0 suivant, <0 precedent
