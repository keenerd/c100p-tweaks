#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <syslog.h>
#ifdef USE_SYSTEMD
#include <glib.h>
#endif

#include <fcntl.h>
#include <stdbool.h>

// non systemd build
//gcc -O2 -lm c100p.tablet-mode.c -o c100p.tablet-mode && strip c100p.tablet-mode

// systemd build
//gcc -O2 -DUSE_SYSTEMD -DXAUTHDIR=$HOME -lm c100p.tablet-mode.c -o c100p.tablet-mode `pkg-config --cflags --libs glib-2.0` && strip c100p.tablet-mode

// requires xosd_cat, xinput and terminus-font

#define STRINGIZE(x) #x
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x)

#define KEYBOARD_LABEL     "cros_ec"
#define TOUCHSCREEN_LABEL  "Elan Touchscreen"
#define TRACKPAD_LABEL     "Elan Touchpad"

// settings
#define TICKS_PER_SECOND  5
#define IIR_COEF          0.2
#define TABLET_ANGLE      -10
#define LAPTOP_ANGLE      30
#define SKEW_LIMIT 100

typedef struct config_t {
  int ticks_per_second;
  float iir_coef ;
  int tablet_angle;
  int laptop_angle;
  int skew_limit;
} tabletcfg;

tabletcfg t_cfg;

void init() {
  t_cfg.ticks_per_second = TICKS_PER_SECOND;
  t_cfg.iir_coef = IIR_COEF;
  t_cfg.tablet_angle = TABLET_ANGLE;
  t_cfg.laptop_angle = LAPTOP_ANGLE;
  t_cfg.skew_limit = SKEW_LIMIT;
}

// skew is cm/s/s
#define ACCEL_PATH "/sys/devices/ff110000.spi/spi_master/spi0/spi0.0/cros-ec-accel.0/iio:device0/in_accel_%s_%s_input"
/*  x/y/z base/lid
    z is perpendicular to the plane
    y is parallel to the hinge (useless here)
    x is the other one
    units is cm/s/s
*/
#define ACCEL_BUF 20

#ifdef USE_SYSTEMD
  #define XINPUT_CMD "export DISPLAY=:0.0 && export XAUTHORITY=%s/.Xauthority && xinput -%s \"%s\""
#else
  #define XINPUT_CMD "xinput -%s \"%s\""
  #define XOSD_CMD "osd_cat -c skyblue -s 5 -l 1 -p middle -A center -d %i -f -*-terminus-*-r-*-*-80-*-*-*-c-*-*-*"
#endif

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
 #ifdef USE_SYSTEMD
   asprintf(&cmd, XINPUT_CMD, STRINGIZE_VALUE_OF(XAUTHDIR), enable, device);
 #else
   asprintf(&cmd, XINPUT_CMD, enable, device);
#endif
   system(cmd);
  free(cmd);
}

void xosd(char* message, int timeout)
{
  #ifndef USE_SYSTEMD
    char* cmd;
    FILE* osd_input;
    asprintf(&cmd, XOSD_CMD, timeout);
    osd_input = popen(cmd, "w");
    fprintf(osd_input, message);
    pclose(osd_input);
    #endif
}

void wait_for(double hinge_angle)
// +angle -> greater than, -angle -> less than
{
    double complex v_base;
    double complex v_lid;
    double m_base, m_lid, angle;
    double sub_iir = 1.0 - t_cfg.iir_coef;
    double angle_avg = -1000;  // un-init flag
    int us_tick = 1000000 / t_cfg.ticks_per_second;
    while (1)
    {
        usleep(us_tick);
        v_base = accel_read(accel_x_base, accel_z_base);
        v_lid  = accel_read(accel_x_lid, accel_z_lid);
        m_base = cabs(v_base);
        m_lid  = cabs(v_lid);
        // ignore if axis might be ambiguous
        if (m_base < t_cfg.skew_limit)
            {continue;}
        if (m_lid  < t_cfg.skew_limit)
            {continue;}
        // ignore if accel > gravity
        if (m_base > (1000 + t_cfg.skew_limit))
            {continue;}
        if (m_lid  > (1000 + t_cfg.skew_limit))
            {continue;}
        // normalize
        v_base = v_base / m_base;
        v_lid  = v_lid  / m_lid;
        angle = acos(cdot(v_base, v_lid)) * 180 / M_PI;
        if (angle_avg == -1000)
            {angle_avg = angle;}
        angle_avg = t_cfg.iir_coef * angle + sub_iir * angle_avg;
        // thresholds
        if (hinge_angle > 0 && angle_avg > hinge_angle)
            {break;}
        if (hinge_angle < 0 && angle_avg < -hinge_angle)
            {break;}
    }
}

#ifdef USE_SYSTEMD
bool read_config(char* filename) {
    GKeyFile* gkf;
    gkf = g_key_file_new();
    if(!g_key_file_load_from_file(gkf, filename, G_KEY_FILE_NONE, NULL)) {
      return false;;
    }
    GError *error;
    t_cfg.ticks_per_second = g_key_file_get_integer(gkf, "TabletMode", "TicksPerSecond", &error);
    syslog(LOG_DEBUG, "TicksPerSecond: %d", t_cfg.ticks_per_second);
    char* iir_coef_c = g_key_file_get_value(gkf, "TabletMode", "IirCoeff", &error);
    t_cfg.iir_coef = atof(iir_coef_c); 
    syslog(LOG_DEBUG, "IirCoef: %f", t_cfg.iir_coef);
    t_cfg.laptop_angle = g_key_file_get_integer(gkf, "TabletMode", "LaptopAngle", &error);
    syslog(LOG_DEBUG, "LaptopAngle: %d", t_cfg.laptop_angle);
    t_cfg.skew_limit = g_key_file_get_integer(gkf, "TabletMode", "SkewLimit", &error);
    syslog(LOG_DEBUG, "SkewLimit: %d", t_cfg.skew_limit);
    t_cfg.tablet_angle = g_key_file_get_integer(gkf, "TabletMode", "TabletAngle", &error);
    syslog(LOG_DEBUG, "TabletAngle: %d", t_cfg.tablet_angle);
    
    g_key_file_free(gkf);
    return true;
}

void handle_signal(int sig) {
  if(sig == SIGINT) {
    syslog(LOG_NOTICE, "Stopping daemon");
    signal(SIGINT, SIG_DFL);
  } else if(sig == SIGHUP) {
    syslog(LOG_NOTICE, "Restarting daemon");
    if(!read_config("/home/falken/github/c100p-tweaks/c100p.conf")) {
      syslog(LOG_ERR, "could not open config file");
      exit(EXIT_FAILURE);
    }
  } 
}
#endif

int main()
{
  init();
#ifdef USE_SYSTEMD
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog("c100p.tablet-mode", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    signal(SIGINT, handle_signal);
    signal(SIGHUP, handle_signal);

    // Read config
    syslog(LOG_DEBUG, "Read config");
    if(!read_config("/home/falken/github/c100p-tweaks/c100p.conf")) {
      syslog(LOG_ERR, "could not open config file");
      return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "deamon started");
 #endif
    accel_setup();
   
    while(1) {
      xosd("waiting for tablet mode", 1);
      wait_for(t_cfg.tablet_angle);
      xinput(TRACKPAD_LABEL, "disable");
      xinput(KEYBOARD_LABEL, "disable");
      syslog(LOG_INFO, "entering tablet mode");
      xosd("entering tablet mode", 1);
      wait_for(t_cfg.laptop_angle);
      xinput(KEYBOARD_LABEL, "enable");
      xinput(TRACKPAD_LABEL, "enable");
      syslog(LOG_INFO, "leaving tablet mode");
      xosd("leaving tablet mode", 1);
    }
     accel_cleanup();

 #ifdef USE_SYSTEMD
     closelog();
 #endif
    return 0;
}
