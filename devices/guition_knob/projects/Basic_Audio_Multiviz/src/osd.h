#pragma once

void osd_init(int total_vizs);
void osd_show(int viz_index, const char* viz_name);
void osd_tick();  // appelée chaque frame, gère le fade
