#pragma once
#include "dashboard.h"

void view_rebuild(Dashboard* d);
void view_sync(Dashboard* d);
void view_show_page(Dashboard* d, int idx);
const char* view_default_layout();
