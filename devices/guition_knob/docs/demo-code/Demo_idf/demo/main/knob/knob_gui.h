#ifndef __KNOB_GUI_H
#define __KNOB_GUI_H

#define REPORT_ID    1
#define DIAL_R       0xC8
#define DIAL_L       0x38
#define DIAL_PRESS   0x01
#define DIAL_RELEASE 0x00
#define DIAL_R_F     0x14
#define DIAL_L_F     0xEC

void knob_hid_init(void *param);
void surface_dial_report(uint8_t key);
void knob_hid_gui(void);

#endif