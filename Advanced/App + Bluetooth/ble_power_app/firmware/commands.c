/*
 * commands.c  -  commands typed into the app's "Send to ATmega" box
 *
 *   HELP              list the commands
 *   STATUS            firmware, settings, calibration, frequency, raw ADC levels
 *   RESET             energy total and time back to zero
 *   RESETMM           clear min / max power
 *   RATE,<ms>         report period, 250-10000 ms            (saved)
 *   AXIS,<W>          top of the app's graph, 1-100 W        (saved)
 *   DISP,<mode>       7-seg: AUTO, V, I, P, E or PF
 *   ZERO              with the AC source OFF: record the noise floor        (saved)
 *   CALV,<volts>      with AC on: true Vrms from a multimeter, e.g. CALV,13.00 (saved)
 *   CALI,<amps>       with a load: true Irms from a multimeter, e.g. CALI,0.264 (saved)
 *   CALCLR            back to design values (clears CALV, CALI, ZERO, RATE, AXIS)
 *   DOOM              DOOM mode on the OLED (keys w a s d q e f, EXIT to leave)
 *
 * Every command replies with OK,<cmd> or ERR,<cmd>,<reason>.
 */

#include <string.h>
#include <math.h>
#include "common.h"
#include "pgm_compat.h"
#include "monitor.h"
#include "settings.h"
#include "ble_report.h"
#include "sampler.h"
#include "display.h"
#include "util.h"

/* cmd and why are flash strings: ok(PSTR("RATE")) */
static void ok(const char *cmd)
{
	ble_send_P(PSTR("OK,"));
	ble_send_P(cmd);
	ble_send_P(PSTR("\n"));
}

static void err(const char *cmd, const char *why)
{
	ble_send_P(PSTR("ERR,"));
	ble_send_P(cmd);
	ble_send_P(PSTR(","));
	ble_send_P(why);
	ble_send_P(PSTR("\n"));
}

static uint32_t to_milli(float x)
{
	return (x <= 0) ? 0 : (uint32_t)(x * 1000.0f + 0.5f);
}

static void send_status(void)
{
	const float lsb = ADC_VREF / 1024.0f;
	uint16_t off = sampler_read_channel(ADC_CH_OFFSET);   /* 2.1Vdc offset net, PE3/ADC7 */
	uint16_t ipk = sampler_read_channel(ADC_CH_IPK);      /* current peak detector, PC3 */

	ble_send_P(PSTR("S,fw=" FW_VERSION ",rate="));
	ble_send_uint(settings.rate_ms);
	ble_send_P(PSTR(",axis="));
	ble_send_fixed(settings.axis_max_mW, 2);
	ble_send_P(PSTR(",calv="));
	ble_send_fixed(to_milli(settings.cal.cal_v), 3);
	ble_send_P(PSTR(",cali="));
	ble_send_fixed(to_milli(settings.cal.cal_i), 3);
	ble_send_P(PSTR(",zero="));
	ble_send_str((settings.cal.noise_vv > 0 || settings.cal.noise_ii > 0) ? "yes" : "no");
	ble_send_P(PSTR("\nS,f="));
	ble_send_fixed((uint32_t)mon.freq_dHz * 100UL, 1);
	ble_send_P(PSTR(",offset="));
	ble_send_fixed(to_milli(off * lsb), 3);
	ble_send_P(PSTR(",ipk="));
	ble_send_fixed(to_milli(ipk * lsb), 3);
	if (mon.have_last) {
		ble_send_P(PSTR(",vdc="));
		ble_send_fixed(to_milli(mon.last.mean_v * lsb), 3);
		ble_send_P(PSTR(",idc="));
		ble_send_fixed(to_milli(mon.last.mean_i * lsb), 3);
		ble_send_P(PSTR(",S="));
		ble_send_fixed(to_milli(mon.last.s), 2);
	}
	ble_send_P(PSTR("\n"));
}

void commands_handle(char *line)
{
	const char *arg;
	uint32_t v;

	str_upper(line);

	if ((arg = match_cmd(line, "HELP"))) {
		ble_send_P(PSTR("S,cmds=STATUS RESET RESETMM RATE,ms AXIS,W DISP,AUTO|V|I|P|E|PF\n"));
		ble_send_P(PSTR("S,cal=ZERO(AC off) CALV,volts CALI,amps CALCLR\n"));
		ble_send_P(PSTR("S,fun=DOOM (then w a s d q e f, EXIT)\n"));
	}
	else if ((arg = match_cmd(line, "STATUS"))) {
		send_status();
	}
	else if ((arg = match_cmd(line, "RESETMM"))) {     /* before RESET: longer name */
		mon.have_mm = 0;
		ok(PSTR("RESETMM"));
	}
	else if ((arg = match_cmd(line, "RESET"))) {
		mon.energy_Wh = 0;
		mon.t0_ticks = ticks();
		ok(PSTR("RESET"));
	}
	else if ((arg = match_cmd(line, "RATE"))) {
		if (parse_milli(arg, &v) && v % 1000 == 0 && v / 1000 >= 250 && v / 1000 <= 10000) {
			settings.rate_ms = (uint16_t)(v / 1000);
			settings_save();
			ok(PSTR("RATE"));
		} else err(PSTR("RATE"), PSTR("use 250-10000 ms"));
	}
	else if ((arg = match_cmd(line, "AXIS"))) {
		if (parse_milli(arg, &v) && v >= 1000 && v <= 100000) {
			settings.axis_max_mW = v;
			settings_save();
			mon.axis_dirty = 1;
			ok(PSTR("AXIS"));
		} else err(PSTR("AXIS"), PSTR("use 1-100 W"));
	}
	else if ((arg = match_cmd(line, "DISP"))) {
		if      (!strcmp(arg, "AUTO")) mon.disp_mode = DISP_AUTO;
		else if (!strcmp(arg, "V"))    mon.disp_mode = DISP_V;
		else if (!strcmp(arg, "I"))    mon.disp_mode = DISP_I;
		else if (!strcmp(arg, "P"))    mon.disp_mode = DISP_P;
		else if (!strcmp(arg, "E"))    mon.disp_mode = DISP_E;
		else if (!strcmp(arg, "PF"))   mon.disp_mode = DISP_PF;
		else { err(PSTR("DISP"), PSTR("use AUTO V I P E PF")); return; }
		ok(PSTR("DISP"));
	}
	else if ((arg = match_cmd(line, "ZERO"))) {
		if (!mon.have_last) { err(PSTR("ZERO"), PSTR("no reading yet")); return; }
		/* refuse if there is clearly a signal: raw voltage above 1 V */
		float raw_v = sqrtf(mon.last.var_v) * (ADC_VREF / 1024.0f) / V_RATIO;
		if (raw_v > 1.0f) { err(PSTR("ZERO"), PSTR("turn the AC source off first")); return; }
		settings.cal.noise_vv = mon.last.var_v;
		settings.cal.noise_ii = mon.last.var_i;
		settings_save();
		ok(PSTR("ZERO"));
	}
	else if ((arg = match_cmd(line, "CALV"))) {
		if (!parse_milli(arg, &v) || v < 1000) { err(PSTR("CALV"), PSTR("use CALV,<true volts>")); return; }
		if (!mon.have_last || mon.last.vrms < 1.0f) { err(PSTR("CALV"), PSTR("no AC voltage")); return; }
		float k = settings.cal.cal_v * (v / 1000.0f) / mon.last.vrms;
		if (k < 0.5f || k > 2.0f) { err(PSTR("CALV"), PSTR("correction over 2x, check wiring")); return; }
		settings.cal.cal_v = k;
		settings_save();
		ok(PSTR("CALV"));
	}
	else if ((arg = match_cmd(line, "CALI"))) {
		if (!parse_milli(arg, &v) || v < 10) { err(PSTR("CALI"), PSTR("use CALI,<true amps>")); return; }
		if (!mon.have_last || mon.last.irms < 0.02f) { err(PSTR("CALI"), PSTR("no load current")); return; }
		float k = settings.cal.cal_i * (v / 1000.0f) / mon.last.irms;
		if (k < 0.5f || k > 2.0f) { err(PSTR("CALI"), PSTR("correction over 2x, check wiring")); return; }
		settings.cal.cal_i = k;
		settings_save();
		ok(PSTR("CALI"));
	}
	else if ((arg = match_cmd(line, "CALCLR"))) {
		settings_defaults();
		settings_save();
		mon.axis_dirty = 1;
		ok(PSTR("CALCLR"));
	}
	else if ((arg = match_cmd(line, "DOOM"))) {
#if ENABLE_DOOM
		mon.doom_request = 1;                 /* main.c runs the game; it replies OK,DOOM */
#else
		err(PSTR("DOOM"), PSTR("disabled"));
#endif
	}
	else if ((arg = match_cmd(line, "EXIT"))) {
		err(PSTR("EXIT"), PSTR("not in DOOM mode"));
	}
	else if (line[0] != '\0') {
		err(PSTR("UNKNOWN"), PSTR("send HELP"));
	}
}
