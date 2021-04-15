/*
 * Written by Kenny
 *
 */
float r2temperature(float ohm);
float adc_2_ohm(int adc_value);

// +5V
// ---+
//    #
//    # 10k constant resistor
//    |---------> Uadc
//    #
//    # ntc 1k
// ---+
//

/* All calibrations should be done here if needed. Normally you will
* only compensate the variance in the value of the 10k resistor and
* the 1k NTC. The 78L05 has also influence. This all can be adjusted
* changing NTC_RN a bit.
*/


/* Constant pullup resistor value */
#define NTC_PULLUP_R 9850.0	//calibrated 10k

/*
NTC temperature constant "B"-value: 3977K +/- 1%
usually between 2000k - 5000k, I don't know the type i'm using, trial and error...
- see PHP test script in comments under
*/
#define NTC_B 3177
/* NTC datasheet is 1 kOhm at 25 degree celsius */
#define NTC_TN 25
/* 1 kOhm +/- 10%, change for calibration: */
#define NTC_RN 950


/*
PHP script for B value trial and error on a linux box

<?

$NTC_B=3177;
$NTC_RN=1000;
$NTC_TN=25;
$ohm = $argv[1]; //1000ohm @25, 653ohm @~37.5 (under my arm....)

$tmp = ( ( 1/$NTC_B ) * log( ($ohm/$NTC_RN) ) ) + ( 1 / ($NTC_TN+273) );

$c = (1 / $tmp) - 273;
print "\n$c C\n";
?>

*/