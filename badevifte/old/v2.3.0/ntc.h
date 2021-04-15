/*
 * Written by Kenny
 *
 */
int r2temperature(float ohm);
float adc_2_ohm(int adc_value);

/* Note on calibration: many household thermometers are much worse than
* a not calibrated version of this thermometer. You many do more damage than
* good if you change too much here
*/

/* all calibrations should be done here if needed. Normally you will
* only compensate the variance in the value of the 10k resistor and
* the 1k NTC. The 78L05 has also influence. This all can be adjusted
* changing NTC_RN a bit.
*/

/* NTC temperature constant "B"-value: 3977K +/- 1% */
// usually between 2000k - 5000k, I don't know the type i'm using, trial and error...
#define NTC_B 3977
/* 1 kOhm at 25 degree celsius */
#define NTC_TN 25
/* 1 kOhm +/- 10%, change for calibration: */
#define NTC_RN 1000
