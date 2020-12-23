/**
* 
*/
#separate
void readTempFreezer(void)
{    
    signed int8 tmp;
                
    EnableSensors;
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
    delay_ms(10);               
    adc_val = getAvgADC(ADC_FREEZER, 100);
                 
        
    SampleFreezer.samples[SampleFreezer.index] = SensorADToDegrees(adc_val, NO);
    //printf(STREAM_DEBUG, "FRYS sample #%u:%d\r\n", SampleFreezer.index, SampleFreezer.samples[SampleFreezer.index]);         
    SampleFreezer.index++;
    
    if(SampleFreezer.index == SAMPLE_COUNT)  
    {

        #ifdef DEBUG
        printf(STREAM_DEBUG, "** FRYS START\r\n");     
        #endif
        
        max_val=-127;
        min_val=127;
        for(x=0; x<SampleFreezer.index; x++)
        {
            tmp = (signed int8)SampleFreezer.samples[x];
            if(min_val >= tmp) min_val = tmp;
            if(max_val <= tmp) max_val = tmp;
        }    
        
        if(Temps.freezer_real != NOT_READY && min_val < max_val)
            if(max_val - min_val >=3 && SampleFreezer.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFreezer.index = 0;            
                SampleFreezer.tries++;
                //printf(STREAM_DEBUG, "Big diff (%d-%d), skip\r\n", max_val, min_val);     
            }
        
        if(SampleFreezer.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.freezer_real = 0;

            for(x=0; x<SampleFreezer.index; x++)
                Temps.freezer_real += SampleFreezer.samples[x];                

            Temps.freezer_real /= (signed int8)(SAMPLE_COUNT);

            SampleFreezer.index = 0;
            SampleFreezer.tries = 0;

        
            if(SampleFreezer.do_final_sample)
            {                
                //printf(STREAM_DEBUG, "Added l_sample #%u:%d\r\n", SampleFreezer.last_index, Temps.freezer_real);             
                SampleFreezer.last_samples[SampleFreezer.last_index] = Temps.freezer_real;
                SampleFreezer.last_index++;
                SampleFreezer.do_final_sample = 0;
            }    
            
            
            // we need SAMPLE_INTERVAL_COUNT equal samples with 10 secs between, before we approve it and set do any actions
            if(SampleFreezer.last_index==SAMPLE_INTERVAL_COUNT)
            {
                max_val=-127;
                min_val=127;
                for(x=0; x<SampleFreezer.last_index; x++)
                {                                
                    tmp = (signed int8)SampleFreezer.last_samples[x];
                    if(min_val >= tmp) min_val = tmp;
                    if(max_val <= tmp) max_val = tmp;                    
                }    
                
                if(min_val == max_val || SampleFreezer.last_tries > 2) 
                {
                    SampleFreezer.do_actions = 1; // OKAY to to any actions, temp seems stable
                    SampleFreezer.last_tries = 0;                    
                }    
                else
                {
                                
                    SampleFreezer.last_tries++;

                    //printf(STREAM_DEBUG, "DIFF last_samples: %d-%d\r\n", min_val, max_val); 
                }
                SampleFreezer.last_index = 0;    
            }
        
        }
    }
                 
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    delay_us(50); // Needed for tris to apply    
    DisableSensors;
    
    
    
}


#separate
void readTempFridge(void)
{
    signed int8 tmp;
                
    
    set_tris_a(TRISA_SAMPLE_SENSORS); 
    set_tris_b(TRISB_SAMPLE_SENSORS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
    EnableSensors;
    output_low(POT_FRIDGE_PULLDOWN); // and pull down voltage divider resistors
    delay_ms(10); // Let LM335A stabilize   
    adc_val = getAvgADC(ADC_FRIDGE, 100);


    SampleFridge.samples[SampleFridge.index] = SensorADToDegrees(adc_val, YES);
    //printf(STREAM_DEBUG, "KJØL sample #%u:%d\r\n", SampleFridge.index, SampleFridge.samples[SampleFridge.index]);         
    SampleFridge.index++;
    
    
    if(SampleFridge.index == SAMPLE_COUNT)  
    {
        printf(STREAM_DEBUG, "** KJØL START\r\n");     

        max_val=-127;
        min_val=127;
        for(x=0; x<SampleFridge.index; x++)
        {
            tmp = (signed int8)SampleFridge.samples[x];
            if(min_val >= tmp) min_val = tmp;
            if(max_val <= tmp) max_val = tmp;            
        }    
        
        if(Temps.fridge_real != NOT_READY && min_val < max_val)
            if(max_val - min_val >=3 && SampleFridge.tries < 3) // too big diff, but not too many tries, reject samples!
            {
                SampleFridge.index = 0;            
                SampleFridge.tries++;
                //printf(STREAM_DEBUG, "big diff (%d-%d), skip\r\n", max_val, min_val);         
            }
        
        if(SampleFridge.index == SAMPLE_COUNT) // We got a complete sample set, calc avg                        
        {                
            Temps.fridge_real = 0;
                        
            for(x=0; x<SampleFridge.index; x++)
                Temps.fridge_real += SampleFridge.samples[x];                
                
            Temps.fridge_real /= (signed int8)(SAMPLE_COUNT);           

            SampleFridge.index = 0;
            SampleFridge.tries = 0;
            
            if(SampleFridge.do_final_sample)
            {                
                //printf(STREAM_DEBUG, "Added l_sample #%u:%d\r\n", SampleFridge.last_index, Temps.fridge);             
                SampleFridge.last_samples[SampleFridge.last_index] = Temps.fridge_real;
                SampleFridge.last_index++;
                SampleFridge.do_final_sample = 0;
            }    
            
            
            // we need SAMPLE_INTERVAL_COUNT equal samples with 10 secs between, before we approve it and set do any actions
            if(SampleFridge.last_index==SAMPLE_INTERVAL_COUNT)
            {
                max_val=-127;
                min_val=127;
                for(x=0; x<SampleFridge.last_index; x++)
                {                                
                    tmp = (signed int8)SampleFridge.last_samples[x];
                    if(min_val >= tmp) min_val = tmp;
                    if(max_val <= tmp) max_val = tmp;                    
                }    
                
                if(min_val == max_val || SampleFridge.last_tries > 2) 
                {
                    SampleFridge.do_actions = 1; // OKAY to to any actions, temp seems stable
                    SampleFridge.last_tries = 0;                    
                }    
                else
                {                    
                    SampleFridge.last_tries++;

                    //printf(STREAM_DEBUG, "DIFF last_samples: %d-%d\r\n", min_val, max_val); 
                }
                SampleFridge.last_index = 0;    
            }
                
            
        }
    }

    #ifdef DEBUG
    //printf(STREAM_DEBUG, "Fridge REAL Temp, Stop/Run-mins: %d, %u/%Lu\r\n", Temps.fridge, fridge_stopped_mins, fridge_run_mins);             
    //printf(STREAM_DEBUG, "** KJØL STOPP\r\n");             
    #endif

    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    delay_us(50); // Needed for tris to apply    
    DisableSensors;
    
}    


/**
* Read the pot's value, this is used for alarm and turning on/off a motor at given temp
*  
*/
#separate
void readPotFreezer(void)
{
    // tris also set pulldown's to output
    set_tris_a(TRISA_SAMPLE_POTS); 
    set_tris_b(TRISB_SAMPLE_POTS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
            
    EnableSensors; // we need to power the pots via transistor (and R6 1K) to read them, just like the sensors...
    
    output_low(POT_FREEZER_PULLDOWN); // and pull down voltage divider resistors
    delay_ms(10);  // Let LM335A stabilize
    
    /*********** v1.9: Average calc **************/    
    SampleFreezerPot.samples[SampleFreezerPot.index] = getAvgADC(ADC_POT_FREEZER, 100);
    output_float(POT_FREEZER_PULLDOWN);
    printf(STREAM_DEBUG, "FRYS POT sample #%u:%Lu\r\n", SampleFreezerPot.index, SampleFreezerPot.samples[SampleFreezerPot.index]);         
    SampleFreezerPot.index++;
    
    if(SampleFreezerPot.index == SAMPLE_COUNT_POT) // We got a complete sample set, calc avg                        
    {                
        adc_val = 0;
                    
        for(x=0; x<SampleFreezerPot.index; x++)
            adc_val += SampleFreezerPot.samples[x];                
            
        adc_val /= (unsigned int16)SAMPLE_COUNT_POT;           

        SampleFreezerPot.index = 0;        
    
        if(!Status.freezer_running)
            adc_val += 25;
        else
            adc_val += 25; // TODO: fjern dette, ADC faller ikke likevel når motor starter....
        
        SampleFreezerPot.avg_adc_value = adc_val;
            
        //pot_freezer = FridgeLookup[1/ adc_val_freezer_pot / 8];
        
        if(adc_val > 770) Temps.pot_freezer = POT_OFF;
        else if(adc_val > 500) Temps.pot_freezer = -18;
        else if(adc_val > 650) Temps.pot_freezer = -19;
        else if(adc_val > 540) Temps.pot_freezer = -20;
        else if(adc_val > 470) Temps.pot_freezer = -21;
        else if(adc_val > 400) Temps.pot_freezer = -22; // 400
        else if(adc_val > 340) Temps.pot_freezer = -23;    //340
        else if(adc_val > 315) Temps.pot_freezer = -24;
        else Temps.pot_freezer = -25;
            
        if(adc_val <200) Temps.pot_freezer = POT_OFF;        
    
        // not allow temp go under 1C, if so ignore FRIDGE_DEGREES_UNDER 
        if((Temps.pot_freezer - FREEZER_DEGREES_UNDER) < 1)
        {
            //config.fridge_degrees_under = 0;
            switch(Temps.pot_freezer)
            {                
                case -19: config.freezer_degrees_under = 1; break;
                case -20: config.freezer_degrees_under = 2; break;
                case -21: config.freezer_degrees_under = 3; break;
                default: config.freezer_degrees_under = 0; 
            }
        }
        
        #ifdef DEBUG       
        //printf(STREAM_DEBUG, "POT Freezer %d/%Lu\r\n", Temps.pot_freezer, adc_val);    
        #endif
        
    }    
    
        
                                                            
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL);     
    delay_us(50); // Needed for tris to apply    
    DisableSensors;
}


#separate
void readPotFridge(void)
{
    // tris also set pulldown's to output
    set_tris_a(TRISA_SAMPLE_POTS); 
    set_tris_b(TRISB_SAMPLE_POTS); 
    delay_us(50); // Needed for tris to apply.... 2 hours troubleshooting...
        
    EnableSensors; // we need to power the pots via transistor (and R6 1K) to read them, just like the sensors...
    
    output_low(POT_FRIDGE_PULLDOWN); // and pull down voltage divider resistors
    delay_ms(1);

    /*********** v1.9: Average calc **************/
    SampleFridgePot.samples[SampleFridgePot.index] = getAvgADC(ADC_POT_FRIDGE, 100);
    output_float(POT_FRIDGE_PULLDOWN);
    delay_ms(1);
    printf(STREAM_DEBUG, "KJØL POT sample #%u:%Lu\r\n", SampleFridgePot.index, SampleFridgePot.samples[SampleFridgePot.index]);         
    SampleFridgePot.index++;
            
    if(SampleFridgePot.index == SAMPLE_COUNT_POT) // We got a complete sample set, calc avg                        
    {                
        adc_val = 0;
                    
        for(x=0; x<SampleFridgePot.index; x++)
            adc_val += SampleFridgePot.samples[x];                
            
        adc_val /= (unsigned int16)SAMPLE_COUNT_POT;           

        SampleFridgePot.index = 0;
        
        SampleFridgePot.avg_adc_value = adc_val;

    
        if(adc_val > 700)      Temps.pot_fridge = POT_OFF;
        else if(adc_val > 600) Temps.pot_fridge = 6;
        else if(adc_val > 505) Temps.pot_fridge = 5;
        else if(adc_val > 390) Temps.pot_fridge = 4;
        else if(adc_val > 350) Temps.pot_fridge = 3;        
        else if(adc_val > 315) Temps.pot_fridge = 2;
        else
            Temps.pot_fridge = 1;
    
        if(adc_val < 200) Temps.pot_fridge = POT_OFF; // POT er litt knirkete...
        
        // not allow temp go under 1C, if so ignore FRIDGE_DEGREES_UNDER 
        if(Temps.pot_fridge != POT_OFF && (Temps.pot_fridge - FRIDGE_DEGREES_UNDER) < 1)
        {            
            switch(Temps.pot_fridge)
            {
                case 1: config.fridge_degrees_under = 0; break;   
                case 2: config.fridge_degrees_under = 1; break;
                case 3: config.fridge_degrees_under = 2; break;
                default: config.fridge_degrees_under = 0; 
            }
        }        
    }
    
                                                            
    // tris also set pulldown's to input, so we don't need to set them float.
    set_tris_a(TRISA_NORMAL);
    set_tris_b(TRISB_NORMAL); 
    delay_us(50); // Needed for tris to apply            
    DisableSensors;
    
}
