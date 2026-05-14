#ifndef __BME680_TYPES_H__
#define __BME680_TYPES_H__

#include "stdbool.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {              
    int16_t temperature;      
    uint32_t pressure;        
    uint32_t humidity;        
    uint32_t gas_resistance;  
} bme680_values_fixed_t;


typedef struct {           
    float temperature;     
    float pressure;        
    float humidity;        
    float gas_resistance;  
} bme680_values_float_t;


typedef enum {
    osr_none = 0,  
    osr_1x = 1,    
    osr_2x = 2,
    osr_4x = 3,
    osr_8x = 4,
    osr_16x = 5
} bme680_oversampling_rate_t;


typedef enum {
    iir_size_0 = 0,  
    iir_size_1 = 1,
    iir_size_3 = 2,
    iir_size_7 = 3,
    iir_size_15 = 4,
    iir_size_31 = 5,
    iir_size_63 = 6,
    iir_size_127 = 7
} bme680_filter_size_t;


typedef struct {
    uint8_t osr_temperature;  
    uint8_t osr_pressure;     
    uint8_t osr_humidity;     
    uint8_t filter_size;      

    int8_t heater_profile;            
    uint16_t heater_temperature[10];  
    uint16_t heater_duration[10];     

    int8_t ambient_temperature;  
} bme680_settings_t;


typedef struct {
    uint16_t par_t1;  
    int16_t par_t2;
    int8_t par_t3;

    uint16_t par_p1;  
    int16_t par_p2;
    int8_t par_p3;
    int16_t par_p4;
    int16_t par_p5;
    int8_t par_p7;
    int8_t par_p6;
    int16_t par_p8;
    int16_t par_p9;
    uint8_t par_p10;

    uint16_t par_h1;  
    uint16_t par_h2;
    int8_t par_h3;
    int8_t par_h4;
    int8_t par_h5;
    uint8_t par_h6;
    int8_t par_h7;

    int8_t par_gh1;  
    int16_t par_gh2;
    int8_t par_gh3;

    int32_t t_fine;  
    uint8_t res_heat_range;
    int8_t res_heat_val;
    int8_t range_sw_err;
} bme680_calib_data_t;


typedef struct {
    int error_code;  

    uint8_t bus;   
    uint8_t addr;  
    uint8_t cs;    
                   

    bool meas_started;    
    uint8_t meas_status;  

    bme680_settings_t settings;      
    bme680_calib_data_t calib_data;  

} bme680_sensor_t;

#ifdef __cplusplus
}
#endif

#endif
