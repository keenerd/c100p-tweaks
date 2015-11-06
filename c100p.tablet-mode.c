#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>

// gcc -O2 -lm c100p.tablet-mode.c -o c100p.tablet-mode && strip c100p.tablet-mode
// requires xosd_cat, xinput and terminus-font

#define KEYBOARD_LABEL     "cros_ec"
#define TOUCHSCREEN_LABEL  "Elan Touchscreen"
#define TRACKPAD_LABEL     "Elan Touchpad"

// settings
#define TICKS_PER_SECOND  5
#define IIR_COEF          0.2
#define TABLET_ANGLE      -10
#define LAPTOP_ANGLE      30
#define SKEW_LIMIT        100
// skew is cm/s/s

#define ACCEL_PATH "/sys/devices/ff110000.spi/spi_master/spi0/spi0.0/cros-ec-accel.0/iio:device0/in_accel_%s_%s_input"
/*  x/y/z base/lid
    z is perpendicular to the plane
    y is parallel to the hinge (useless here)
    x is the other one
    units is cm/s/s
*/
#define ACCEL_BUF 20
#define XINPUT_CMD "xinput -%s \"%s\""
#define XOSD_CMD "osd_cat -c skyblue -s 5 -l 1 -p middle -A center -d %i -f -*-terminus-*-r-*-*-80-*-*-*-c-*-*-*"

// globals
FILE* accel_x_base;
FILE* accel_z_base;
FILE* accel_x_lid;
FILE* accel_z_lid;

FILE* accel_file(char* axis, char* device)
{
    FILE* fp;
    char* path;
    asprintf(&path, ACCEL_PATH, axis, device);
    fp = fopen(path, "r");
    free(path);
    return fp;
}

void accel_setup()
{
    accel_x_base = accel_file("x", "base");
    accel_z_base = accel_file("z", "base");
    accel_x_lid  = accel_file("x", "lid");
    accel_z_lid  = accel_file("z", "lid");
}

void accel_cleanup()
{
    fclose(accel_x_base);
    fclose(accel_z_base);
    fclose(accel_x_lid);
    fclose(accel_z_lid);
}

double complex accel_read(FILE* a_x, FILE* a_z)
{
    char buf[ACCEL_BUF];
    double complex vector;
    fread(buf, 1, ACCEL_BUF, a_x);
    vector = atof(buf);
    fread(buf, 1, ACCEL_BUF, a_z);
    vector = vector + atof(buf) * I;
    rewind(a_x);
    rewind(a_z);
    return vector; 
}

double cdot(double complex a, double complex b)
{
    return creal(a) * creal(b) + cimag(a) * cimag(b);
}

void xinput(char* device, char* enable)
{
    char* cmd;
    asprintf(&cmd, XINPUT_CMD, enable, device);
    system(cmd);
    free(cmd);
}

void xosd(char* message, int timeout)
{
    char* cmd;
    FILE* osd_input;
    asprintf(&cmd, XOSD_CMD, timeout);
    osd_input = popen(cmd, "w");
    fprintf(osd_input, message);
    pclose(osd_input);
}

void wait_for(double hinge_angle)
// +angle -> greater than, -angle -> less than
{
    double complex v_base;
    double complex v_lid;
    double m_base, m_lid, angle;
    double sub_iir = 1.0 - IIR_COEF;
    double angle_avg = -1000;  // un-init flag
    int us_tick = 1000000 / TICKS_PER_SECOND;
    while (1)
    {
        usleep(us_tick);
        v_base = accel_read(accel_x_base, accel_z_base);
        v_lid  = accel_read(accel_x_lid, accel_z_lid);
        m_base = cabs(v_base);
        m_lid  = cabs(v_lid);
        // ignore if axis might be ambiguous
        if (m_base < SKEW_LIMIT)
            {continue;}
        if (m_lid  < SKEW_LIMIT)
            {continue;}
        // ignore if accel > gravity
        if (m_base > (1000 + SKEW_LIMIT))
            {continue;}
        if (m_lid  > (1000 + SKEW_LIMIT))
            {continue;}
        // normalize
        v_base = v_base / m_base;
        v_lid  = v_lid  / m_lid;
        angle = acos(cdot(v_base, v_lid)) * 180 / M_PI;
        if (angle_avg == -1000)
            {angle_avg = angle;}
        angle_avg = IIR_COEF * angle + sub_iir * angle_avg;
        // thresholds
        if (hinge_angle > 0 && angle_avg > hinge_angle)
            {break;}
        if (hinge_angle < 0 && angle_avg < -hinge_angle)
            {break;}
    }
}

int main()
{
    accel_setup();
    xosd("waiting for tablet mode", 1);
    wait_for(TABLET_ANGLE);
    xinput(TRACKPAD_LABEL, "disable");
    xinput(KEYBOARD_LABEL, "disable");
    xosd("entering tablet mode", 1);
    wait_for(LAPTOP_ANGLE);
    xinput(KEYBOARD_LABEL, "enable");
    xinput(TRACKPAD_LABEL, "enable");
    accel_cleanup();
    xosd("leaving tablet mode", 5);
    sleep(1);
    return 0;
}
